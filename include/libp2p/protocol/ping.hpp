/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/coro/coro.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/protocol/base_protocol.hpp>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::connection {
  struct CapableConnection;
}  // namespace libp2p::connection

namespace libp2p::crypto::random {
  class RandomGenerator;
}  // namespace libp2p::crypto::random

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace libp2p::protocol {
  struct PingConfig {
    // Fixes default field values with boost::di.
    PingConfig() = default;

    /**
     * Time to wait for response.
     */
    std::chrono::seconds timeout{20};
    /**
     * Time between ping requests.
     */
    std::chrono::seconds interval{15};
  };

  class Ping : public std::enable_shared_from_this<Ping>, public BaseProtocol {
   public:
    enum Error {
      INVALID_RESPONSE,
    };
    Q_ENUM_ERROR_CODE_FRIEND(Error) {
      using E = decltype(e);
      switch (e) {
        case E::INVALID_RESPONSE:
          return "Ping received invalid response";
      }
      abort();
    }

    Ping(std::shared_ptr<boost::asio::io_context> io_context,
         std::shared_ptr<host::BasicHost> host,
         std::shared_ptr<libp2p::crypto::random::RandomGenerator> random,
         PingConfig config);

    // Adaptor
    StreamProtocols getProtocolIds() const override;

    // BaseProtocol
    void handle(std::shared_ptr<connection::Stream> stream) override;

    void start();

    /**
     * Performs a single ping on the given connection.
     * Opens a new stream for the ping.
     * On successful ping, automatically updates the RTT repository with the
     * measured round-trip time.
     * @param conn Connection to ping.
     * @param timeout Timeout for the ping operation.
     * @return RTT of the ping.
     */
    CoroOutcome<std::chrono::microseconds> ping(
        std::shared_ptr<connection::CapableConnection> conn,
        std::chrono::milliseconds timeout);

   private:
    Coro<void> pingLoop(std::shared_ptr<connection::CapableConnection> connection);

    CoroOutcome<std::chrono::microseconds> ping(
        std::shared_ptr<connection::Stream> stream,
        std::chrono::milliseconds timeout);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<host::BasicHost> host_;
    std::shared_ptr<libp2p::crypto::random::RandomGenerator> random_;
    PingConfig config_;
    event::Handle on_peer_connected_sub_;
  };
}  // namespace libp2p::protocol
