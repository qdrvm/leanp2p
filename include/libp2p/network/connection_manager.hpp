/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <unordered_set>

#include <libp2p/basic/garbage_collectable.hpp>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/peer/peer_info.hpp>

namespace libp2p::event::network
{
    /// fired when any new connection, in or outbound, is created
    using OnNewConnectionChannel =
    channel_decl<struct OnNewConnection,
                 std::weak_ptr<connection::CapableConnection>>;

    /// fired when all connections to peer closed
    using OnPeerDisconnectedChannel =
    channel_decl<struct PeerDisconnected, const libp2p::peer::PeerId&>;
} // namespace libp2p::event::network

namespace libp2p::network
{
    /**
     * @brief Connection Manager stores all known connections, and is capable of
     * selecting subset of connections
     */
    class ConnectionManager : public basic::GarbageCollectable
    {
    public:
        using Connection = connection::CapableConnection;
        using ConnectionSPtr = std::shared_ptr<Connection>;

        explicit ConnectionManager(std::shared_ptr<libp2p::event::Bus> bus);

        std::vector<ConnectionSPtr> getConnections() const;

        std::vector<ConnectionSPtr> getConnectionsToPeer(
            const peer::PeerId& p) const;

        ConnectionSPtr getBestConnectionForPeer(
            const peer::PeerId& p) const;

        void addConnectionToPeer(const peer::PeerId& p, ConnectionSPtr c);

        void collectGarbage();

        void closeConnectionsToPeer(const peer::PeerId& p);

        void onConnectionClosed(
            const peer::PeerId& peer_id,
            const std::shared_ptr<connection::CapableConnection>& conn);

    private:
        std::unordered_map<peer::PeerId, std::unordered_set<ConnectionSPtr>>
        connections_;

        std::shared_ptr<libp2p::event::Bus> bus_;

        /// Reentrancy resolver between closeConnectionsToPeer and
        /// onConnectionClosed
        boost::optional<peer::PeerId> closing_connections_to_peer_;
    };
} // namespace libp2p::network
