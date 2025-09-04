/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <functional>
#include <memory>

#include <libp2p/coro/coro.hpp>

#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/transport/transport_listener.hpp>

namespace libp2p::transport {

  /**
   * Allows to establish connections with other peers and react to received
   * attempts to do so; can be implemented, for example, as TCP, UDP etc
   */
  class TransportAdaptor {
   public:
    using ConnectionCallback =
        void(outcome::result<std::shared_ptr<connection::CapableConnection>>);
    using HandlerFunc = std::function<ConnectionCallback>;

    virtual ~TransportAdaptor() = default;

    /**
     * Try to establish connection with a peer
     * @param remoteId id of remote peer to dial
     * @param address of the peer
     * @return connection in case of success, error otherwise
     */
    virtual CoroOutcome<std::shared_ptr<connection::CapableConnection>> dial(
        const peer::PeerId &remoteId, multi::Multiaddress address) = 0;

    /**
     * Create a listener for incoming connections of this Transport; in case
     * it was already created, return it
     * @return pointer to the created listener
     */
    virtual std::shared_ptr<TransportListener> createListener() = 0;

    /**
     * Check if this transport supports a given multiaddress
     * @param ma to be checked against
     * @return true, if transport supports that multiaddress, false otherwise
     * @note example: '/tcp/...' on tcp transport will return true
     */
    virtual bool canDial(const multi::Multiaddress &ma) const = 0;
  };
}  // namespace libp2p::transport
