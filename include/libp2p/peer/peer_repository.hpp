/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <libp2p/peer/address_repository.hpp>
#include <libp2p/peer/key_repository.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <libp2p/peer/protocol_repository.hpp>
#include <libp2p/peer/rtt_repository.hpp>
#include <libp2p/peer/user_agent_repository.hpp>

namespace libp2p::peer {
  /**
   * @brief Repository which stores all known information about peers, including
   * this peer.
   */
  class PeerRepository {
   public:
    PeerRepository(std::shared_ptr<AddressRepository> addrRepo,
                   std::shared_ptr<KeyRepository> keyRepo,
                   std::shared_ptr<ProtocolRepository> protocolRepo,
                   std::shared_ptr<RttRepository> rttRepo,
                   std::shared_ptr<UserAgentRepository> uagent_repo);
    /**
     * @brief Getter for an address repository.
     * @return associated instance of an address repository.
     */
    AddressRepository &getAddressRepository();

    /**
     * @brief Getter for a key repository.
     * @return associated instance of a key repository
     */
    KeyRepository &getKeyRepository();

    /**
     * @brief Getter for a protocol repository.
     * @return associated instance of a protocol repository.
     */
    ProtocolRepository &getProtocolRepository();

    /**
     * @brief Getter for an RTT repository.
     * @return associated instance of an RTT repository.
     */
    RttRepository &getRttRepository();

    /**
     * @brief Getter for a user-agent repository.
     * @return associated instance of a protocol repository.
     */
    UserAgentRepository &getUserAgentRepository();

    /**
     * @brief Returns set of peer ids known by this peer repository.
     * @return unordered set of peers
     */
    [[nodiscard]] std::unordered_set<PeerId> getPeers() const;

    /**
     * @brief Derive a PeerInfo object from the PeerId; can be useful, for
     * example, to establish connections, when only a PeerId is known at the
     * current program point
     * @param peer_id to get PeerInfo for
     * @return PeerInfo
     */
    [[nodiscard]] PeerInfo getPeerInfo(const PeerId &peer_id) const;

   private:
    std::shared_ptr<AddressRepository> addr_;
    std::shared_ptr<KeyRepository> key_;
    std::shared_ptr<ProtocolRepository> proto_;
    std::shared_ptr<RttRepository> rtt_;
    std::shared_ptr<UserAgentRepository> uagent_;
  };
}  // namespace libp2p::peer
