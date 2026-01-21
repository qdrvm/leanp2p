/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/peer/peer_repository.hpp>

#include <libp2p/multi/multiaddress.hpp>

namespace {

  template <typename T>
  inline void merge_sets(std::unordered_set<T> &out,
                         std::unordered_set<T> &&in) {
    out.insert(in.begin(), in.end());
  }

}  // namespace

namespace libp2p::peer {

  PeerRepository::PeerRepository(
      std::shared_ptr<AddressRepository> addr_repo,
      std::shared_ptr<KeyRepository> key_repo,
      std::shared_ptr<ProtocolRepository> protocol_repo,
      std::shared_ptr<RttRepository> rtt_repo)
      : addr_(std::move(addr_repo)),
        key_(std::move(key_repo)),
        proto_(std::move(protocol_repo)),
        rtt_(std::move(rtt_repo)) {
    BOOST_ASSERT(addr_ != nullptr);
    BOOST_ASSERT(key_ != nullptr);
    BOOST_ASSERT(proto_ != nullptr);
    BOOST_ASSERT(rtt_ != nullptr);
  }

  AddressRepository &PeerRepository::getAddressRepository() {
    return *addr_;
  }

  KeyRepository &PeerRepository::getKeyRepository() {
    return *key_;
  }

  ProtocolRepository &PeerRepository::getProtocolRepository() {
    return *proto_;
  }

  RttRepository &PeerRepository::getRttRepository() {
    return *rtt_;
  }

  std::unordered_set<PeerId> PeerRepository::getPeers() const {
    std::unordered_set<PeerId> peers;
    merge_sets<PeerId>(peers, addr_->getPeers());
    merge_sets<PeerId>(peers, key_->getPeers());
    merge_sets<PeerId>(peers, proto_->getPeers());
    // RttRepository doesn't have getPeers() yet, but it's fine.
    // Usually peers in RttRepository should be in others too.
    return peers;
  }

  PeerInfo PeerRepository::getPeerInfo(const PeerId &peer_id) const {
    auto peer_addrs_res = addr_->getAddresses(peer_id);
    if (!peer_addrs_res) {
      return PeerInfo{peer_id, {}};
    }
    return PeerInfo{peer_id, std::move(peer_addrs_res.value())};
  }

}  // namespace libp2p::peer
