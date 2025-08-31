/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <lsquic.h>

#include <boost/asio/ip/udp.hpp>
#include <boost/asio/steady_timer.hpp>
#include <deque>
#include <libp2p/coro/channel.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/coro/handler.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <memory>
#include <optional>
#include <qtils/bytes.hpp>

#include "libp2p/transport/transport_listener.hpp"

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace boost::asio::ssl {
  class context;
}  // namespace boost::asio::ssl

namespace libp2p::connection {
  struct QuicStream;
}  // namespace libp2p::connection

namespace libp2p::crypto::marshaller {
  class KeyMarshaller;
}  // namespace libp2p::crypto::marshaller

namespace libp2p::muxer {
  struct MuxedConnectionConfig;
}  // namespace libp2p::muxer

namespace libp2p::transport {
  struct QuicConnection;
}  // namespace libp2p::transport

namespace libp2p::transport::lsquic {
  class Engine;
  struct ConnCtx;
  struct StreamCtx;

  using ConnectionPtrOutcome = outcome::result<std::shared_ptr<QuicConnection>>;
  using ConnectionPtrCoroOutcome = CoroOutcome<std::shared_ptr<QuicConnection>>;
  /**
   * Connect operation arguments.
   */
  struct Connecting {
    boost::asio::ip::udp::endpoint remote;
    PeerId peer;
    CoroHandler<ConnectionPtrOutcome> cb;
  };
  /**
   * `lsquic_conn_ctx_t` for libp2p connection.
   */
  struct ConnCtx {
    Engine *engine;
    lsquic_conn_t *ls_conn;
    std::optional<Connecting> connecting{};
    std::optional<std::shared_ptr<connection::QuicStream>> new_stream{};
    std::weak_ptr<QuicConnection> conn{};
  };

  /**
   * `lsquic_stream_ctx_t` for libp2p stream.
   */
  struct StreamCtx {
    Engine *engine;
    lsquic_stream_t *ls_stream;
    std::weak_ptr<connection::QuicStream> stream{};
    /**
     * Stream read operation arguments.
     */
    std::optional<CoroHandler<void>> reading;
  };

  /**
   * libp2p wrapper and adapter for lsquic server/client socket.
   */
  class Engine : public std::enable_shared_from_this<Engine> {
   public:
    Engine(std::shared_ptr<boost::asio::io_context> io_context,
           std::shared_ptr<boost::asio::ssl::context> ssl_context,
           const muxer::MuxedConnectionConfig &mux_config,
           PeerId local_peer,
           std::shared_ptr<crypto::marshaller::KeyMarshaller> key_codec,
           boost::asio::ip::udp::socket &&socket,
           bool client);
    ~Engine();

    // clang-tidy cppcoreguidelines-special-member-functions
    Engine(const Engine &) = delete;
    void operator=(const Engine &) = delete;
    Engine(Engine &&) = delete;
    void operator=(Engine &&) = delete;

    auto &local() const {
      return local_;
    }
    void start();
    ConnectionPtrCoroOutcome connect(
        const boost::asio::ip::udp::endpoint &remote, const PeerId &peer);
    outcome::result<std::shared_ptr<connection::QuicStream>> newStream(
        ConnCtx *conn_ctx);
    ConnectionPtrCoroOutcome asyncAccept();
    void wantProcess();

   private:
    void process();
    void readLoop();
    void onConnection(outcome::result<std::shared_ptr<QuicConnection>> conn);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<boost::asio::ssl::context> ssl_context_;
    PeerId local_peer_;
    std::shared_ptr<crypto::marshaller::KeyMarshaller> key_codec_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::steady_timer timer_;
    boost::asio::ip::udp::endpoint socket_local_;
    Multiaddress local_;
    lsquic_engine_t *engine_ = nullptr;
    bool started_ = false;
    bool want_process_ = false;
    std::optional<Connecting> connecting_;
    struct Reading {
      static constexpr size_t kMaxUdpPacketSize = 64 << 10;
      qtils::ByteArr<kMaxUdpPacketSize> buf;
      boost::asio::ip::udp::endpoint remote;
    };
    Reading reading_;
    // Channel for signaling new connections
    CoroOutcomeChannel<std::shared_ptr<QuicConnection>> conn_signal_;
  };
}  // namespace libp2p::transport::lsquic
