/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_set>

#include <libp2p/peer/peer_id.hpp>

namespace libp2p::peer {

  /**
   * @brief Storage for mapping between peer and its known protocols.
   */
  class UserAgentRepository {
   public:
    virtual ~UserAgentRepository() = default;

    /**
     * @brief Set agent for a peer.
     * @param p peer
     * @param agent type of agent-name
     * @return peer error, if no peer {@param p} found
     */
    virtual void setUserAgent(const PeerId &p, std::string_view ua) = 0;

    /**
     * @brief Removes user-agent of the peer.
     * @param p peer
     * @return peer error, if no peer {@param p} found
     */
    virtual void unsetUserAgent(const PeerId &) = 0;

    /**
     * @brief Get user-agent by given peer {@param p}
     * @param p peer
     * @return user-agent (may be "unknown") or peer error, if no peer
     * {@param p} found
     */
    [[nodiscard]] virtual std::optional<std::string> getUserAgent(
        const PeerId &p) const = 0;

    /**
     * @brief Returns set of peer ids known by this repository.
     * @return unordered set of peers
     */
    [[nodiscard]] virtual std::unordered_set<PeerId> getPeers() const = 0;
  };

}  // namespace libp2p::peer
