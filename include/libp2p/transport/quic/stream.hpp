/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/connection/stream.hpp>

namespace libp2p::transport {
  class QuicConnection;
}  // namespace libp2p::transport

namespace libp2p::transport::lsquic {
  class Engine;
  struct StreamCtx;
}  // namespace libp2p::transport::lsquic

namespace libp2p::connection {
  class QuicStream : public Stream {
    friend libp2p::transport::lsquic::Engine;

   public:
    QuicStream(std::shared_ptr<transport::QuicConnection> conn,
               transport::lsquic::StreamCtx *stream_ctx,
               bool is_initiator);
    ~QuicStream() override;

    // clang-tidy cppcoreguidelines-special-member-functions
    QuicStream(const QuicStream &) = delete;
    void operator=(const QuicStream &) = delete;
    QuicStream(QuicStream &&) = delete;
    void operator=(QuicStream &&) = delete;

    // Coroutine-based methods
    CoroOutcome<size_t> readSome(BytesOut out) override;
    CoroOutcome<size_t> writeSome(BytesIn in) override;

    outcome::result<void> close() override;
    void reset() override;
    outcome::result<bool> isInitiator() const override;
    PeerId remotePeerId() const override;
    outcome::result<Multiaddress> localMultiaddr() const override;
    outcome::result<Multiaddress> remoteMultiaddr() const override;

    void onClose();

   private:
    std::shared_ptr<transport::QuicConnection> conn_;
    transport::lsquic::StreamCtx *stream_ctx_;
    bool initiator_;
  };
}  // namespace libp2p::connection
