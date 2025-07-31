/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <lsquic.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/use_awaitable.hpp>
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
      initiator_{initiator} {
  }

  QuicStream::~QuicStream() {
    reset();
  }

  template <typename T>
  void ambigousSize(std::span<T> &s, size_t n) {
    if (n > s.size()) {
      throw std::logic_error{"libp2p::ambigousSize"};
    }
    s = s.first(n);
  }

  boost::asio::awaitable<outcome::result<size_t>> QuicStream::read(
      BytesOut out,
      size_t bytes) {
    ambigousSize(out, bytes);
    if (not stream_ctx_) {
      co_return QuicError::STREAM_CLOSED;
    }
    if (stream_ctx_->reading) {
      co_return QuicError::STREAM_READ_IN_PROGRESS;
    }
    auto n = lsquic_stream_read(stream_ctx_->ls_stream, out.data(), out.size());
    if (n == -1 && errno == EWOULDBLOCK) {
      bool done = false;
      outcome::result<size_t> r = QuicError::STREAM_CLOSED;
      stream_ctx_->reading.emplace(transport::lsquic::StreamCtx::Reading{
          out, [&](auto res) {
            r = res;
            done = true;
          }});
      lsquic_stream_wantread(stream_ctx_->ls_stream, 1);
      while (!done) {
        co_await boost::asio::post(boost::asio::use_awaitable);
      }
      co_return r;
    }
    if (n > 0) {
      co_return n;
    }
    co_return QuicError::STREAM_CLOSED;
  }

  boost::asio::awaitable<outcome::result<size_t>> QuicStream::readSome(
      BytesOut out,
      size_t bytes) {
    ambigousSize(out, bytes);
    if (not stream_ctx_) {
      co_return QuicError::STREAM_CLOSED;
    }
    if (stream_ctx_->reading) {
      co_return QuicError::STREAM_READ_IN_PROGRESS;
    }
    auto n = lsquic_stream_read(stream_ctx_->ls_stream, out.data(), out.size());
    if (n == -1 && errno == EWOULDBLOCK) {
      bool done = false;
      outcome::result<size_t> r = QuicError::STREAM_CLOSED;
      stream_ctx_->reading.emplace(transport::lsquic::StreamCtx::Reading{
          out, [&](auto res) {
            r = res;
            done = true;
          }});
      lsquic_stream_wantread(stream_ctx_->ls_stream, 1);
      while (!done) {
        co_await boost::asio::post(boost::asio::use_awaitable);
      }
      co_return r;
    }
    if (n > 0) {
      co_return n;
    }
    co_return QuicError::STREAM_CLOSED;
  }

  boost::asio::awaitable<std::error_code> QuicStream::writeSome(
      BytesIn in,
      size_t bytes) {
    ambigousSize(in, bytes);
    if (not stream_ctx_) {
      co_return QuicError::STREAM_CLOSED;
    }
    auto n = lsquic_stream_write(stream_ctx_->ls_stream, in.data(), in.size());
    if (n > 0 && lsquic_stream_flush(stream_ctx_->ls_stream) == 0) {
      stream_ctx_->engine->process();
      co_return std::error_code{};
    }
    stream_ctx_->engine->process();
    co_return QuicError::STREAM_CLOSED;
  }
}