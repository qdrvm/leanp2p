/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <unordered_map>

#include <libp2p/peer/user_agent_repository.hpp>

namespace libp2p::peer {

  /**
   * @brief In-memory implementation of UserAgent repository. For each peer
   * stores UserAgent data.
   */
  class InmemUserAgentRepository : public UserAgentRepository {
   public:
    ~InmemUserAgentRepository() override = default;

    void setUserAgent(const PeerId &p, std::string_view ua) override;

    void unsetUserAgent(const PeerId &p) override;

    [[nodiscard]] std::optional<std::string> getUserAgent(
        const PeerId &p) const override;

    [[nodiscard]] std::unordered_set<PeerId> getPeers() const override;

   private:
    std::unordered_map<PeerId, std::string> db_;
  };

}  // namespace libp2p::peer
