/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/transport/quic/connection.hpp>
#include <libp2p/transport/quic/engine.hpp>
#include <libp2p/transport/quic/error.hpp>
#include <libp2p/transport/quic/stream.hpp>
#include <qtils/option_take.hpp>

namespace libp2p::transport {
  QuicConnection::QuicConnection(
      std::shared_ptr<boost::asio::io_context> io_context,
      lsquic::ConnCtx *conn_ctx,
      bool initiator,
      Multiaddress local,
      Multiaddress remote,
      PeerId local_peer,
      PeerId peer,
      crypto::PublicKey key)
      : io_context_{std::move(io_context)},
        conn_ctx_{conn_ctx},
        initiator_{initiator},
        local_{std::move(local)},
        remote_{std::move(remote)},
        local_peer_{std::move(local_peer)},
        peer_{std::move(peer)},
        key_{std::move(key)},
        stream_signal_{{io_context_->get_executor(), 1}} {}

  QuicConnection::~QuicConnection() {
    std::ignore = close();
  }

  CoroOutcome<size_t> QuicConnection::readSome(BytesOut) {
    throw std::logic_error{
        "QuicConnection::readSome (coroutine) must not be called"};
  }

  CoroOutcome<size_t> QuicConnection::writeSome(BytesIn) {
    throw std::logic_error{"QuicConnection::writeSome must not be called"};
  }

  bool QuicConnection::isClosed() const {
    return not conn_ctx_;
  }

  outcome::result<void> QuicConnection::close() {
    if (conn_ctx_) {
      lsquic_conn_close(conn_ctx_->ls_conn);
    }
    return outcome::success();
  }

  bool QuicConnection::isInitiator() const noexcept {
    return initiator_;
  }

  outcome::result<Multiaddress> QuicConnection::remoteMultiaddr() {
    return remote_;
  }

  outcome::result<Multiaddress> QuicConnection::localMultiaddr() {
    return local_;
  }

  outcome::result<PeerId> QuicConnection::localPeer() const {
    return local_peer_;
  }

  PeerId QuicConnection::remotePeer() const {
    return peer_;
  }

  outcome::result<crypto::PublicKey> QuicConnection::remotePublicKey() const {
    return key_;
  }

  void QuicConnection::start() {}

  void QuicConnection::stop() {}

  void QuicConnection::newStream(CapableConnection::StreamHandlerFunc cb) {
    cb(newStream());
  }

  CoroOutcome<std::shared_ptr<connection::Stream>>
  QuicConnection::newStreamCoroutine() {}

  outcome::result<std::shared_ptr<libp2p::connection::Stream>>
  QuicConnection::newStream() {
    if (not conn_ctx_) {
      return QuicError::CONN_CLOSED;
    }
    OUTCOME_TRY(stream, conn_ctx_->engine->newStream(conn_ctx_));
    return stream;
  }

  CoroOutcome<std::shared_ptr<connection::Stream>>
  QuicConnection::acceptStream() {
    co_return co_await stream_signal_.receive();
  }

  void QuicConnection::onStream(std::shared_ptr<connection::Stream> stream) {
    stream_signal_.send(stream);
  }

  void QuicConnection::onClose() {
    conn_ctx_ = nullptr;
    stream_signal_.send(QuicError::CONN_CLOSED);
  }
}  // namespace libp2p::transport