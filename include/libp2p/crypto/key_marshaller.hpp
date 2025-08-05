/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/crypto/error.hpp>
#include <libp2p/crypto/key.hpp>
#include <libp2p/crypto/key_validator.hpp>
#include <libp2p/crypto/protobuf/protobuf_key.hpp>

namespace libp2p::crypto::marshaller {
  /**
   * @class KeyMarshaller provides methods for serializing and deserializing
   * private and public keys from/to google-protobuf format
   */
  class KeyMarshaller {
   public:
    explicit KeyMarshaller(
        std::shared_ptr<validator::KeyValidator> key_validator);
    /**
     * Convert the public key into Protobuf representation
     * @param key - public key to be mashalled
     * @return bytes of Protobuf object if marshalling was successful, error
     * otherwise
     */
    outcome::result<ProtobufKey> marshal(const PublicKey &key) const;

    /**
     * Convert the private key into Protobuf representation
     * @param key - public key to be mashalled
     * @return bytes of Protobuf object if marshalling was successful, error
     * otherwise
     */
    outcome::result<ProtobufKey> marshal(const PrivateKey &key) const;
    /**
     * Convert Protobuf representation of public key into the object
     * @param key_bytes - bytes of the public key
     * @return public key in case of success, error otherwise
     */
    outcome::result<PublicKey> unmarshalPublicKey(const ProtobufKey &key) const;

    /**
     * Convert Protobuf representation of private key into the object
     * @param key_bytes - bytes of the private key
     * @return private key in case of success, error otherwise
     */
    outcome::result<PrivateKey> unmarshalPrivateKey(
        const ProtobufKey &key) const;

   private:
    std::shared_ptr<validator::KeyValidator> key_validator_;
  };
}  // namespace libp2p::crypto::marshaller
