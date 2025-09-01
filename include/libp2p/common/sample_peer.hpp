/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/crypto/ed25519_provider/ed25519_provider_impl.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/peer/peer_info.hpp>
#include <qtils/bytes.hpp>

namespace libp2p {
  struct SamplePeer {
    SamplePeer(size_t index,
               uint16_t port,
               crypto::KeyPair keypair,
               PeerId peer_id,
               Multiaddress listen,
               Multiaddress connect,
               PeerInfo connect_info)
        : index{index},
          port{port},
          keypair{keypair},
          peer_id{peer_id},
          listen{listen},
          connect{connect},
          connect_info{connect_info} {}

    SamplePeer(size_t index, crypto::Key::Type key_type)
        : SamplePeer{[&] {
            uint16_t port = 10000 + index;
            crypto::ed25519::PrivateKey private_key;
            for (size_t i = 0; i < private_key.size(); ++i) {
              private_key.at(i) = i + index;
            }
            crypto::KeyPair keypair{
                crypto::PublicKey{{key_type, {}}},
                crypto::PrivateKey{{key_type, qtils::ByteVec(private_key)}},
            };
            switch (key_type) {
              case crypto::Key::Type::Ed25519: {
                crypto::ed25519::Ed25519ProviderImpl ed25519;
                keypair.publicKey.data =
                    qtils::ByteVec(ed25519.derive(private_key).value());
                break;
              }
              case crypto::Key::Type::Secp256k1: {
                crypto::secp256k1::Secp256k1ProviderImpl secp256k1{nullptr};
                keypair.publicKey.data =
                    qtils::ByteVec(secp256k1.derive(private_key).value());
                break;
              }
              default: {
                abort();
              }
            }
            auto peer_id =
                peer::IdentityManager{
                    keypair,
                    std::make_shared<crypto::marshaller::KeyMarshaller>(
                        nullptr)}
                    .getId();
            auto listen =
                Multiaddress::create(
                    std::format("/ip4/127.0.0.1/udp/{}/quic-v1", port))
                    .value();
            auto connect =
                Multiaddress::create(
                    std::format("{}/p2p/{}", listen, peer_id.toBase58()))
                    .value();
            return SamplePeer{
                index,
                port,
                keypair,
                peer_id,
                listen,
                connect,
                {peer_id, {connect}},
            };
          }()} {}

    static SamplePeer makeEd25519(size_t index) {
      return SamplePeer{index, crypto::Key::Type::Ed25519};
    }

    static SamplePeer makeSecp256k1(size_t index) {
      return SamplePeer{index, crypto::Key::Type::Secp256k1};
    }

    size_t index;
    uint16_t port;
    crypto::KeyPair keypair;
    PeerId peer_id;
    Multiaddress listen;
    Multiaddress connect;
    PeerInfo connect_info;
  };
}  // namespace libp2p
