/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <functional>
#include <string_view>

#include <libp2p/connection/stream.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <libp2p/peer/peer_repository.hpp>
#include <libp2p/peer/protocol.hpp>
// #include <libp2p/peer/protocol_predicate.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/network/transport_manager.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/peer/stream_protocols.hpp>
#include <libp2p/protocol/base_protocol.hpp>

#include "libp2p/network/dialer.hpp"
#include "libp2p/network/listener_manager.hpp"
#include "libp2p/network/transport_manager.hpp"
#include "libp2p/peer/identity_manager.hpp"

namespace libp2p::host {
  struct Libp2pClientVersion {
    std::string version;
  };

  /**
   * Main class, which represents single peer in p2p network.
   *
   * It is capable of:
   * - create new connections to remote peers
   * - create new streams to remote peers
   * - listen on one or multiple addresses
   * - register protocols
   * - handle registered protocols (receive and handle incoming streams with
   * given protocol)
   */
  class BasicHost {
   public:
    using ConnectionResult =
        outcome::result<std::shared_ptr<connection::CapableConnection>>;
    using ConnectionResultHandler = std::function<void(ConnectionResult)>;

    using NewConnectionHandler = std::function<void(peer::PeerInfo &&)>;

    enum class Connectedness {
      NOT_CONNECTED,  ///< we don't know peer's addresses, and are not connected
      CONNECTED,      ///< we have at least one connection to this peer
      CAN_CONNECT,    ///< we know peer's addr, and we can dial
      CAN_NOT_CONNECT  ///< we know peer's addr, but can not dial (no
      ///< transports)
    };

    BasicHost(std::shared_ptr<peer::IdentityManager> idmgr,
              // std::unique_ptr<network::Network> network,
              std::unique_ptr<network::ListenerManager> listener,
              std::unique_ptr<network::ConnectionManager> connection_manager,
              std::unique_ptr<network::Dialer> dialer,
              std::unique_ptr<peer::PeerRepository> repo,
              std::shared_ptr<event::Bus> bus,
              std::shared_ptr<network::TransportManager> transport_manager,
              Libp2pClientVersion libp2p_client_version);

    /**
     * @brief Get a version of Libp2p, supported by this Host
     */
    std::string_view getLibp2pVersion() const;

    /**
     * @brief Stores OnNewConnectionHandler.
     * @param h handler function to store
     */
    event::Handle setOnNewConnectionHandler(
        const NewConnectionHandler &h) const;

    /**
     * @brief Get a version of this Libp2p client
     */
    std::string_view getLibp2pClientVersion() const;

    /**
     * @brief Get identifier of this Host
     */
    peer::PeerId getId() const;

    /**
     * @brief Get PeerInfo of this Host
     */
    peer::PeerInfo getPeerInfo() const;

    /**
     * @brief Get addresses we provided to listen on (added by listen).
     */
    std::vector<multi::Multiaddress> getAddresses() const;

    /**
     * @brief Get addresses that read from listen sockets.
     *
     * May return 0 addresses if no listeners found or all listeners stopped.
     */
    std::vector<multi::Multiaddress> getAddressesInterfaces() const;

    /**
     * @brief Get our addresses observed by other peers.
     *
     * May return 0 addresses if we don't know our observed addresses.
     */
    std::vector<multi::Multiaddress> getObservedAddresses() const;

    /**
     * @brief Get connectedness information for given peer
     * @param p Peer info
     * @return Connectedness
     */
    Connectedness connectedness(const peer::PeerInfo &p) const;

    /**
     * @brief Let Host handle given protocols, and use matcher to check if we
     * support given remote protocol.
     * @param cb of the arrived stream
     * @param predicate function that takes received protocol (/ping/1.0.0) and
     * should return true, if this protocol can be handled.
     */
    // void setProtocolHandler(StreamProtocols protocols,
    //                                 StreamAndProtocolCb cb,
    //                                 ProtocolPredicate predicate = {});
    outcome::result<void> listenProtocol(
        const peer::ProtocolName &name,
        std::shared_ptr<protocol::BaseProtocol> protocol);

    /**
     * @brief Initiates connection to the peer {@param peer_info}.
     * @param peer_info peer to connect.
     */
    CoroOutcome<std::shared_ptr<connection::CapableConnection>> connect(
        const peer::PeerInfo &peer_info);

    /**
     * Closes all connections (outbound and inbound) to given {@param peer_id}
     */
    void disconnect(const peer::PeerId &peer_id);

    /**
     * @brief Open new stream to the peer {@param peer_info} with protocol
     * {@param protocol}
     * @param peer_info stream will be opened to this peer
     * @param protocols "speak" using first supported protocol
     */
    CoroOutcome<std::shared_ptr<connection::Stream>> newStream(
        const peer::PeerInfo &peer_info, StreamProtocols protocols);

    /**
     * @brief Open new stream to the peer {@param peer} with protocol
     * {@param protocol} in optimistic way. Assuming that connection exists.
     * @param peer stream will be opened to this peer
     * @param protocols "speak" using first supported protocol
     */
    CoroOutcome<std::shared_ptr<connection::Stream>> newStream(
        const PeerId &peer_id, StreamProtocols protocols) {
      co_return co_await newStream(PeerInfo{.id = peer_id},
                                   std::move(protocols));
    }

    /**
     * @brief Create listener on given multiaddress.
     * @param ma address
     * @return may return error
     */
    outcome::result<void> listen(const multi::Multiaddress &ma);

    /**
     * @brief Close listener on given address.
     * @param ma address
     * @return may return error
     */
    outcome::result<void> closeListener(const multi::Multiaddress &ma);

    /**
     * @brief Removes listener on given address.
     * @param ma address
     * @return may return error
     */
    outcome::result<void> removeListener(const multi::Multiaddress &ma);

    /**
     * @brief Start all listeners.
     */
    void start();

    /**
     * @brief Stop all listeners.
     */
    void stop();

    /**
     * @brief Getter for a network.
     */
    // network::Network &getNetwork();

    /**
     * @brief Getter for a peer repository.
     */
    peer::PeerRepository &getPeerRepository();

    /**
     * @brief Getter for a router.
     */
    // network::Router &getRouter();

    /**
     * @brief Getter for event bus.
     */
    event::Bus &getBus();

   private:
    std::shared_ptr<peer::IdentityManager> idmgr_;
    std::unique_ptr<network::ListenerManager> listener_;
    std::unique_ptr<network::ConnectionManager> connection_manager_;
    std::unique_ptr<network::Dialer> dialer_;
    // std::unique_ptr<network::Network> network_;
    std::unique_ptr<peer::PeerRepository> repo_;
    std::shared_ptr<event::Bus> bus_;
    std::shared_ptr<network::TransportManager> transport_manager_;
    Libp2pClientVersion libp2p_client_version_;
  };
}  // namespace libp2p::host
