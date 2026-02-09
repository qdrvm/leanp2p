/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <unordered_map>

#include <libp2p/peer/address_repository/inmem_address_repository.hpp>
#include <libp2p/peer/user_agent_repository.hpp>

namespace libp2p::peer {

  /**
   * @brief In-memory implementation of UserAgent repository. For each peer
   * stores UserAgent data.
   */
  class InmemUserAgentRepository : public UserAgentRepository {
   public:
    ~InmemUserAgentRepository() override = default;

    void updateUserAgent(const PeerId &p,
                         std::string_view ua,
                         Milliseconds ttl) override;

    void updateTtl(const PeerId &p, Milliseconds ttl) override;

    [[nodiscard]] std::optional<std::string> getUserAgent(
        const PeerId &p) const override;

    void collectGarbage() override;

   private:
    std::unordered_map<PeerId, std::pair<std::string, Clock::time_point>> db_;

    [[nodiscard]] Clock::time_point calculateExpirationTime(const Milliseconds &ttl) const;
  };

}  // namespace libp2p::peer
