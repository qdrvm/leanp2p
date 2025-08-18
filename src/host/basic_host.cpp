/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/host/basic_host.hpp>

#include <boost/assert.hpp>

#include <libp2p/crypto/key_marshaller.hpp>
#include <utility>

namespace libp2p::host {

  BasicHost::BasicHost(
      std::shared_ptr<peer::IdentityManager> idmgr,
      std::shared_ptr<network::ListenerManager> listener,
      std::shared_ptr<network::ConnectionManager> connection_manager,
      std::shared_ptr<network::Dialer> dialer,
      // std::shared_ptr<network::Network> network,
      std::shared_ptr<peer::PeerRepository> repo,
      std::shared_ptr<event::Bus> bus,
      std::shared_ptr<network::TransportManager> transport_manager,
      Libp2pClientVersion libp2p_client_version)
      : idmgr_{std::move(idmgr)},
        listener_{std::move(listener)},
        connection_manager_{std::move(connection_manager)},
        dialer_{std::move(dialer)},
        // network_{std::move(network)},
        repo_{std::move(repo)},
        bus_{std::move(bus)},
        transport_manager_{std::move(transport_manager)},
        libp2p_client_version_{std::move(libp2p_client_version)} {
    BOOST_ASSERT(idmgr_ != nullptr);
    // BOOST_ASSERT(network_ != nullptr);
    BOOST_ASSERT(listener_ != nullptr);
    BOOST_ASSERT(connection_manager_ != nullptr);
    BOOST_ASSERT(dialer_ != nullptr);
    BOOST_ASSERT(repo_ != nullptr);
    BOOST_ASSERT(bus_ != nullptr);
    BOOST_ASSERT(transport_manager_ != nullptr);
  }

  std::string_view BasicHost::getLibp2pVersion() const {
    return "0.0.0";
  }

  std::string_view BasicHost::getLibp2pClientVersion() const {
    return libp2p_client_version_.version;
  }

  peer::PeerId BasicHost::getId() const {
    return idmgr_->getId();
  }

  peer::PeerInfo BasicHost::getPeerInfo() const {
    auto addresses = getAddresses();
    auto observed = getObservedAddresses();
    auto interfaces = getAddressesInterfaces();

    std::set<multi::Multiaddress> unique_addresses;
    unique_addresses.insert(std::make_move_iterator(addresses.begin()),
                            std::make_move_iterator(addresses.end()));
    unique_addresses.insert(std::make_move_iterator(interfaces.begin()),
                            std::make_move_iterator(interfaces.end()));
    unique_addresses.insert(std::make_move_iterator(observed.begin()),
                            std::make_move_iterator(observed.end()));

    // TODO(xDimon): Needs to filter special interfaces (e.g. INADDR_ANY, etc.)
    for (auto i = unique_addresses.begin(); i != unique_addresses.end();) {
      bool is_good_addr = true;
      for (auto &pv : i->getProtocolsWithValues()) {
        if (pv.first.code == multi::Protocol::Code::IP4) {
          if (pv.second == "0.0.0.0") {
            is_good_addr = false;
            break;
          }
        } else if (pv.first.code == multi::Protocol::Code::IP6) {
          if (pv.second == "::") {
            is_good_addr = false;
            break;
          }
        }
      }
      if (not is_good_addr) {
        i = unique_addresses.erase(i);
      } else {
        ++i;
      }
    }

    std::vector<multi::Multiaddress> unique_addr_list(
        std::make_move_iterator(unique_addresses.begin()),
        std::make_move_iterator(unique_addresses.end()));

    return {getId(), std::move(unique_addr_list)};
  }

  std::vector<multi::Multiaddress> BasicHost::getAddresses() const {
    return listener_->getListenAddresses();
  }

  std::vector<multi::Multiaddress> BasicHost::getAddressesInterfaces() const {
    return listener_->getListenAddressesInterfaces();
  }

  std::vector<multi::Multiaddress> BasicHost::getObservedAddresses() const {
    auto r = repo_->getAddressRepository().getAddresses(getId());
    if (r) {
      return r.value();
    }

    // we don't know our addresses
    return {};
  }

  BasicHost::Connectedness BasicHost::connectedness(
      const peer::PeerInfo &p) const {
    auto conn = connection_manager_->getBestConnectionForPeer(p.id);
    if (conn != nullptr) {
      return Connectedness::CONNECTED;
    }

    // for each address, try to find transport to dial
    for (auto &&ma : p.addresses) {
      if (auto tr = transport_manager_->findBest(ma); tr != nullptr) {
        // we can dial to the peer
        return Connectedness::CAN_CONNECT;
      }
    }

    auto res = repo_->getAddressRepository().getAddresses(p.id);
    if (res.has_value()) {
      for (auto &&ma : res.value()) {
        if (auto tr = transport_manager_->findBest(ma); tr != nullptr) {
          // we can dial to the peer
          return Connectedness::CAN_CONNECT;
        }
      }
    }

    // we did not find available transports to dial
    return Connectedness::CAN_NOT_CONNECT;
  }

  void BasicHost::listenProtocol(
      std::shared_ptr<protocol::BaseProtocol> protocol) {
    return listener_->listenProtocol(std::move(protocol));
  }

  // void BasicHost::setProtocolHandler(StreamProtocols protocols,
  //                                    StreamAndProtocolCb cb,
  //                                    ProtocolPredicate predicate) {
  //   network_->getListener().getRouter().setProtocolHandler(
  //       std::move(protocols), std::move(cb), std::move(predicate));
  // }

  CoroOutcome<StreamAndProtocol> BasicHost::newStream(
      const peer::PeerInfo &peer_info, StreamProtocols protocols) {
    co_return co_await dialer_->newStream(peer_info, std::move(protocols));
  }

  CoroOutcome<StreamAndProtocol> BasicHost::newStream(
      std::shared_ptr<connection::CapableConnection> connection,
      StreamProtocols protocols) {
    co_return co_await dialer_->newStream(connection, std::move(protocols));
  }

  outcome::result<void> BasicHost::listen(const multi::Multiaddress &ma) {
    return listener_->listen(ma);
  }

  outcome::result<void> BasicHost::closeListener(
      const multi::Multiaddress &ma) {
    return listener_->closeListener(ma);
  }

  outcome::result<void> BasicHost::removeListener(
      const multi::Multiaddress &ma) {
    return listener_->removeListener(ma);
  }

  void BasicHost::start() {
    listener_->start();
  }

  event::Handle BasicHost::setOnNewConnectionHandler(
      const NewConnectionHandler &h) const {
    return bus_->getChannel<event::network::OnNewConnectionChannel>().subscribe(
        [h{std::move(h)}](auto &&conn) {
          if (auto connection = conn.lock()) {
            auto remote_peer = connection->remotePeer();

            auto remote_peer_addr_res = connection->remoteMultiaddr();
            if (!remote_peer_addr_res) {
              return;
            }

            if (h != nullptr) {
              h(peer::PeerInfo{std::move(remote_peer),
                               std::vector<multi::Multiaddress>{
                                   std::move(remote_peer_addr_res.value())}});
            }
          }
        });
  }

  void BasicHost::stop() {
    listener_->stop();
  }

  // network::Network &BasicHost::getNetwork() {
  //   return *network_;
  // }

  peer::PeerRepository &BasicHost::getPeerRepository() {
    return *repo_;
  }

  // network::Router &BasicHost::getRouter() {
  //   return network_->getListener().getRouter();
  // }

  event::Bus &BasicHost::getBus() {
    return *bus_;
  }

  CoroOutcome<std::shared_ptr<connection::CapableConnection>>
  BasicHost::connect(const peer::PeerInfo &peer_info) {
    co_return co_await dialer_->dial(peer_info);
  }

  void BasicHost::disconnect(const peer::PeerId &peer_id) {
    connection_manager_->closeConnectionsToPeer(peer_id);
  }

}  // namespace libp2p::host
