/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/crypto/key.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/peer/peer_id.hpp>

namespace libp2p::peer {

  /**
   * @brief Component, which "owns" information about current Host identity.
   */
  class IdentityManager {
   public:
    IdentityManager(
        crypto::KeyPair keyPair,
        const std::shared_ptr<crypto::marshaller::KeyMarshaller> &marshaller);

    const peer::PeerId &getId() const;

    const crypto::KeyPair &getKeyPair() const;

   private:
    std::unique_ptr<peer::PeerId> id_;
    std::unique_ptr<crypto::KeyPair> keyPair_;
  };

}  // namespace libp2p::peer
