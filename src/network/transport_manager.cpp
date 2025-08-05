/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/network/transport_manager.hpp>

#include <algorithm>

namespace libp2p::network {
  TransportManager::TransportManager(const std::vector<TransportSPtr>& transports)
      : transports_{transports} {
    BOOST_ASSERT_MSG(!transports_.empty(), "TransportManager got 0 transports");
    BOOST_ASSERT(std::all_of(transports_.begin(),
                             transports_.end(),
                             [](auto &&t) { return t != nullptr; }));
  }

  std::span<const TransportManager::TransportSPtr> TransportManager::getAll()
      const {
    return transports_;
  }

  void TransportManager::clear() {
    transports_.clear();
  }

  TransportManager::TransportSPtr TransportManager::findBest(
      const multi::Multiaddress &ma) {
    auto it = std::find_if(transports_.begin(),
                           transports_.end(),
                           [&ma](const auto &t) { return t->canDial(ma); });
    if (it != transports_.end()) {
      return *it;
    }
    return nullptr;
  }
}  // namespace libp2p::network
