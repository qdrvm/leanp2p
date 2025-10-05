/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <secp256k1.h>
#include <memory>

#include <libp2p/crypto/secp256k1_provider.hpp>

namespace libp2p::crypto::random {
  class CSPRNG;
}  // namespace libp2p::crypto::random

namespace libp2p::crypto::secp256k1 {
  class Secp256k1ProviderImpl : public Secp256k1Provider {
   public:
    Secp256k1ProviderImpl(std::shared_ptr<random::CSPRNG> random);

    outcome::result<KeyPair> generate() const override;

    outcome::result<PublicKey> derive(const PrivateKey &key) const override;

    outcome::result<Signature> sign(BytesIn message,
                                    const PrivateKey &key) const override;
    outcome::result<SignatureCompact> signCompact(
        const Prehashed &prehashed, const PrivateKey &key) const override;

    outcome::result<bool> verify(BytesIn message,
                                 const Signature &signature,
                                 const PublicKey &key) const override;
    outcome::result<bool> verifyCompact(const Prehashed &prehashed,
                                        const SignatureCompact &signature,
                                        const PublicKey &key) const override;

   private:
    outcome::result<secp256k1_ecdsa_signature> signRaw(
        const Prehashed &prehashed, const PrivateKey &key) const;
    outcome::result<bool> verifyRaw(const Prehashed &prehashed,
                                    const secp256k1_ecdsa_signature &ffi_sig,
                                    const PublicKey &key) const;

    std::shared_ptr<random::CSPRNG> random_;
    std::unique_ptr<secp256k1_context, void (*)(secp256k1_context *)> ctx_;
  };
}  // namespace libp2p::crypto::secp256k1
