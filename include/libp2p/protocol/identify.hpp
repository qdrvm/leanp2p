/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/coro/coro.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/peer/stream_protocols.hpp>
#include <libp2p/protocol/base_protocol.hpp>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::connection {
  class CapableConnection;
}  // namespace libp2p::connection

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace libp2p::peer {
  class IdentityManager;
}  // namespace libp2p::peer

namespace libp2p::protocol {
  struct IdentifyInfo {
    PeerId peer_id;
    std::string protocol_version;
    std::string agent_version;
    std::vector<Multiaddress> listen_addresses;
    Multiaddress observed_address;
    StreamProtocols protocols;
  };

  using OnIdentifyChannel = event::channel_decl<IdentifyInfo, IdentifyInfo>;

  struct IdentifyConfig {
    // Fixes default field values with boost::di.
    IdentifyConfig() = default;

    std::string protocol_version;
    std::string agent_version;
    std::vector<Multiaddress> listen_addresses;
  };

  class Identify : public std::enable_shared_from_this<Identify>,
                   public BaseProtocol {
   public:
    Identify(std::shared_ptr<boost::asio::io_context> io_context,
             std::shared_ptr<host::BasicHost> host,
             std::shared_ptr<peer::IdentityManager> id_mgr,
             IdentifyConfig config);

    // Adaptor
    StreamProtocols getProtocolIds() const override;

    // BaseProtocol
    void handle(std::shared_ptr<connection::Stream> stream) override;

    void start();

   private:
    Coro<void> recv_identify(
        std::shared_ptr<connection::CapableConnection> connection);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<host::BasicHost> host_;
    std::shared_ptr<peer::IdentityManager> id_mgr_;
    IdentifyConfig config_;
    event::Handle on_peer_connected_sub_;
  };
}  // namespace libp2p::protocol
