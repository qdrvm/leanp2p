/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/log/logger.hpp>
#include <libp2p/peer/protocol.hpp>
#include <libp2p/protocol/base_protocol.hpp>
#include <libp2p/protocol/echo/echo_config.hpp>

namespace libp2p::protocol {

  /**
   * @brief Simple echo protocol. It will keep responding with the same data it
   * reads from the connection.
   */
  class Echo : public BaseProtocol {
   public:
    Echo(std::shared_ptr<boost::asio::io_context> io_context,
         EchoConfig config = EchoConfig{});

    // NOLINTNEXTLINE(modernize-use-nodiscard)
    StreamProtocols getProtocolIds() const override;

    // handle incoming stream
    void handle(std::shared_ptr<Stream> stream) override;

    // create client session, which simplifies writing tests and interaction
    // with server.
    // std::shared_ptr<ClientEchoSession> createClient(
    //     const std::shared_ptr<connection::Stream> &stream);

   private:
    Coro<void> doRead(std::shared_ptr<connection::Stream>);
    void stop(std::shared_ptr<connection::Stream> stream);

    std::shared_ptr<boost::asio::io_context> io_context_;
    EchoConfig config_;
    bool repeat_infinitely_;
    log::Logger log_ = log::createLogger("Echo");
  };

}  // namespace libp2p::protocol
