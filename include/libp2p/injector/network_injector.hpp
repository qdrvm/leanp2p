/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/di.hpp>
#include <memory>

// Core interfaces
#include <libp2p/crypto/crypto_provider.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/crypto/random_generator.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/network/connection_manager.hpp>
#include <libp2p/network/dialer.hpp>
#include <libp2p/network/listener_manager.hpp>
#include <libp2p/network/transport_manager.hpp>
#include <libp2p/peer/address_repository.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <libp2p/peer/key_repository.hpp>
#include <libp2p/peer/protocol_repository.hpp>

// Available implementations
#include <libp2p/crypto/key_marshaller/key_marshaller_impl.hpp>

namespace libp2p::injector {

  /**
   * @brief Instruct injector to use this keypair. Can be used once.
   */
  inline auto useKeyPair(crypto::KeyPair key_pair) {
    return boost::di::bind<crypto::KeyPair>().to(
        std::move(key_pair))[boost::di::override];
  }

  /**
   * @brief Main function that creates Network Injector.
   * This creates all the network-level dependencies needed for a libp2p host.
   * Note: Some implementations need to be provided by the user until they are implemented.
   * @tparam InjectorConfig configuration for the injector
   * @tparam Ts types of injector bindings
   * @param args injector bindings that override default bindings
   * @return complete network injector
   */
  template <typename InjectorConfig = BOOST_DI_CFG, typename... Ts>
  inline auto makeNetworkInjector(Ts &&...args) {
    namespace di = boost::di;

    // clang-format off
    return di::make_injector<InjectorConfig>(
        // Available implementations
        di::bind<crypto::marshaller::KeyMarshaller>().to<crypto::marshaller::KeyMarshallerImpl>(),

        // Note: Logger binding removed due to Boost.DI constraints with shared_ptr types
        // Users can create loggers directly using log::createLogger("tag") when needed

        // Placeholders for implementations that need to be provided by user or implemented
        // These would be overridden by the user with actual implementations

        // Note: The following components need implementations to be provided:
        // - crypto::random::RandomGenerator
        // - crypto::CryptoProvider
        // - crypto::KeyPair (can be overridden with useKeyPair())
        // - event::Bus
        // - peer::IdentityManager
        // - peer::AddressRepository
        // - peer::KeyRepository
        // - peer::ProtocolRepository
        // - network::TransportManager
        // - network::ConnectionManager
        // - network::ListenerManager
        // - network::Dialer

        // User-defined overrides and implementations
        std::forward<decltype(args)>(args)...
    );
    // clang-format on
  }

}  // namespace libp2p::injector
