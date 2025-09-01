#include <libp2p/common/asio_buffer.hpp>
#include <libp2p/common/sample_peer.hpp>
#include <libp2p/coro/asio.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/bytestr.hpp>

// Example: Gossip chat over libp2p
// How to run (in separate terminals):
//  1) Peer A listens on index 1 and connects to 2
//     ./gossip_chat_example 1 2
//  2) Peer B listens on index 2 and connects to 1
//     ./gossip_chat_example 2 1
// Type a line and press Enter to broadcast it to subscribers of topic
// "example". Each process logs received messages; your own sent messages are
// not echoed back by default.

// Wrap stdin as an async input source that yields lines.
struct Input {
  // Use explicit to prevent accidental implicit conversions.
  explicit Input(boost::asio::io_context &io_context)
      : fd_{io_context, STDIN_FILENO} {}

  // Await a single line from stdin (without trailing newline). Returns nullopt
  // on EOF.
  libp2p::Coro<std::optional<std::string>> read() {
    // Read until a '\n' is seen; co_await an outcome<...>
    auto read = libp2p::coroOutcome(co_await boost::asio::async_read_until(
        fd_, buf_, "\n", libp2p::useCoroOutcome));
    if (not read.has_value()) {
      co_return std::nullopt;
    }
    // Convert buffer to string, trim trailing newline, move to std::string
    auto buf = qtils::byte2str(libp2p::asioBuffer(buf_.data()));
    auto i = buf.find('\n');
    if (i != std::string_view::npos) {
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
  // Initialize simple logging and a logger for this example.
  libp2p::simpleLoggingSystem();
  auto log = libp2p::log::createLogger("chat");

  // Arguments: index to listen on, followed by indices to connect to.
  if (argc < 2) {
    std::println(
        "usage: gossip_chat_example [index to listen] [indices to connect]...");
    return EXIT_FAILURE;
  }
  auto index = std::stoul(argv[1]);

  // SamplePeer maps a small integer index to deterministic keypair and address.
  auto sample_peer = libp2p::SamplePeer::makeEd25519(index);
  std::vector<libp2p::SamplePeer> connect;
  for (auto &arg : std::span{argv + 2, static_cast<size_t>(argc) - 2}) {
    connect.emplace_back(libp2p::SamplePeer::makeEd25519(std::stoul(arg)));
  }

  // Construct a host with QUIC transport and our chosen identity.
  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useKeyPair(sample_peer.keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());
  auto io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();
  auto gossip =
      injector.create<std::shared_ptr<libp2p::protocol::gossip::Gossip>>();

  // Start listening and the gossip protocol.
  host->listen(sample_peer.listen).value();
  gossip->start();
  host->start();

  // Attempt to connect to listed peers in the background.
  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    for (auto &peer : connect) {
      log->info("connect to {}", peer.index);
      auto r = co_await host->connect(peer.connect_info);
      if (not r.has_value()) {
        log->warn("can't connect to {}", peer.index);
      }
    }
  });

  // Subscribe to a simple topic named "example".
  auto topic = gossip->subscribe("example");

  // Receiver task: prints all messages arriving on the topic.
  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    while (true) {
      auto msg_result = co_await topic->receive();
      if (not msg_result.has_value()) {
        break;  // channel closed
      }
      auto msg = msg_result.value();
      log->info("{}", qtils::byte2str(msg));
    }
  });

  // Sender task: reads stdin lines and publishes them to the topic.
  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    Input input{*io_context};
    while (true) {
      auto msg = co_await input.read();
      if (not msg.has_value()) {
        break;  // EOF on stdin
      }
      if (msg->empty()) {
        continue;
      }
      topic->publish(qtils::str2byte(std::format("{}: {}", index, *msg)));
    }
    io_context->stop();
  });

  // Run the event loop until stopped (e.g., EOF on stdin).
  io_context->run();
  return EXIT_SUCCESS;
}
