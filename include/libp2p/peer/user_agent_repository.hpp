/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/garbage_collectable.hpp>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <libp2p/peer/peer_id.hpp>

namespace libp2p::peer {

  /**
   * @brief Storage for mapping between peer and its known protocols.
   */
  class UserAgentRepository : public basic::GarbageCollectable {
   public:
    using Milliseconds = std::chrono::milliseconds;

    ~UserAgentRepository() override = default;

    /**
     * @brief Update user-agent with new {@param ttl} or insert new
     * user-agent with new {@param ttl}
     * @param p peer
     * @param ua user-agent type
     * @param ttl ttl
     * @return true/false if at least one new user-agent was added or not,
     */
    virtual void updateUserAgent(const PeerId &p,
                                 std::string_view ua,
                                 Milliseconds ttl) = 0;

    /**
     * @brief Bump ttl of a given peer {@param p}
     * @param p peer
     * @param ttl time to live for user-agent
     * @return error when no peer has been found
     */
    virtual void updateTtl(const PeerId &p, Milliseconds ttl) = 0;

    /**
     * @brief Get user-agent by given peer {@param p}
     * @param p peer
     * @return user-agent (may be "unknown") or peer error, if no peer
     * {@param p} found
     */
    [[nodiscard]] virtual std::optional<std::string> getUserAgent(
        const PeerId &p) const = 0;
  };

}  // namespace libp2p::peer
