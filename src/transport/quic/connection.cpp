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
        key_{std::move(key)} {}

  QuicConnection::~QuicConnection() {
    std::ignore = close();
  }

  boost::asio::awaitable<outcome::result<size_t>> QuicConnection::read(BytesOut,
                                                                       size_t) {
    throw std::logic_error{
        "QuicConnection::read (coroutine) must not be called"};
  }

  boost::asio::awaitable<outcome::result<size_t>> QuicConnection::readSome(
      BytesOut, size_t) {
    throw std::logic_error{
        "QuicConnection::readSome (coroutine) must not be called"};
  }

  boost::asio::awaitable<std::error_code> QuicConnection::writeSome(BytesIn,
                                                                    size_t) {
    throw std::logic_error{
        "QuicConnection::writeSome (coroutine) must not be called"};
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

  outcome::result<PeerId> QuicConnection::remotePeer() const {
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

  boost::asio::awaitable<outcome::result<std::shared_ptr<connection::Stream>>>
  QuicConnection::newStreamCoroutine() {}

  outcome::result<std::shared_ptr<libp2p::connection::Stream>>
  QuicConnection::newStream() {
    if (not conn_ctx_) {
      return QuicError::CONN_CLOSED;
    }
    OUTCOME_TRY(stream, conn_ctx_->engine->newStream(conn_ctx_));
    return stream;
  }

  connection::AsyncGenerator<
      outcome::result<std::shared_ptr<connection::Stream>>>
  QuicConnection::acceptStream() {
    while (true) {
      if (pending_streams_.empty()) {
        struct awaiter {
          QuicConnection *self;
          bool await_ready() noexcept {
            return !self->pending_streams_.empty();
          }
          void await_suspend(std::coroutine_handle<> h) noexcept {
            self->resume_accept_ = [h] { h.resume(); };
          }
          void await_resume() noexcept {}
        };
        co_await awaiter{this};
      }
      auto stream = std::move(pending_streams_.front());
      pending_streams_.pop_front();
      co_yield stream;
    }
  }

  void QuicConnection::onStream(std::shared_ptr<connection::Stream> stream) {
    pending_streams_.emplace_back(std::move(stream));
    if (resume_accept_) {
      (*qtils::optionTake(resume_accept_))();
    }
  }

  void QuicConnection::onClose() {
    conn_ctx_ = nullptr;
  }
}  // namespace libp2p::transport