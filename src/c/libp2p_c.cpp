/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/c/libp2p_c.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>

#include <boost/asio/executor_work_guard.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>

#include <libp2p/basic/read.hpp>
#include <libp2p/basic/write.hpp>
#include <libp2p/common/sample_peer.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/injector/host_injector.hpp>
#include <libp2p/log/simple.hpp>
#include <libp2p/network/dialer.hpp>
#include <libp2p/transport/quic/transport.hpp>
#include <qtils/bytestr.hpp>
#include <soralog/logging_system.hpp>

// Include the base protocol header
#include <libp2p/protocol/base_protocol.hpp>

// Internal wrapper structures
struct libp2p_context {
  std::shared_ptr<boost::asio::io_context> io_context;
  std::unique_ptr<
      boost::asio::executor_work_guard<boost::asio::io_context::executor_type>>
      work_guard;
  bool running = false;
};

struct libp2p_host {
  std::shared_ptr<libp2p::host::BasicHost> host;
  libp2p_context_t *context;
  std::unordered_map<std::string, std::pair<libp2p_stream_handler_t, void *>>
      protocol_handlers;
  std::optional<libp2p::PeerId> peer_id;  // set after creation
};

struct libp2p_stream {
  std::shared_ptr<libp2p::connection::Stream> stream;
  libp2p_host_t *host;
};

struct libp2p_multiaddr {
  libp2p::Multiaddress addr;
  std::string addr_str;

  libp2p_multiaddr(libp2p::Multiaddress a, std::string s)
      : addr(std::move(a)), addr_str(std::move(s)) {}
};

struct libp2p_peer_id {
  libp2p::PeerId peer_id;
  std::string peer_id_str;

  libp2p_peer_id(libp2p::PeerId id, std::string s)
      : peer_id(std::move(id)), peer_id_str(std::move(s)) {}
};

// Generic protocol handler that bridges C++ protocol interface to C callbacks
class GenericProtocolHandler : public libp2p::protocol::BaseProtocol {
 public:
  GenericProtocolHandler(std::string protocol_id,
                         libp2p_stream_handler_t handler,
                         void *user_data,
                         libp2p_host_t *host_wrapper)
      : protocol_id_(std::move(protocol_id)),
        handler_(handler),
        user_data_(user_data),
        host_wrapper_(host_wrapper) {}

  [[nodiscard]] libp2p::StreamProtocols getProtocolIds() const override {
    return {protocol_id_};
  }

  // Override the handle method from BaseProtocol
  void handle(libp2p::StreamAndProtocol stream) override {
    assert(handler_ && "Handler cannot be null");
    // Create a stream wrapper for the C API
    auto stream_wrapper = new libp2p_stream_t();
    stream_wrapper->stream = stream.stream;
    stream_wrapper->host = host_wrapper_;

    // Call the C callback
    handler_(stream_wrapper, user_data_);
  }

 private:
  std::string protocol_id_;
  libp2p_stream_handler_t handler_;
  void *user_data_;
  libp2p_host_t *host_wrapper_;
};

// Helper function to convert C++ errors to C error codes
libp2p_error_t convert_error(const std::error_code &ec) {
  if (!ec) {
    return LIBP2P_SUCCESS;
  }
  // Map specific error codes as needed
  return LIBP2P_ERROR_UNKNOWN;
}

// Context management implementation
libp2p_context_t *libp2p_context_create() {
  try {
    auto ctx = new libp2p_context_t();
    ctx->io_context = std::make_shared<boost::asio::io_context>();
    ctx->work_guard = std::make_unique<boost::asio::executor_work_guard<
        boost::asio::io_context::executor_type>>(
        boost::asio::make_work_guard(*ctx->io_context));
    return ctx;
  } catch (...) {
    return nullptr;
  }
}

void libp2p_context_destroy(libp2p_context_t *ctx) {
  if (ctx) {
    if (ctx->running) {
      ctx->io_context->stop();
    }
    ctx->work_guard.reset();
    delete ctx;
  }
}

libp2p_error_t libp2p_context_run(libp2p_context_t *ctx) {
  if (!ctx) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    ctx->running = true;
    ctx->io_context->run();
    ctx->running = false;
    return LIBP2P_SUCCESS;
  } catch (...) {
    ctx->running = false;
    return LIBP2P_ERROR_UNKNOWN;
  }
}

void libp2p_context_stop(libp2p_context_t *ctx) {
  if (ctx) {
    ctx->work_guard.reset();  // allow run() to exit
    if (ctx->running) {
      ctx->io_context->stop();
    }
  }
}

// Host management implementation
libp2p_host_t *libp2p_host_create(libp2p_context_t *ctx,
                                  const char *keypair_seed) {
  if (!ctx) {
    return nullptr;
  }

  libp2p_host_t *host_wrapper = nullptr;
  // Ensure logging system initialized once
  static bool logging_inited = false;
  if (!logging_inited) {
    libp2p::simpleLoggingSystem();
    logging_inited = true;
  }
  host_wrapper = new libp2p_host_t();
  host_wrapper->context = ctx;

  // Derive deterministic index from seed
  uint32_t index = 0;
  if (keypair_seed) {
    index = static_cast<uint32_t>(std::hash<std::string>{}(keypair_seed));
  }
  auto sample_peer =
      libp2p::SamplePeer::makeEd25519(index % 1000);  // keep port reasonable

  auto injector = libp2p::injector::makeHostInjector(
      boost::di::bind<boost::asio::io_context>().to(ctx->io_context),
      libp2p::injector::useKeyPair(sample_peer.keypair),
      libp2p::injector::useTransportAdaptors<
          libp2p::transport::QuicTransport>());

  auto host = injector.create<std::shared_ptr<libp2p::host::BasicHost>>();
  host_wrapper->host = host;
  host_wrapper->peer_id = sample_peer.peer_id;

  return host_wrapper;
}

void libp2p_host_destroy(libp2p_host_t *host) {
  if (host) {
    delete host;
  }
}

libp2p_error_t libp2p_host_start(libp2p_host_t *host) {
  if (!host || !host->host) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    host->host->start();
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

libp2p_error_t libp2p_host_listen(libp2p_host_t *host,
                                  const char *multiaddr_str) {
  if (!host || !host->host || !multiaddr_str) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    auto addr_result = libp2p::Multiaddress::create(multiaddr_str);
    if (!addr_result.has_value()) {
      return LIBP2P_ERROR_INVALID_ARGUMENT;
    }

    if (!host->host->listen(addr_result.value())) {
      return LIBP2P_ERROR_CONNECTION_FAILED;
    }
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

libp2p_error_t libp2p_host_register_protocol(libp2p_host_t *host,
                                             const char *protocol_id,
                                             libp2p_stream_handler_t handler,
                                             void *user_data) {
  if (!host || !host->host || !protocol_id || !handler) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    // Store the C callback for later use
    host->protocol_handlers[protocol_id] = {handler, user_data};

    // Create a generic protocol handler that bridges C++ to C callbacks
    auto protocol_handler = std::make_shared<GenericProtocolHandler>(
        protocol_id, handler, user_data, host);

    // Register the protocol with the host - now passing the protocol handler
    // directly
    host->host->listenProtocol(protocol_handler);

    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

// Stream operations implementation
libp2p_error_t libp2p_stream_read(libp2p_stream_t *stream,
                                  uint8_t *buffer,
                                  size_t buffer_size,
                                  libp2p_read_callback_t callback,
                                  void *user_data) {
  if (!stream || !stream->stream || !buffer || !callback) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    libp2p::coroSpawn(
        *stream->host->context->io_context,
        [stream, buffer, buffer_size, callback, user_data]()
            -> libp2p::Coro<void> {
          std::span<uint8_t> buffer_span{buffer, buffer_size};
          auto n_res = co_await stream->stream->readSome(buffer_span);
          if (n_res.has_value()) {
            callback(stream, buffer, n_res.value(), LIBP2P_SUCCESS, user_data);
          } else {
            callback(
                stream, nullptr, 0, convert_error(n_res.error()), user_data);
          }
        });
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

libp2p_error_t libp2p_stream_write(libp2p_stream_t *stream,
                                   const uint8_t *data,
                                   size_t size,
                                   libp2p_write_callback_t callback,
                                   void *user_data) {
  if (!stream || !stream->stream || !data || !callback) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    libp2p::coroSpawn(
        *stream->host->context->io_context,
        [stream, data, size, callback, user_data]() -> libp2p::Coro<void> {
          std::span<const uint8_t> data_span{data, size};
          auto result = co_await libp2p::write(stream->stream, data_span);
          if (result.has_value()) {
            callback(stream, size, LIBP2P_SUCCESS, user_data);
          } else {
            callback(stream, 0, convert_error(result.error()), user_data);
          }
        });
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

libp2p_error_t libp2p_stream_close(libp2p_stream_t *stream) {
  if (!stream || !stream->stream) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    if (auto result = stream->stream->close(); !result) {
      return convert_error(result.error());
    }
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

bool libp2p_stream_is_closed(libp2p_stream_t *stream) {
  if (!stream || !stream->stream) {
    return true;
  }
  // Use a different method to check if stream is closed since isClosed()
  // doesn't exist
  return false;  // Simplified for now
}

// Connection management implementation
libp2p_error_t libp2p_host_dial(libp2p_host_t *host,
                                const char *multiaddr_str,
                                const char *peer_id_str,
                                libp2p_connection_handler_t callback,
                                void *user_data) {
  if (!host || !host->host || !multiaddr_str || !callback || !peer_id_str
      || std::strlen(peer_id_str) == 0) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    auto addr_result = libp2p::Multiaddress::create(multiaddr_str);
    if (!addr_result.has_value()) {
      return LIBP2P_ERROR_INVALID_ARGUMENT;
    }

    // Parse PeerId
    auto pid_res = libp2p::PeerId::fromBase58(peer_id_str);
    if (!pid_res.has_value()) {
      return LIBP2P_ERROR_INVALID_ARGUMENT;
    }

    // Build PeerInfo aggregate (PeerId is non-default-constructible)
    libp2p::peer::PeerInfo peer_info{pid_res.value(), {addr_result.value()}};

    // Try to connect asynchronously; on success, call callback
    libp2p::coroSpawn(*host->context->io_context,
                      [host,
                       peer_info = std::move(peer_info),
                       callback,
                       user_data]() mutable -> libp2p::Coro<void> {
                        std::ignore = co_await host->host->connect(peer_info);
                        // Always report the requested peer id
                        const std::string pid_str = peer_info.id.toBase58();
                        callback(host, pid_str.c_str(), user_data);
                      });
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

libp2p_error_t libp2p_host_new_stream(libp2p_host_t *host,
                                      const char *peer_id_str,
                                      const char *protocol_id,
                                      libp2p_stream_handler_t callback,
                                      void *user_data) {
  if (!host || !host->host || !peer_id_str || !protocol_id || !callback) {
    return LIBP2P_ERROR_INVALID_ARGUMENT;
  }

  try {
    auto peer_id_result = libp2p::PeerId::fromBase58(peer_id_str);
    if (!peer_id_result.has_value()) {
      return LIBP2P_ERROR_INVALID_ARGUMENT;
    }

    libp2p::coroSpawn(*host->context->io_context,
                      [host, peer_id_result, protocol_id, callback, user_data]()
                          -> libp2p::Coro<void> {
                        // Create a vector of protocols for the API
                        std::vector<std::string> protocols = {protocol_id};
                        auto stream_result = co_await host->host->newStream(
                            peer_id_result.value(), protocols);
                        if (stream_result.has_value()) {
                          auto stream_wrapper = new libp2p_stream_t();
                          stream_wrapper->stream = stream_result.value().stream;
                          stream_wrapper->host = host;
                          callback(stream_wrapper, user_data);
                        } else {
                          callback(nullptr, user_data);
                        }
                      });
    return LIBP2P_SUCCESS;
  } catch (...) {
    return LIBP2P_ERROR_UNKNOWN;
  }
}

// Utility functions implementation
libp2p_multiaddr_t *libp2p_multiaddr_create(const char *addr_str) {
  if (!addr_str) {
    return nullptr;
  }

  try {
    auto addr_result = libp2p::Multiaddress::create(addr_str);
    if (!addr_result.has_value()) {
      return nullptr;
    }

    return new libp2p_multiaddr_t(addr_result.value(), std::string(addr_str));
  } catch (...) {
    return nullptr;
  }
}

void libp2p_multiaddr_destroy(libp2p_multiaddr_t *addr) {
  delete addr;
}

const char *libp2p_multiaddr_to_string(libp2p_multiaddr_t *addr) {
  if (!addr) {
    return nullptr;
  }
  return addr->addr_str.c_str();
}

libp2p_peer_id_t *libp2p_peer_id_create(const char *base58_str) {
  if (!base58_str) {
    return nullptr;
  }

  try {
    auto peer_id_result = libp2p::PeerId::fromBase58(base58_str);
    if (!peer_id_result.has_value()) {
      return nullptr;
    }

    return new libp2p_peer_id_t(peer_id_result.value(),
                                std::string(base58_str));
  } catch (...) {
    return nullptr;
  }
}

void libp2p_peer_id_destroy(libp2p_peer_id_t *peer_id) {
  delete peer_id;
}

const char *libp2p_peer_id_to_string(libp2p_peer_id_t *peer_id) {
  if (!peer_id) {
    return nullptr;
  }
  return peer_id->peer_id_str.c_str();
}

// Error handling implementation
const char *libp2p_error_string(libp2p_error_t error) {
  switch (error) {
    case LIBP2P_SUCCESS:
      return "Success";
    case LIBP2P_ERROR_INVALID_ARGUMENT:
      return "Invalid argument";
    case LIBP2P_ERROR_OUT_OF_MEMORY:
      return "Out of memory";
    case LIBP2P_ERROR_CONNECTION_FAILED:
      return "Connection failed";
    case LIBP2P_ERROR_PROTOCOL_ERROR:
      return "Protocol error";
    case LIBP2P_ERROR_IO_ERROR:
      return "I/O error";
    case LIBP2P_ERROR_TIMEOUT:
      return "Timeout";
    case LIBP2P_ERROR_NOT_FOUND:
      return "Not found";
    case LIBP2P_ERROR_ALREADY_EXISTS:
      return "Already exists";
    case LIBP2P_ERROR_UNKNOWN:
      return "Unknown error";
    default:
      return "Invalid error code";
  }
}

// Simplified logging implementation without spdlog dependency
void libp2p_set_log_level(libp2p_log_level_t level) {
  // For now, just a simplified implementation
  // In a full implementation, you would integrate with the actual logging
  // system
  (void)level;  // Suppress unused parameter warning
}

libp2p_peer_id_t *libp2p_host_peer_id(libp2p_host_t *host) {
  if (!host || !host->host || !host->peer_id.has_value()) {
    return nullptr;
  }

  try {
    std::string id_str = host->peer_id->toBase58();
    return new libp2p_peer_id_t(host->peer_id.value(), std::move(id_str));
  } catch (...) {
    return nullptr;
  }
}

const char *libp2p_host_default_listen(libp2p_host_t *host) {
  if (!host || !host->host) {
    return nullptr;
  }
  static thread_local std::string addr;
  auto addrs = host->host->getAddressesInterfaces();
  if (addrs.empty()) {
    addrs = host->host->getAddresses();
  }
  if (addrs.empty()) {
    return nullptr;
  }
  addr = addrs.front().getStringAddress();
  return addr.c_str();
}
