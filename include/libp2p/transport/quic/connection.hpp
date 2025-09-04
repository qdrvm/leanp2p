/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/coro/channel.hpp>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::transport::lsquic {
  class Engine;
  struct ConnCtx;
}  // namespace libp2p::transport::lsquic

namespace libp2p::transport {
  class QuicConnection : public connection::CapableConnection {
   public:
    QuicConnection(std::shared_ptr<boost::asio::io_context> io_context,
                   lsquic::ConnCtx *conn_ctx,
                   bool initiator,
                   Multiaddress local,
                   Multiaddress remote,
                   PeerId local_peer,
                   PeerId peer,
                   crypto::PublicKey key);
    ~QuicConnection() override;

    // clang-tidy cppcoreguidelines-special-member-functions
    QuicConnection(const QuicConnection &) = delete;
    void operator=(const QuicConnection &) = delete;
    QuicConnection(QuicConnection &&) = delete;
    void operator=(QuicConnection &&) = delete;

    CoroOutcome<size_t> readSome(BytesOut out) override;
    CoroOutcome<size_t> writeSome(BytesIn in) override;

    // Closeable
    bool isClosed() const override;
    outcome::result<void> close() override;

    // LayerConnection
    bool isInitiator() const noexcept override;
    outcome::result<Multiaddress> remoteMultiaddr() override;
    outcome::result<Multiaddress> localMultiaddr() override;

    // SecureConnection
    outcome::result<PeerId> localPeer() const override;
    PeerId remotePeer() const override;
    outcome::result<crypto::PublicKey> remotePublicKey() const override;

    // CapableConnection
    void start() override;
    void stop() override;
    void newStream(StreamHandlerFunc cb) override;
    CoroOutcome<std::shared_ptr<connection::Stream>> newStreamCoroutine()
        override;
    outcome::result<std::shared_ptr<connection::Stream>> newStream() override;
    CoroOutcome<std::shared_ptr<connection::Stream>> acceptStream() override;

    void onClose();

    void onStream(std::shared_ptr<connection::Stream>);

   private:
    std::shared_ptr<boost::asio::io_context> io_context_;
    lsquic::ConnCtx *conn_ctx_;
    bool initiator_;
    Multiaddress local_, remote_;
    PeerId local_peer_, peer_;
    crypto::PublicKey key_;
    CoroOutcomeChannel<std::shared_ptr<connection::Stream>> stream_signal_;
  };
}  // namespace libp2p::transport
