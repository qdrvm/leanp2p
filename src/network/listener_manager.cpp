/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/network/listener_manager.hpp>

#include <libp2p/log/logger.hpp>

OUTCOME_CPP_DEFINE_CATEGORY(libp2p::network, ListenerManager::Error, e) {
  using E = libp2p::network::ListenerManager::Error;
  switch (e) {
    case E::NO_HANDLER_FOUND:
      return "no handler was found for a given protocol";
  }
  return "unknown error";
}

namespace libp2p::network {

  namespace {
    log::Logger log() {
      static log::Logger logger = log::createLogger("ListenerManager");
      return logger;
    }
  }  // namespace

  ListenerManager::ListenerManager(
      std::shared_ptr<boost::asio::io_context> io_context,
      std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect,
      // std::shared_ptr<network::Router> router,
      std::shared_ptr<TransportManager> tmgr,
      std::shared_ptr<ConnectionManager> cmgr)
      : io_context_(io_context),
        multiselect_(std::move(multiselect)),
        // router_(std::move(router)),
        tmgr_(std::move(tmgr)),
        cmgr_(std::move(cmgr)) {
    BOOST_ASSERT(multiselect_ != nullptr);
    // BOOST_ASSERT(router_ != nullptr);
    BOOST_ASSERT(tmgr_ != nullptr);
    BOOST_ASSERT(cmgr_ != nullptr);
  }

  bool ListenerManager::isStarted() const {
    return started;
  }

  outcome::result<void> ListenerManager::closeListener(
      const multi::Multiaddress &ma) {
    // we can find multiaddress directly
    auto it = listeners_.find(ma);
    if (it != listeners_.end()) {
      auto listener = it->second;
      listeners_.erase(it);
      if (!listener->isClosed()) {
        return listener->close();
      }

      return outcome::success();
    }

    // we did not find direct multiaddress
    // lets try to search across interface addresses
    for (auto &&entry : listeners_) {
      auto r = entry.second->getListenMultiaddr();
      if (!r) {
        // ignore error
        continue;
      }

      auto &&addr = r.value();
      if (addr == ma) {
        // found. close listener.
        auto listener = entry.second;
        listeners_.erase(it);
        if (!listener->isClosed()) {
          return listener->close();
        }

        return outcome::success();
      }
    }

    return std::errc::invalid_argument;
  }

  outcome::result<void> ListenerManager::removeListener(
      const multi::Multiaddress &ma) {
    auto it = listeners_.find(ma);
    if (it != listeners_.end()) {
      listeners_.erase(it);
      return outcome::success();
    }

    return std::errc::invalid_argument;
  };

  // starts listening on all provided multiaddresses
  void ListenerManager::start() {
    if (started) {
      return;
    }

    auto begin = listeners_.begin();
    auto end = listeners_.end();
    for (auto it = begin; it != end;) {
      auto r = it->second->listen(it->first);
      if (!r) {
        // can not start listening on this multiaddr, remove listener
        it = listeners_.erase(it);
      } else {
        ++it;
      }
    }

    started = true;
  }

  // stops listening on all multiaddresses
  void ListenerManager::stop() {
    if (!started) {
      return;
    }

    auto begin = listeners_.begin();
    auto end = listeners_.end();
    for (auto it = begin; it != end;) {
      auto r = it->second->close();
      if (!r) {
        // error while stopping listener, remove it
        it = listeners_.erase(it);
      } else {
        ++it;
      }
    }

    started = false;
  }

  outcome::result<void> ListenerManager::listen(const multi::Multiaddress &ma) {
    auto tr = this->tmgr_->findBest(ma);
    if (tr == nullptr) {
      // can not listen on this address
      return std::errc::address_family_not_supported;
    }

    auto it = listeners_.find(ma);
    if (it != listeners_.end()) {
      // this address is already used
      return std::errc::address_in_use;
    }

    // auto listener = tr->createListener(
    //     [this](auto &&r) { this->onConnection(std::forward<decltype(r)>(r));
    //     });
    auto listener = tr->createListener();
    boost::asio::co_spawn(
        *io_context_,
        [this, listener]() mutable -> boost::asio::awaitable<void> {
          while (auto item = co_await listener->asyncAccept()) {
            this->onConnection(std::move(item));
          }
        },
        boost::asio::detached);

    listeners_.insert({ma, std::move(listener)});

    return outcome::success();
  }

  outcome::result<void> ListenerManager::listenProtocol(
      const peer::ProtocolName &name,
      std::shared_ptr<protocol::BaseProtocol> protocol) {
    protocols_[name] = std::move(protocol);
    return outcome::success();
  }

  std::vector<peer::ProtocolName> ListenerManager::getSupportedProtocols()
      const {
    std::vector<peer::ProtocolName> protos;
    if (protocols_.empty()) {
      return protos;
    }

    std::string key_buffer;  // a workaround, recommended by the library's devs
    protos.reserve(protocols_.size());
    for (auto it = protocols_.begin(); it != protocols_.end(); ++it) {
      it.key(key_buffer);
      protos.push_back(std::move(key_buffer));
    }
    return protos;
  }

  std::vector<multi::Multiaddress> ListenerManager::getListenAddresses() const {
    std::vector<multi::Multiaddress> mas;
    mas.reserve(listeners_.size());

    for (auto &&e : listeners_) {
      mas.push_back(e.first);
    }

    return mas;
  }

  std::vector<multi::Multiaddress>
  ListenerManager::getListenAddressesInterfaces() const {
    std::vector<multi::Multiaddress> mas;
    mas.reserve(listeners_.size());

    for (auto &&e : listeners_) {
      auto addr = e.second->getListenMultiaddr();
      // ignore failed sockets
      if (addr) {
        mas.push_back(std::move(addr.value()));
      }
    }

    return mas;
  }

  void ListenerManager::onConnection(
      outcome::result<std::shared_ptr<connection::CapableConnection>> rconn) {
    if (!rconn) {
      log()->warn("can not accept valid connection, {}", rconn.error());
      return;  // ignore
    }
    auto &&conn = rconn.value();

    auto rid = conn->remotePeer();
    if (!rid) {
      log()->warn("can not get remote peer id, {}", rid.error());
      return;  // ignore
    }
    auto &&id = rid.value();

    boost::asio::co_spawn(
        *io_context_,
        [this, id, conn]() mutable -> boost::asio::awaitable<void> {
          while (not conn->isClosed()) {
            auto rstream = co_await conn->acceptStream();
            if (!rstream) {
              // connection was closed or had some error
              log()->warn("can not accept stream, {}", rstream.error());
              continue;  // ignore
            }
            auto &&stream = rstream.value();
            auto protocols = this->getSupportedProtocols();
            if (protocols.empty()) {
              log()->warn("no protocols are served, resetting inbound stream");
              stream->reset();
              continue;  // ignore
            }
            // negotiate protocols
            outcome::result<peer::ProtocolName> rproto =
                co_await this->multiselect_->selectOneOf(
                    protocols, stream, false, true);

            bool success = true;

            if (!rproto) {
              log()->warn("can not negotiate protocols, {}", rproto.error());
              success = false;
            } else {
              auto &&proto_name = rproto.value();
              outcome::result<std::shared_ptr<protocol::BaseProtocol>>
                  rprotocol = this->getProtocol(proto_name);
              if (!rprotocol) {
                log()->warn("can not negotiate protocols, {}",
                            rprotocol.error());
                success = false;
              }
              const auto &protocol = rprotocol.value();
              protocol->handle(stream);
            }

            if (!success) {
              stream->reset();
            }
          }
          // connection was closed, notify connection manager
          this->cmgr_->onConnectionClosed(id, conn);
        },
        boost::asio::detached);

    // store connection
    this->cmgr_->addConnectionToPeer(id, conn);
  }

  outcome::result<std::shared_ptr<protocol::BaseProtocol>>
  ListenerManager::getProtocol(const peer::ProtocolName &p) const {
    // firstly, try to find the longest prefix - even if it's not perfect match,
    // but a predicate one, it still will save the resources
    auto matched_proto = protocols_.longest_prefix(p);
    if (matched_proto == protocols_.end()) {
      return Error::NO_HANDLER_FOUND;
    }

    std::shared_ptr<protocol::BaseProtocol> protocol = matched_proto.value();
    if (matched_proto.key() == p) {
      return protocol;
    }

    // fallback: find all matches for the first two letters of the given (the
    // first letter is a '/', so we need two) protocol and test them against the
    // predicate; the longest match is to be called
    auto matched_protos = protocols_.equal_prefix_range_ks(p.data(), 2);

    auto longest_match{matched_protos.second};
    for (auto match = matched_protos.first; match != matched_protos.second;
         ++match) {
      if (longest_match == matched_protos.second
          or match.key().size() > longest_match.key().size()) {
        longest_match = match;
      }
    }

    if (longest_match == matched_protos.second) {
      return Error::NO_HANDLER_FOUND;
    }
    return longest_match.value<>();
  }

}  // namespace libp2p::network
