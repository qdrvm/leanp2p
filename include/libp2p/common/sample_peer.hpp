/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/crypto/ed25519_provider/ed25519_provider_impl.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/multi/multiaddress.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <qtils/bytes.hpp>

namespace libp2p {
  struct SamplePeer {
    SamplePeer(crypto::KeyPair keypair,
               PeerId peer_id,
               Multiaddress listen,
               Multiaddress connect)
        : keypair{keypair},
          peer_id{peer_id},
          listen{listen},
          connect{connect} {}

    SamplePeer(uint32_t index)
        : SamplePeer{[index] {
            auto port = 10000 + index;
            crypto::ed25519::Ed25519ProviderImpl ed25519;
            crypto::ed25519::PrivateKey private_key;
            for (size_t i = 0; i < private_key.size(); ++i) {
              private_key.at(i) = i + index;
            }
            auto public_key = ed25519.derive(private_key).value();
            crypto::KeyPair keypair{
                crypto::PublicKey{{
                    crypto::Key::Type::Ed25519,
                    qtils::asVec(public_key),
                }},
                crypto::PrivateKey{{
                    crypto::Key::Type::Ed25519,
                    qtils::asVec(private_key),
                }},
            };
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
            return SamplePeer{keypair, peer_id, listen, connect};
          }()} {}

    crypto::KeyPair keypair;
    PeerId peer_id;
    Multiaddress listen;
    Multiaddress connect;
  };
}  // namespace libp2p
