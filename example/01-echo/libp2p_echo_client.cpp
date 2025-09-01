#include <libp2p/basic/read.hpp>
#include <libp2p/basic/write.hpp>
#include <libp2p/common/sample_peer.hpp>
#include <libp2p/connection/stream.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/protocol/echo/echo.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/bytestr.hpp>

int main(int argc, char **argv) {
  libp2p::simpleLoggingSystem();
  auto log = libp2p::log::createLogger("EchoClient");

  auto connect_info = libp2p::SamplePeer::makeEd25519(0).connect_info;
  if (argc >= 2) {
    auto address = libp2p::Multiaddress::create(argv[1]).value();
    auto peer_id =
        libp2p::PeerId::fromBase58(address.getPeerId().value()).value();
    connect_info = {peer_id, {address}};
  }
  const std::string message = "Hello from C++";
  auto sample_peer = libp2p::SamplePeer::makeEd25519(1);

  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useKeyPair(sample_peer.keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());
  auto io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();
  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();
  auto echo = injector.create<std::shared_ptr<libp2p::protocol::Echo>>();
  host->start();

  libp2p::coroSpawn(*io_context, [&]() -> libp2p::Coro<void> {
    log->info("Connect to {}", connect_info.addresses.at(0));
    auto stream =
        (co_await host->newStream(connect_info, echo->getProtocolIds()))
            .value()
            .stream;

    log->info("SENDING {}", message);
    (co_await libp2p::write(stream, qtils::str2byte(message))).value();

    std::string response;
    response.resize(message.size());
    (co_await libp2p::read(stream, qtils::str2byte(std::span{response})))
        .value();
    log->info("RESPONSE {}", response);

    io_context->stop();
  });

  log->info("Client started");
  io_context->run();
  log->info("Client stopped");
  return EXIT_SUCCESS;
}