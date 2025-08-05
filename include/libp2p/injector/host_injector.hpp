/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/di.hpp>
// #include <libp2p/injector/network_injector.hpp>

// implementations
#include <libp2p/crypto/crypto_provider/crypto_provider_impl.hpp>
#include <libp2p/crypto/ecdsa_provider/ecdsa_provider_impl.hpp>
#include <libp2p/crypto/ed25519_provider/ed25519_provider_impl.hpp>
#include <libp2p/crypto/hmac_provider/hmac_provider_impl.hpp>
#include <libp2p/crypto/rsa_provider/rsa_provider_impl.hpp>
#include <libp2p/crypto/secp256k1_provider/secp256k1_provider_impl.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/peer/address_repository/inmem_address_repository.hpp>
#include <libp2p/peer/key_repository/inmem_key_repository.hpp>
#include <libp2p/peer/peer_repository.hpp>
#include <libp2p/peer/protocol_repository/inmem_protocol_repository.hpp>
#include <libp2p/protocol_muxer/multiselect.hpp>
#include "libp2p/crypto/key_validator/key_validator_impl.hpp"
#include "libp2p/crypto/random_generator/boost_generator.hpp"

namespace libp2p::injector {

  /**
   * @brief Configure the libp2p client version string
   */
  inline auto useLibp2pClientVersion(host::Libp2pClientVersion version) {
    return boost::di::bind<host::Libp2pClientVersion>().to(
        std::move(version))[boost::di::override];
  }

  template <typename... TransportImpl>
  inline auto useTransportAdaptors() {
    return boost::di::bind<transport::TransportAdaptor *[]>()  // NOLINT
        .to<TransportImpl...>()[boost::di::override];
  }

  /**
   * @brief Instruct injector to use this keypair. Can be used once.
   */
  inline auto useKeyPair(crypto::KeyPair key_pair) {
    return boost::di::bind<crypto::KeyPair>().to(
        std::move(key_pair))[boost::di::override];
  }

  /**
   * @brief Main function that creates Host Injector.
   * This creates a complete BasicHost with all network dependencies.
   *
   * Usage example:
   * @code
   * // You need to provide implementations for the required components
   * auto injector = makeHostInjector(
   *     boost::di::bind<crypto::random::RandomGenerator>().to<YourRandomGeneratorImpl>(),
   *     boost::di::bind<crypto::CryptoProvider>().to<YourCryptoProviderImpl>(),
   *     boost::di::bind<event::Bus>().to<YourEventBusImpl>(),
   *     boost::di::bind<peer::IdentityManager>().to<YourIdentityManagerImpl>(),
   *     // ... other required implementations
   *     useLibp2pClientVersion({"my-custom-client"})
   * );
   * auto host = injector.create<std::shared_ptr<host::BasicHost>>();
   * @endcode
   *
   * @tparam InjectorConfig configuration for the injector
   * @tparam Ts types of injector bindings
   * @param args injector bindings that override default bindings
   * @return complete host injector
   */
  template <typename InjectorConfig = BOOST_DI_CFG, typename... Ts>
  inline auto makeHostInjector(Ts &&...args) {
    namespace di = boost::di;

    // clang-format off
    return di::make_injector<InjectorConfig>(
        // Include all network-level dependencies
        // makeNetworkInjector<InjectorConfig>(),

        // Default client version
        di::bind<host::Libp2pClientVersion>().to(host::Libp2pClientVersion{"cpp-libp2p-mini"}),

        di::bind<peer::KeyRepository>.to<peer::InmemKeyRepository>(),
        di::bind<peer::AddressRepository>.to<peer::InmemAddressRepository>(),
        di::bind<peer::ProtocolRepository>.to<peer::InmemProtocolRepository>(),
        di::bind<protocol_muxer::ProtocolMuxer>.to<protocol_muxer::multiselect::Multiselect>(),
        di::bind<crypto::validator::KeyValidator>.to<crypto::validator::KeyValidatorImpl>(),
        di::bind<crypto::CryptoProvider>.to<crypto::CryptoProviderImpl>(),
        di::bind<crypto::ed25519::Ed25519Provider>.to<crypto::ed25519::Ed25519ProviderImpl>(),
        di::bind<crypto::rsa::RsaProvider>.to<crypto::rsa::RsaProviderImpl>(),
        di::bind<crypto::ecdsa::EcdsaProvider>.to<crypto::ecdsa::EcdsaProviderImpl>(),
        di::bind<crypto::secp256k1::Secp256k1Provider>.to<crypto::secp256k1::Secp256k1ProviderImpl>(),
        di::bind<crypto::hmac::HmacProvider>.to<crypto::hmac::HmacProviderImpl>(),
        di::bind<libp2p::crypto::random::CSPRNG>.to<libp2p::crypto::random::BoostRandomGenerator>(),
        // User-defined overrides...
        std::forward<decltype(args)>(args)...
    );
    // clang-format on
  }

}  // namespace libp2p::injector
