/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <tsl/htrie_map.h>

#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/network/connection_manager.hpp>
#include <libp2p/network/transport_manager.hpp>
#include <libp2p/peer/address_repository.hpp>
#include <libp2p/protocol/base_protocol.hpp>
#include <libp2p/protocol_muxer/protocol_muxer.hpp>

namespace libp2p::network {

  class ListenerManager {
   public:
    enum class Error { NO_HANDLER_FOUND = 1 };

    ListenerManager(std::shared_ptr<boost::asio::io_context> io_context,
                    std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect,
                    // std::shared_ptr<Router> router,
                    std::shared_ptr<TransportManager> tmgr,
                    std::shared_ptr<ConnectionManager> cmgr);

    bool isStarted() const;

    void start();

    void stop();

    outcome::result<void> closeListener(const multi::Multiaddress &ma);

    outcome::result<void> removeListener(const multi::Multiaddress &ma);

    outcome::result<void> listen(const multi::Multiaddress &ma);

    outcome::result<void> listenProtocol(
        const peer::ProtocolName &name,
        std::shared_ptr<protocol::BaseProtocol> protocol);

    /**
     * Get a list of handled protocols
     * @return supported protocols; may also include protocol prefixes; if any
     * set
     */
    std::vector<peer::ProtocolName> getSupportedProtocols() const;

    std::vector<multi::Multiaddress> getListenAddresses() const;

    std::vector<multi::Multiaddress> getListenAddressesInterfaces() const;

    // Router &getRouter();

    void onConnection(
        outcome::result<std::shared_ptr<connection::CapableConnection>> rconn);

   private:
    outcome::result<std::shared_ptr<protocol::BaseProtocol>> getProtocol(
        const peer::ProtocolName &name) const;

    std::shared_ptr<boost::asio::io_context> io_context_;
    bool started = false;

    // clang-format off
    std::unordered_map<multi::Multiaddress, std::shared_ptr<transport::TransportListener>> listeners_;
    // clang-format on

    std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect_;
    // std::shared_ptr<network::Router> router_;
    std::shared_ptr<TransportManager> tmgr_;
    std::shared_ptr<ConnectionManager> cmgr_;
    tsl::htrie_map<char, std::shared_ptr<protocol::BaseProtocol>> protocols_;
  };

}  // namespace libp2p::network

OUTCOME_HPP_DECLARE_ERROR(libp2p::network, ListenerManager::Error)