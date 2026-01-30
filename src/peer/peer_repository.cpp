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
      std::shared_ptr<UserAgentRepository> uagent_repository)
      : addr_(std::move(addr_repo)),
        key_(std::move(key_repo)),
        proto_(std::move(protocol_repo)) ,
        uagent_(std::move(uagent_repository)) {
    BOOST_ASSERT(addr_ != nullptr);
    BOOST_ASSERT(key_ != nullptr);
    BOOST_ASSERT(proto_ != nullptr);
    BOOST_ASSERT(uagent_ != nullptr);
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

  UserAgentRepository &PeerRepository::getUserAgentRepository() {
    return *uagent_;
  }

  std::unordered_set<PeerId> PeerRepository::getPeers() const {
    std::unordered_set<PeerId> peers;
    merge_sets<PeerId>(peers, addr_->getPeers());
    merge_sets<PeerId>(peers, key_->getPeers());
    merge_sets<PeerId>(peers, proto_->getPeers());
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
