/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/peer/rtt_repository/inmem_rtt_repository.hpp>

namespace libp2p::peer {

  void InmemRttRepository::updateRtt(const PeerId &p,
                                     std::chrono::microseconds rtt) {
    auto now = std::chrono::steady_clock::now();
    auto it = rtt_map_.find(p);
    if (it == rtt_map_.end()) {
      rtt_map_.emplace(p, PeerRtt{rtt, now});
    } else {
      // EWMA: SRTT = (1 - alpha) * SRTT + alpha * RTT
      // alpha = 0.125 (1/8) is standard for TCP
      // SRTT = (7 * SRTT + RTT) / 8
      it->second.value = (it->second.value * 7 + rtt) / 8;
      it->second.last_updated = now;
    }
  }

  std::optional<std::chrono::microseconds> InmemRttRepository::getRtt(
      const PeerId &p) const {
    auto it = rtt_map_.find(p);
    if (it != rtt_map_.end()) {
      return it->second.value;
    }
    return std::nullopt;
  }

  void InmemRttRepository::clear(const PeerId &p) {
    rtt_map_.erase(p);
  }

  void InmemRttRepository::collectGarbage() {
    auto now = std::chrono::steady_clock::now();
    // Expire entries older than 1 hour
    auto ttl = std::chrono::hours(1);

    for (auto it = rtt_map_.begin(); it != rtt_map_.end();) {
      if (now - it->second.last_updated > ttl) {
        it = rtt_map_.erase(it);
      } else {
        ++it;
      }
    }
  }

}  // namespace libp2p::peer
