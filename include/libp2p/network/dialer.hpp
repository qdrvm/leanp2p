/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <deque>
#include <unordered_map>
#include <unordered_set>

// #include <libp2p/basic/scheduler.hpp>
#include <libp2p/network/connection_manager.hpp>
#include <libp2p/network/dialer.hpp>
#include <libp2p/network/listener_manager.hpp>
#include <libp2p/network/transport_manager.hpp>
#include <libp2p/peer/address_repository.hpp>
#include <libp2p/peer/stream_protocols.hpp>
#include <libp2p/protocol_muxer/protocol_muxer.hpp>

namespace libp2p::network {

  using DialResult =
      outcome::result<std::shared_ptr<connection::CapableConnection>>;
  using DialResultFunc = std::function<void(DialResult)>;

  class Dialer {
   public:
    enum class Error {
      ANOTHER_DIAL_IN_PROGRESS = 1,
      STATE_INCONSISTENCY = 2,
      NO_DESTINATION_ADDRESS = 3,
      ADDRESS_FAMILY_NOT_SUPPORTED = 4,
      HOST_UNREACHABLE = 5
    };
    Dialer(std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect,
           std::shared_ptr<TransportManager> tmgr,
           std::shared_ptr<ConnectionManager> cmgr,
           std::shared_ptr<ListenerManager> listener,
           std::shared_ptr<peer::AddressRepository> addr_repo
           // std::shared_ptr<basic::Scheduler> scheduler
    );

    // Establishes a connection to a given peer
    CoroOutcome<std::shared_ptr<connection::CapableConnection>> dial(
        const PeerInfo &p);

    /**
     * NewStream returns a new stream to given peer p.
     * If there is no connection to p, returns error.
     */
    CoroOutcome<std::shared_ptr<Stream>> newStream(
        std::shared_ptr<connection::CapableConnection> conn,
        StreamProtocols protocols);

    /**
     * NewStream returns a new stream to given peer p.
     * If there is no connection to p, returns error.
     */
    CoroOutcome<std::shared_ptr<Stream>> newStream(const PeerInfo &peer_id,
                                                   StreamProtocols protocols);

   private:
    // A context to handle an intermediary state of the peer we are dialing to
    // but the connection is not yet established
    struct DialCtx {
      /// Queue of addresses to try connect to
      std::deque<Multiaddress> addr_queue;

      /// Tracks addresses added to `addr_queue`
      std::unordered_set<Multiaddress> addr_seen;

      /// Callbacks for all who requested a connection to the peer
      // std::vector<Dialer::DialResultFunc> callbacks;

      /// Result temporary storage to propagate via callbacks
      std::optional<DialResult> result;
      // ^ used when all connecting attempts failed and no more known peer
      // addresses are left

      // indicates that at least one attempt to dial was happened
      // (at least one supported network transport was found and used)
      bool dialled = false;
    };

    // Perform a single attempt to dial to the peer via the next known address
    CoroOutcome<std::shared_ptr<connection::CapableConnection>> rotate(
        const peer::PeerId &peer_id);
    // Finalize dialing to the peer and propagate a given result to all
    // connection requesters
    void completeDial(const peer::PeerId &peer_id);

    std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect_;
    std::shared_ptr<TransportManager> tmgr_;
    std::shared_ptr<ConnectionManager> cmgr_;
    std::shared_ptr<ListenerManager> listener_;
    std::shared_ptr<peer::AddressRepository> addr_repo_;
    // std::shared_ptr<basic::Scheduler> scheduler_;
    log::Logger log_;

    std::unordered_map<peer::PeerId, DialCtx> dialing_peers_;
  };

}  // namespace libp2p::network

OUTCOME_HPP_DECLARE_ERROR(libp2p::network, Dialer::Error)