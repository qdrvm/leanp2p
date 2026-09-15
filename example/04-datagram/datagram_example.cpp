/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * Exercises unreliable QUIC datagrams (RFC 9221) end to end.
 *
 * Three hosts run in a single process:
 *   - "echo"  : datagrams enabled, sends every datagram back;
 *   - "client": datagrams enabled, drives the checks;
 *   - "plain" : datagrams disabled, used to check negotiation failure.
 *
 * Each host owns its own io_context, so all of them are polled from this
 * thread. Every check reports instead of throwing, and the process exits
 * non-zero on failure, which makes this usable as a smoke test.
 */

#include <boost/asio/steady_timer.hpp>
#include <chrono>
#include <thread>

#include <libp2p/common/sample_peer.hpp>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/coro/asio.hpp>
#include <libp2p/coro/handler.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/bytestr.hpp>

using namespace std::chrono_literals;

using libp2p::BytesIn;
using libp2p::Coro;
using libp2p::CoroOutcome;
using libp2p::DatagramError;
using libp2p::PollFuture;
using libp2p::connection::CapableConnection;

namespace {
  libp2p::log::Logger logger;
  size_t failures = 0;

  void check(bool ok, std::string_view what) {
    if (ok) {
      SL_INFO(logger, "  ok   : {}", what);
    } else {
      ++failures;
      SL_ERROR(logger, "  FAIL : {}", what);
    }
  }

  qtils::ByteVec bytes(std::string_view s) {
    return qtils::ByteVec{qtils::str2byte(s)};
  }

  /**
   * Drive a `PollFuture` to completion.
   */
  CoroOutcome<void> awaitPoll(std::shared_ptr<PollFuture> future) {
    using Handler = libp2p::CoroHandler<outcome::result<void>>;
    co_return co_await libp2p::coroHandler<outcome::result<void>>(
        [&](Handler &&handler) {
          auto shared = std::make_shared<Handler>(std::move(handler));
          // Polling with an empty waker only reads the current state.
          auto result = future->poll(
              [future, shared] { (*shared)(future->poll({}).value()); });
          if (result) {
            (*shared)(result.value());
          }
        });
  }

  /**
   * Wait until `predicate` holds, or give up after `timeout`.
   */
  Coro<bool> waitFor(boost::asio::io_context &io_context,
                     std::chrono::milliseconds timeout,
                     std::function<bool()> predicate) {
    boost::asio::steady_timer timer{io_context};
    auto deadline = std::chrono::steady_clock::now() + timeout;
    while (not predicate()) {
      if (std::chrono::steady_clock::now() >= deadline) {
        co_return false;
      }
      timer.expires_after(2ms);
      co_await timer.async_wait(boost::asio::use_awaitable);
    }
    co_return true;
  }

  Coro<void> sleepFor(boost::asio::io_context &io_context,
                      std::chrono::milliseconds delay) {
    boost::asio::steady_timer timer{io_context};
    timer.expires_after(delay);
    co_await timer.async_wait(boost::asio::use_awaitable);
  }

  struct Host {
    std::shared_ptr<void> injector_lifetime;
    std::shared_ptr<boost::asio::io_context> io_context;
    std::shared_ptr<libp2p::host::BasicHost> host;
  };

  /**
   * boost::di keeps singletons in storage that is static per injector *type*,
   * so two injectors built from the same bindings hand out the very same
   * host and io_context. Binding a distinct tag per host keeps the injector
   * types - and therefore the hosts - separate.
   */
  template <size_t Index>
  struct HostTag {};

  template <typename MakeInjector>
  Host makeHost(MakeInjector make) {
    auto injector = std::make_shared<decltype(make())>(make());
    return Host{
        injector,
        injector->template create<std::shared_ptr<boost::asio::io_context>>(),
        injector->template create<std::shared_ptr<libp2p::host::BasicHost>>(),
    };
  }

  template <size_t Index>
  Host datagramHost(const libp2p::SamplePeer &peer,
                    libp2p::OnDatagram on_datagram) {
    return makeHost([&] {
      return libp2p::injector::makeHostInjector(
          libp2p::injector::useKeyPair(peer.keypair),
          libp2p::injector::useTransportAdaptors<
              libp2p::transport::QuicTransport>(),
          libp2p::injector::enableDatagram(std::move(on_datagram)),
          boost::di::bind<HostTag<Index>>().to(HostTag<Index>{}));
    });
  }

  template <size_t Index>
  Host plainHost(const libp2p::SamplePeer &peer) {
    return makeHost([&] {
      return libp2p::injector::makeHostInjector(
          libp2p::injector::useKeyPair(peer.keypair),
          libp2p::injector::useTransportAdaptors<
              libp2p::transport::QuicTransport>(),
          boost::di::bind<HostTag<Index>>().to(HostTag<Index>{}));
    });
  }
}  // namespace

int main() {
  libp2p::simpleLoggingSystem();
  logger = libp2p::log::createLogger("DatagramExample");

  auto echo_peer = libp2p::SamplePeer::makeEd25519(40);
  auto client_peer = libp2p::SamplePeer::makeEd25519(41);
  auto plain_peer = libp2p::SamplePeer::makeEd25519(42);

  std::vector<qtils::ByteVec> echo_received;
  std::vector<qtils::ByteVec> client_received;

  // Echoes every datagram straight back, discarding the returned future.
  // This is the fire-and-forget path: the transport must still send it.
  auto echo_host = datagramHost<0>(
      echo_peer,
      [&](const std::shared_ptr<CapableConnection> &connection,
          qtils::ByteVec msg) {
        echo_received.push_back(msg);
        connection->sendDatagram(msg);
      });

  auto client_host = datagramHost<1>(
      client_peer,
      [&](const std::shared_ptr<CapableConnection> &, qtils::ByteVec msg) {
        client_received.push_back(std::move(msg));
      });

  auto plain_host = plainHost<2>(plain_peer);

  echo_host.host->listen(echo_peer.listen).value();
  echo_host.host->start();
  plain_host.host->listen(plain_peer.listen).value();
  plain_host.host->start();
  client_host.host->start();

  bool done = false;
  libp2p::coroSpawn(*client_host.io_context, [&]() -> Coro<void> {
    auto &io_context = *client_host.io_context;

    SL_INFO(logger, "connecting to echo host");
    auto connection =
        (co_await client_host.host->connect(echo_peer.connect_info)).value();

    // 1. negotiation
    check(connection->datagramsEnabled(), "datagrams negotiated");
    auto max_size = connection->maxDatagramSize();
    check(max_size > 0, std::format("max datagram size is {}", max_size));

    // 2. round trip, awaiting the send
    auto hello = bytes("hello datagram");
    auto sent = co_await awaitPoll(connection->sendDatagram(hello));
    check(sent.has_value(), "send completes");
    check(co_await waitFor(
              io_context, 2s, [&] { return not client_received.empty(); }),
          "echo received");
    if (not client_received.empty()) {
      check(client_received.front() == hello, "echo payload matches");
    }

    // 3. fire and forget: the returned future is dropped immediately
    auto forget = bytes("fire and forget");
    connection->sendDatagram(forget);
    check(co_await waitFor(
              io_context, 2s, [&] { return echo_received.size() >= 2; }),
          "discarded future is still sent");
    if (echo_received.size() >= 2) {
      check(echo_received.at(1) == forget, "discarded payload matches");
    }

    // 4. zero length datagram
    auto before_empty = echo_received.size();
    auto empty = co_await awaitPoll(connection->sendDatagram(BytesIn{}));
    check(empty.has_value(), "zero length send completes");
    check(co_await waitFor(
              io_context, 2s,
              [&] { return echo_received.size() > before_empty; }),
          "zero length datagram received");
    if (echo_received.size() > before_empty) {
      check(echo_received.back().empty(), "zero length payload is empty");
    }

    // 5. oversized payload is rejected, connection survives
    qtils::ByteVec oversized(max_size + 1, 0x42);
    auto too_big = co_await awaitPoll(connection->sendDatagram(oversized));
    check(too_big.has_error() and too_big.error() == DatagramError::TOO_LARGE,
          "oversized payload rejected with TOO_LARGE");
    check(connection->datagramsEnabled(), "connection survives oversized send");

    // 6. exactly at the limit still works
    qtils::ByteVec at_limit(max_size, 0x43);
    auto limit_sent = co_await awaitPoll(connection->sendDatagram(at_limit));
    check(limit_sent.has_value(), "payload at the limit is accepted");

    // 7. a send on an otherwise idle connection is flushed promptly
    co_await sleepFor(io_context, 300ms);
    auto start = std::chrono::steady_clock::now();
    auto idle = co_await awaitPoll(connection->sendDatagram(bytes("idle")));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    check(idle.has_value(), "idle connection send completes");
    check(elapsed < 1s,
          std::format("idle connection send is prompt ({}ms)",
                      elapsed.count()));

    // 8. peer without datagram support
    SL_INFO(logger, "connecting to host without datagram support");
    auto plain_connection =
        (co_await client_host.host->connect(plain_peer.connect_info)).value();
    check(not plain_connection->datagramsEnabled(),
          "datagrams not negotiated with peer that disabled them");
    auto unsupported =
        co_await awaitPoll(plain_connection->sendDatagram(bytes("nope")));
    check(unsupported.has_error()
              and unsupported.error() == DatagramError::NOT_NEGOTIATED,
          "send fails with NOT_NEGOTIATED instead of hanging");

    // 9. a datagram queued just before closing must still resolve, either
    // because it made it out or because the close failed it. If it were
    // abandoned instead, this coroutine would never finish.
    auto racing = connection->sendDatagram(bytes("racing the close"));
    std::ignore = connection->close();
    auto racing_result = co_await awaitPoll(racing);
    check(true,
          std::format("datagram queued before close resolves ({})",
                      racing_result.has_value()
                          ? "sent"
                          : racing_result.error().message()));

    // 10. once the connection has finished closing, sending reports an error
    check(co_await waitFor(io_context, 5s,
                           [&] { return connection->isClosed(); }),
          "connection reports closed");
    auto after_close =
        co_await awaitPoll(connection->sendDatagram(bytes("after close")));
    check(after_close.has_error()
              and after_close.error() == DatagramError::CONN_CLOSED,
          "send after close fails with CONN_CLOSED");

    done = true;
  });

  auto deadline = std::chrono::steady_clock::now() + 30s;
  while (not done and std::chrono::steady_clock::now() < deadline) {
    auto handlers = echo_host.io_context->poll()
                  + client_host.io_context->poll()
                  + plain_host.io_context->poll();
    if (handlers == 0) {
      std::this_thread::sleep_for(1ms);
    }
  }
  if (not done) {
    SL_ERROR(logger, "timed out before finishing the checks");
    ++failures;
  }

  SL_INFO(logger, "{}", failures == 0 ? "ALL CHECKS PASSED" : "THERE WERE FAILURES");
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
