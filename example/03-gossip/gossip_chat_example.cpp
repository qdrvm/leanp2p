#include <libp2p/common/asio_buffer.hpp>
#include <libp2p/common/sample_peer.hpp>
#include <libp2p/coro/asio.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/bytestr.hpp>

struct Input {
  Input(boost::asio::io_context &io_context) : fd_{io_context, STDIN_FILENO} {}

  libp2p::Coro<std::optional<std::string>> read() {
    auto read = libp2p::coroOutcome(co_await boost::asio::async_read_until(
        fd_, buf_, "\n", libp2p::useCoroOutcome));
    if (not read.has_value()) {
      co_return std::nullopt;
    }
    auto buf = qtils::byte2str(libp2p::asioBuffer(buf_.data()));
    auto i = buf.find("\n");
    if (i != buf.npos) {
      buf = buf.substr(0, i);
    }
    auto line = std::string{buf};
    buf_.consume(buf_.size());
    co_return line;
  }

  boost::asio::posix::stream_descriptor fd_;
  boost::asio::streambuf buf_;
};

int main(int argc, char **argv) {
  libp2p::simpleLoggingSystem();
  auto log = libp2p::log::createLogger("chat");

  if (argc < 2) {
    std::println(
        "usage: gossip_chat_example [index to listen] [indices to connect]...");
    return EXIT_FAILURE;
  }
  auto index = std::stoul(argv[1]);
  libp2p::SamplePeer sample_peer{index};
  std::vector<libp2p::SamplePeer> connect;
  for (auto &arg : std::span{argv + 2, static_cast<size_t>(argc) - 2}) {
    connect.emplace_back(libp2p::SamplePeer{std::stoul(arg)});
  }

  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useKeyPair(sample_peer.keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());
  auto io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();
  auto gossip =
      injector.create<std::shared_ptr<libp2p::protocol::gossip::Gossip>>();
  host->listen(sample_peer.listen).value();
  gossip->start();
  host->start();

  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    for (auto &peer : connect) {
      log->info("connect to {}", peer.index);
      auto r = co_await host->connect(peer.connect_info);
      if (not r.has_value()) {
        log->warn("can't connect to {}", peer.index);
      }
    }
  });

  auto topic = gossip->subscribe("example");

  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    while (true) {
      auto msg_result = co_await topic->receive();
      if (not msg_result.has_value()) {
        break;
      }
      auto msg = msg_result.value();
      log->info("{}", qtils::byte2str(msg));
    }
  });

  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    Input input{*io_context};
    while (true) {
      auto msg = co_await input.read();
      if (not msg.has_value()) {
        break;
      }
      if (msg->empty()) {
        continue;
      }
      topic->publish(qtils::str2byte(std::format("{}: {}", index, *msg)));
    }
    io_context->stop();
  });

  io_context->run();
  return EXIT_SUCCESS;
}
