/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <optional>

#include <libp2p/basic/garbage_collectable.hpp>
#include <libp2p/peer/peer_id.hpp>

namespace libp2p::peer {

  /**
   * @brief Repository to store Round Trip Time (RTT) for peers.
   * It calculates smoothed RTT using EWMA.
   */
  class RttRepository : public basic::GarbageCollectable {
   public:
    virtual ~RttRepository() = default;

    /**
     * @brief Update RTT for a peer.
     * @param p PeerId
     * @param rtt Measured RTT
     */
    virtual void updateRtt(const PeerId &p, std::chrono::microseconds rtt) = 0;

    /**
     * @brief Get smoothed RTT for a peer.
     * @param p PeerId
     * @return Smoothed RTT if available, std::nullopt otherwise.
     */
    virtual std::optional<std::chrono::microseconds> getRtt(
        const PeerId &p) const = 0;

    /**
     * @brief Remove RTT information for a peer.
     * @param p PeerId
     */
    virtual void clear(const PeerId &p) = 0;
  };

}  // namespace libp2p::peer
