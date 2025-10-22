/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>

#include <libp2p/crypto/error.hpp>
#include <libp2p/crypto/random_generator.hpp>
#include <libp2p/crypto/sha/sha256.hpp>

namespace libp2p::crypto::secp256k1 {
  Secp256k1ProviderImpl::Secp256k1ProviderImpl(
      std::shared_ptr<random::CSPRNG> random)
      : random_{std::move(random)},
        ctx_{
            secp256k1_context_create(SECP256K1_CONTEXT_SIGN
                                     | SECP256K1_CONTEXT_VERIFY),
            secp256k1_context_destroy,
        } {}

  outcome::result<KeyPair> Secp256k1ProviderImpl::generate() const {
    PrivateKey private_key{};
    do {
      random_->fillRandomly(private_key);
    } while (secp256k1_ec_seckey_verify(ctx_.get(), private_key.data()) == 0);
    OUTCOME_TRY(public_key, derive(private_key));
    return KeyPair{private_key, public_key};
  }

  outcome::result<PublicKey> Secp256k1ProviderImpl::derive(
      const PrivateKey &key) const {
    secp256k1_pubkey ffi_pub;
    if (secp256k1_ec_pubkey_create(ctx_.get(), &ffi_pub, key.data()) == 0) {
      return KeyGeneratorError::KEY_DERIVATION_FAILED;
    }
    PublicKey public_key{};
    size_t size = public_key.size();
    if (secp256k1_ec_pubkey_serialize(ctx_.get(),
                                      public_key.data(),
                                      &size,
                                      &ffi_pub,
                                      SECP256K1_EC_COMPRESSED)
        == 0) {
      return KeyGeneratorError::KEY_DERIVATION_FAILED;
    }
    return public_key;
  }

  outcome::result<Signature> Secp256k1ProviderImpl::sign(
      BytesIn message, const PrivateKey &key) const {
    OUTCOME_TRY(ffi_sig, signRaw(sha256(message).value(), key));
    uint8_t empty = 0;
    size_t size = 0;
    secp256k1_ecdsa_signature_serialize_der(
        ctx_.get(), &empty, &size, &ffi_sig);
    Signature signature;
    signature.resize(size);
    if (secp256k1_ecdsa_signature_serialize_der(
            ctx_.get(), signature.data(), &size, &ffi_sig)
        == 0) {
      return CryptoProviderError::SIGNATURE_GENERATION_FAILED;
    }
    return signature;
  }

  outcome::result<SignatureCompact> Secp256k1ProviderImpl::signCompact(
      const Prehashed &prehashed, const PrivateKey &key) const {
    OUTCOME_TRY(ffi_sig, signRaw(prehashed, key));
    SignatureCompact signature;
    secp256k1_ecdsa_signature_serialize_compact(
        ctx_.get(), signature.data(), &ffi_sig);
    return signature;
  }

  outcome::result<bool> Secp256k1ProviderImpl::verify(
      BytesIn message, const Signature &signature, const PublicKey &key) const {
    secp256k1_ecdsa_signature ffi_sig;
    if (secp256k1_ecdsa_signature_parse_der(
            ctx_.get(), &ffi_sig, signature.data(), signature.size())
        == 0) {
      return CryptoProviderError::SIGNATURE_VERIFICATION_FAILED;
    }
    return verifyRaw(sha256(message).value(), ffi_sig, key);
  }

  outcome::result<bool> Secp256k1ProviderImpl::verifyCompact(
      const Prehashed &prehashed,
      const SignatureCompact &signature,
      const PublicKey &key) const {
    secp256k1_ecdsa_signature ffi_sig;
    if (secp256k1_ecdsa_signature_parse_compact(
            ctx_.get(), &ffi_sig, signature.data())
        == 0) {
      return CryptoProviderError::SIGNATURE_VERIFICATION_FAILED;
    }
    return verifyRaw(prehashed, ffi_sig, key);
  }

  outcome::result<secp256k1_ecdsa_signature> Secp256k1ProviderImpl::signRaw(
      const Prehashed &prehashed, const PrivateKey &key) const {
    secp256k1_ecdsa_signature ffi_sig;
    if (secp256k1_ecdsa_sign(ctx_.get(),
                             &ffi_sig,
                             prehashed.data(),
                             key.data(),
                             secp256k1_nonce_function_rfc6979,
                             nullptr)
        == 0) {
      return CryptoProviderError::SIGNATURE_GENERATION_FAILED;
    }
    return ffi_sig;
  }

  outcome::result<bool> Secp256k1ProviderImpl::verifyRaw(
      const Prehashed &prehashed,
      const secp256k1_ecdsa_signature &ffi_sig,
      const PublicKey &key) const {
    secp256k1_pubkey ffi_pub;
    if (secp256k1_ec_pubkey_parse(ctx_.get(), &ffi_pub, key.data(), key.size())
        == 0) {
      return CryptoProviderError::SIGNATURE_VERIFICATION_FAILED;
    }
    return secp256k1_ecdsa_verify(
               ctx_.get(), &ffi_sig, prehashed.data(), &ffi_pub)
        == 1;
  }
}  // namespace libp2p::crypto::secp256k1
