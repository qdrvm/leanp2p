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
    static constexpr auto Ed25519 = crypto::Key::Type::Ed25519;
    static constexpr auto Secp256k1 = crypto::Key::Type::Secp256k1;

    static crypto::KeyPair sampleKeypair(size_t index,
                                         crypto::Key::Type key_type) {
      crypto::ed25519::PrivateKey private_key;
      std::array<uint64_t, sizeof(private_key) / sizeof(uint64_t)> seed64;
      static_assert(sizeof(seed64) >= sizeof(private_key));
      for (size_t i = 0; i < seed64.size(); ++i) {
        seed64.at(i) = i + index;
      }
      memcpy(private_key.data(), seed64.data(), private_key.size());
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
      return keypair;
    }

    static uint16_t samplePort(size_t index) {
      return 10000 + index;
    }

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

    SamplePeer(size_t index,
               std::string_view ip,
               uint16_t port,
               crypto::KeyPair keypair)
        : SamplePeer{[&] {
            auto peer_id =
                peer::IdentityManager{
                    keypair,
                    std::make_shared<crypto::marshaller::KeyMarshaller>(
                        nullptr)}
                    .getId();
            auto listen = Multiaddress::create(
                              std::format("/ip4/0.0.0.0/udp/{}/quic-v1", port))
                              .value();
            auto connect = Multiaddress::create(
                               std::format("/ip4/{}/udp/{}/quic-v1/p2p/{}",
                                           ip,
                                           port,
                                           peer_id.toBase58()))
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

    SamplePeer(size_t index,
               std::string_view ip,
               uint16_t port,
               crypto::Key::Type key_type)
        : SamplePeer{index, ip, port, sampleKeypair(index, key_type)} {}

    SamplePeer(size_t index, crypto::Key::Type key_type)
        : SamplePeer{
              index,
              "127.0.0.1",
              samplePort(index),
              key_type,
          } {}

    static SamplePeer makeEd25519(size_t index) {
      return SamplePeer{index, Ed25519};
    }

    static SamplePeer makeSecp256k1(size_t index) {
      return SamplePeer{index, Secp256k1};
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
