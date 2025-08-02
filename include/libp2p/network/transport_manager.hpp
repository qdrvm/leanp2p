/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>
#include <span>
#include <vector>

#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <libp2p/transport/transport_adaptor.hpp>

namespace libp2p::network {
  class TransportManager {
    using TransportSPtr = std::shared_ptr<transport::TransportAdaptor>;

   public:
    /**
     * Initialize a transport manager from a collection of transports
     * @param transports, which this manager is going to support
     */
    explicit TransportManager(std::vector<TransportSPtr> transports);

    ~TransportManager() = default;

    std::span<const TransportSPtr> getAll() const;

    void clear();

    TransportSPtr findBest(const multi::Multiaddress &ma);

   private:
    std::vector<TransportSPtr> transports_;
  };
}  // namespace libp2p::network
