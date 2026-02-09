/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/peer/user_agent_repository/inmem_user_agent_repository.hpp>

#include <ranges>

namespace libp2p::peer {

  Clock::time_point InmemUserAgentRepository::calculateExpirationTime(
      const Milliseconds &ttl) const {
    static const auto max_time = Clock::time_point::max();
    const auto now = Clock::now();
    if (now >= max_time - ttl) {
      return max_time;
    }
    return now + ttl;
  }

  void InmemUserAgentRepository::updateUserAgent(const PeerId &p,
                                                 std::string_view ua,
                                                 Milliseconds ttl) {
    db_.insert_or_assign(
        p, std::pair(std::string(ua), calculateExpirationTime(ttl)));
  }

  void InmemUserAgentRepository::updateTtl(const PeerId &p, Milliseconds ttl) {
    if (ttl > std::chrono::milliseconds::zero()) {
      if (auto it = db_.find(p); it != db_.end()) {
        it->second.second = calculateExpirationTime(ttl);
      }
    } else {
      db_.erase(p);
    }
  }

  std::optional<std::string> InmemUserAgentRepository::getUserAgent(
      const PeerId &p) const {
    if (auto it = db_.find(p); it != db_.end()) {
      return it->second.first;
    }
    return std::nullopt;
  }

  void InmemUserAgentRepository::collectGarbage() {
    auto now = Clock::now();
    for (auto it = db_.begin(); it != db_.end();) {
      const auto &expiration_time = it->second.second;
      if (now >= expiration_time) {
        it = db_.erase(it);
      } else {
        ++it;
      }
    }
  }

}  // namespace libp2p::peer
