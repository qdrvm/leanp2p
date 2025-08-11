// LibP2P Echo Server Example
// This example demonstrates how to create a basic echo server using libp2p
// that listens for incoming connections and echoes back any data it receives

#include <lsquic.h>

#include <iostream>
#include <libp2p/injector/host_injector.hpp>

#include <libp2p/common/sample_peer.hpp>
#include <libp2p/log/configurator.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/muxer/muxed_connection_config.hpp>
#include <libp2p/protocol/echo/echo.hpp>
#include <libp2p/transport/quic/transport.hpp>

namespace {
  // YAML configuration for the logging system
  // Sets up console logging with info level for the main group and libp2p
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
  // Initialize the logging system with both libp2p's default config
  // and our custom application-specific logging configuration
  auto logging_system = std::make_shared<soralog::LoggingSystem>(
      std::make_shared<soralog::ConfiguratorFromYAML>(
          // Original LibP2P logging config
          std::make_shared<libp2p::log::Configurator>(),
          // Additional logging config for application
          logger_config));

  // Configure the logging system and handle any errors
  auto r = logging_system->configure();
  if (not r.message.empty()) {
    (r.has_error ? std::cerr : std::cout) << r.message << std::endl;
  }
  if (r.has_error) {
    exit(EXIT_FAILURE);
  }

  // Set the global logging system and create a logger for this application
  libp2p::log::setLoggingSystem(logging_system);
  auto log = libp2p::log::createLogger("EchoServer");

  libp2p::SamplePeer sample_peer{0};

  // Create the dependency injection container (injector) for the libp2p host
  // This configures the host to use our key pair and QUIC transport
  auto injector = libp2p::injector::makeHostInjector(
      libp2p::injector::useKeyPair(sample_peer.keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());

  // Create the I/O context that will handle all asynchronous operations
  auto io_context = injector.create<std::shared_ptr<boost::asio::io_context>>();

  // Create the libp2p host - this is the main entry point for libp2p
  // functionality
  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();

  // Create and register the echo protocol handler
  // This will handle incoming connections that use the echo protocol
  libp2p::protocol::Echo echo{io_context};
  if (not host->listenProtocol(
          echo.getProtocolId(),
          std::make_shared<libp2p::protocol::Echo>(echo))) {
    std::cerr << "Error listening protocol" << std::endl;
    return 1;
  }

  // Start listening on the specified multiaddress
  if (not host->listen(sample_peer.listen)) {
    std::println("Error listening on {}", sample_peer.listen);
    return 1;
  }

  // Start the host and begin accepting connections
  host->start();

  // Log server startup information including connection details
  log->info("Server started");
  log->info("Listening on: {}", sample_peer.listen);
  log->info("Peer id: {}", sample_peer.peer_id.toBase58());
  log->info("Connection string: {}", sample_peer.connect);

  // Set up a signal handler to gracefully stop the server on SIGINT or SIGTERM
  boost::asio::signal_set signals(*io_context, SIGINT, SIGTERM);
  signals.async_wait(
      [&](const boost::system::error_code &, int) { io_context->stop(); });

  // Run the I/O context event loop - this blocks until the server is stopped
  io_context->run();
  log->info("Server stopped");

  return 0;
}