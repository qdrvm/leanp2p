/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lsquic.h>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/transport/quic/connection.hpp>
#include <libp2p/transport/quic/engine.hpp>
#include <libp2p/transport/quic/error.hpp>
#include <libp2p/transport/quic/stream.hpp>

namespace libp2p::connection {
  using transport::lsquic::StreamCtx;

  QuicStream::QuicStream(std::shared_ptr<transport::QuicConnection> conn,
                         StreamCtx *stream_ctx,
                         bool initiator)
      : conn_{std::move(conn)},
        stream_ctx_{stream_ctx},
        initiator_{initiator} {}

  QuicStream::~QuicStream() {
    reset();
  }

  CoroOutcome<size_t> QuicStream::readSome(BytesOut out) {
    if (not stream_ctx_) {
      co_return QuicError::STREAM_CLOSED;
    }
    if (stream_ctx_->reading) {
      co_return QuicError::STREAM_READ_IN_PROGRESS;
    }
    while (true) {
      auto n =
          lsquic_stream_read(stream_ctx_->ls_stream, out.data(), out.size());
      if (n == 0) {
        break;
      }
      if (n == -1) {
        if (errno != EWOULDBLOCK) {
          break;
        }
        co_await coroHandler<void>([&](CoroHandler<void> &&handler) {
          stream_ctx_->reading.emplace(std::move(handler));
          lsquic_stream_wantread(stream_ctx_->ls_stream, 1);
        });
        continue;
      }
      co_return n;
    }
    co_return QuicError::STREAM_CLOSED;
  }

  CoroOutcome<size_t> QuicStream::writeSome(BytesIn in) {
    outcome::result<size_t> r = QuicError::STREAM_CLOSED;
    if (not stream_ctx_) {
      co_return r;
    }
    while (true) {
      // Missing from `lsquic_stream_write` documentation comment.
      // Return value 0 means buffer is full.
      // Call `lsquic_stream_wantwrite` and wait for `stream_if.on_write`
      // callback, before calling `lsquic_stream_write` again.
      auto n =
          lsquic_stream_write(stream_ctx_->ls_stream, in.data(), in.size());
      stream_ctx_->engine->wantFlush(stream_ctx_);
      if (n == 0) {
        co_await coroHandler<void>([&](CoroHandler<void> &&handler) {
          stream_ctx_->writing.emplace(std::move(handler));
          lsquic_stream_wantwrite(stream_ctx_->ls_stream, 1);
        });
        continue;
      }
      if (n > 0) {
        r = n;
      }
      break;
    }
    co_return r;
  }

  outcome::result<void> QuicStream::close() {
    if (not stream_ctx_) {
      return outcome::success();
    }
    lsquic_stream_shutdown(stream_ctx_->ls_stream, 1);
    return outcome::success();
  }

  void QuicStream::reset() {
    if (not stream_ctx_) {
      return;
    }
    lsquic_stream_close(stream_ctx_->ls_stream);
  }

  outcome::result<bool> QuicStream::isInitiator() const {
    return initiator_;
  }

  PeerId QuicStream::remotePeerId() const {
    return conn_->remotePeer();
  }

  outcome::result<Multiaddress> QuicStream::localMultiaddr() const {
    return conn_->localMultiaddr();
  }

  outcome::result<Multiaddress> QuicStream::remoteMultiaddr() const {
    return conn_->remoteMultiaddr();
  }

  void QuicStream::onClose() {
    stream_ctx_ = nullptr;
  }

}  // namespace libp2p::connection