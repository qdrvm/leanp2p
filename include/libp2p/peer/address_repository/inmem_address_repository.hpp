/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/peer/address_repository.hpp>

#include <memory>
#include <set>
#include <unordered_map>
#include <vector>

namespace libp2p::peer {

  /**
   * @brief Default clock that is used for TTLs. Steady clock guarantees that
   * for each invocation, time "continues to go forward".
   */
  using Clock = std::chrono::steady_clock;

  /**
   * @brief IN-memory implementation of Address repository.
   */
  class InmemAddressRepository : public AddressRepository {
   public:
    static constexpr auto kDefaultTtl = std::chrono::milliseconds(1000);

    outcome::result<bool> addAddresses(const PeerId &p,
                                       std::span<const multi::Multiaddress> ma,
                                       Milliseconds ttl) override;

    outcome::result<bool> upsertAddresses(
        const PeerId &p,
        std::span<const multi::Multiaddress> ma,
        Milliseconds ttl) override;

    outcome::result<void> updateAddresses(const PeerId &p,
                                          Milliseconds ttl) override;

    void dialFailed(const PeerId &peer_id, const Multiaddress &addr) override;

    outcome::result<std::vector<multi::Multiaddress>> getAddresses(
        const PeerId &p) const override;

    void collectGarbage() override;

    void clear(const PeerId &p) override;

    std::unordered_set<PeerId> getPeers() const override;

   private:
    struct Peer {
      std::unordered_map<Multiaddress, Clock::time_point> expires;
      std::vector<Multiaddress> order;

      bool eraseOrder(const Multiaddress &addr);
    };
    using peer_db = std::unordered_map<PeerId, Peer>;

    bool isNewDnsAddr(const multi::Multiaddress &ma);

    Clock::time_point calculateExpirationTime(const Milliseconds &ttl) const;

    peer_db db_;
    std::set<multi::Multiaddress> resolved_dns_addrs_;
  };

}  // namespace libp2p::peer
