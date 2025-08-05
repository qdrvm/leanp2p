#include <iostream>
#include <libp2p/injector/host_injector.hpp>

#include <libp2p/common/literals.hpp>

#include <libp2p/log/configurator.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include "libp2p/muxer/muxed_connection_config.hpp"
#include "libp2p/protocol/echo/echo.hpp"

namespace {
  const std::string logger_config(R"(
# ----------------
sinks:
 - name: console
   type: console
   color: true
groups:
 - name: main
   sink: console
   level: info
   children:
     - name: libp2p
# ----------------
 )");
}  // namespace

int main(int argc, char *argv[]) {
  using libp2p::crypto::Key;
  using libp2p::crypto::KeyPair;
  using libp2p::crypto::PrivateKey;
  using libp2p::crypto::PublicKey;
  using libp2p::common::operator""_unhex;

  auto logging_system = std::make_shared<soralog::LoggingSystem>(
      std::make_shared<soralog::ConfiguratorFromYAML>(
          // Original LibP2P logging config
          std::make_shared<libp2p::log::Configurator>(),
          // Additional logging config for application
          logger_config));
  auto r = logging_system->configure();
  if (not r.message.empty()) {
    (r.has_error ? std::cerr : std::cout) << r.message << std::endl;
  }
  if (r.has_error) {
    exit(EXIT_FAILURE);
  }
  libp2p::log::setLoggingSystem(logging_system);
  auto log = libp2p::log::createLogger("EchoServer");
  KeyPair keypair{PublicKey{{Key::Type::Ed25519,
                             "48453469c62f4885373099421a7365520b5ffb"
                             "0d93726c124166be4b81d852e6"_unhex}},
                  PrivateKey{{Key::Type::Ed25519,
                              "4a9361c525840f7086b893d584ebbe475b4ec"
                              "7069951d2e897e8bceb0a3f35ce"_unhex}}};

  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useKeyPair(keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());
  auto io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();

  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();

  libp2p::protocol::Echo echo{
      io_context,
      libp2p::protocol::EchoConfig{
          .max_server_repeats =
              libp2p::protocol::EchoConfig::kInfiniteNumberOfRepeats,
          .max_recv_size =
              libp2p::muxer::MuxedConnectionConfig::kDefaultMaxWindowSize}};
  if (not host->listenProtocol(
          echo.getProtocolId(),
          std::make_shared<libp2p::protocol::Echo>(echo))) {
    std::cerr << "Error listening protocol" << std::endl;
    return 1;
  }
  std::string _ma = "/ip4/127.0.0.1/udp/40010/quic-v1";
  auto ma = libp2p::multi::Multiaddress::create(_ma).value();

  if (not host->listen(ma)) {
    std::cerr << "Error listening on " << _ma << std::endl;
    return 1;
  }

  host->start();
  log->info("Server started");
  log->info("Listening on: {}", ma.getStringAddress());
  log->info("Peer id: {}", host->getPeerInfo().id.toBase58());
  log->info("Connection string: {}/p2p/{}",
            ma.getStringAddress(),
            host->getPeerInfo().id.toBase58());

  return 0;
}