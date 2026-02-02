/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/peer/user_agent_repository/inmem_user_agent_repository.hpp>

#include <ranges>

#include <libp2p/peer/errors.hpp>

namespace libp2p::peer {

  void InmemUserAgentRepository::setUserAgent(const PeerId &p,
                                              std::string_view ua) {
    db_.insert_or_assign(p, ua);
  }

  void InmemUserAgentRepository::unsetUserAgent(const PeerId &p) {
    db_.erase(p);
  }

  std::optional<std::string> InmemUserAgentRepository::getUserAgent(
      const PeerId &p) const {
    if (auto it = db_.find(p); it != db_.end()) {
      return it->second;
    }
    return std::nullopt;
  }

  std::unordered_set<PeerId> InmemUserAgentRepository::getPeers() const {
    return db_
         | std::ranges::views::transform([](const auto &p) { return p.first; })
         | std::ranges::to<std::unordered_set<PeerId>>();
  }

}  // namespace libp2p::peer
