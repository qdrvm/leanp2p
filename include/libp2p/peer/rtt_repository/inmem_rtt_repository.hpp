/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/peer/rtt_repository.hpp>

#include <unordered_map>

namespace libp2p::peer {

  class InmemRttRepository : public RttRepository {
   public:
    void updateRtt(const PeerId &p, std::chrono::microseconds rtt) override;

    std::optional<std::chrono::microseconds> getRtt(
        const PeerId &p) const override;

    void clear(const PeerId &p) override;

    void collectGarbage() override;

   private:
    struct PeerRtt {
      std::chrono::microseconds value;
      std::chrono::steady_clock::time_point last_updated;
    };

    std::unordered_map<PeerId, PeerRtt> rtt_map_;
  };

}  // namespace libp2p::peer
