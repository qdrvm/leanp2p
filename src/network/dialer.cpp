/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <fmt/ranges.h>

#include <libp2p/network/dialer.hpp>

#include <boost/asio/async_result.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <libp2p/connection/stream.hpp>
#include <libp2p/log/logger.hpp>

OUTCOME_CPP_DEFINE_CATEGORY(libp2p::network, Dialer::Error, e) {
  using E = libp2p::network::Dialer::Error;
  switch (e) {
    case E::ANOTHER_DIAL_IN_PROGRESS:
      return "Another dial to the peer is in progress";
    case E::STATE_INCONSISTENCY:
      return "State inconsistency";
    case E::NO_DESTINATION_ADDRESS:
      return "No destination address";
    case E::ADDRESS_FAMILY_NOT_SUPPORTED:
      return "Address family not supported";
    case E::HOST_UNREACHABLE:
      return "Host unreachable";
  }
  return "unknown error";
}

namespace libp2p::network {

  CoroOutcome<std::shared_ptr<connection::CapableConnection>> Dialer::dial(
      const PeerInfo &p) {
    SL_TRACE(log_, "Dialing to {}", p.id.toBase58().substr(46));
    if (auto c = cmgr_->getBestConnectionForPeer(p.id); c != nullptr) {
      // we have connection to this peer
      SL_TRACE(
          log_, "Reusing connection to peer {}", p.id.toBase58().substr(46));
      co_return c;
    }
    if (auto ctx = dialing_peers_.find(p.id); dialing_peers_.end() != ctx) {
      SL_TRACE(log_,
               "Dialing to {} is already in progress",
               p.id.toBase58().substr(46));
      // populate known addresses for in-progress dial if any new appear
      for (const auto &addr : p.addresses) {
        if (ctx->second.addr_seen.emplace(addr).second) {
          ctx->second.addr_queue.emplace_back(addr);
        }
      }
      co_return Error::ANOTHER_DIAL_IN_PROGRESS;
    }

    // we don't have a connection to this peer.
    // did user supply its addresses in {@param p}?
    if (p.addresses.empty()) {
      // we don't have addresses of peer p
      co_return Error::NO_DESTINATION_ADDRESS;
    }

    DialCtx new_ctx{
        .addr_queue = {p.addresses.begin(), p.addresses.end()},
        .addr_seen = {p.addresses.begin(), p.addresses.end()},
    };
    bool scheduled = dialing_peers_.emplace(p.id, std::move(new_ctx)).second;
    BOOST_ASSERT(scheduled);
    co_return co_await rotate(p.id);
  }

  CoroOutcome<std::shared_ptr<connection::CapableConnection>> Dialer::rotate(
      const peer::PeerId &peer_id) {
    auto ctx_found = dialing_peers_.find(peer_id);
    if (dialing_peers_.end() == ctx_found) {
      SL_ERROR(
          log_, "State inconsistency - cannot dial {}", peer_id.toBase58());
      co_return Error::STATE_INCONSISTENCY;
    }
    auto &&ctx = ctx_found->second;
    if (ctx.addr_queue.empty()) {
      if (not ctx.dialled) {
        completeDial(peer_id);
        co_return Error::ADDRESS_FAMILY_NOT_SUPPORTED;
      }
      if (ctx.result.has_value()) {
        completeDial(peer_id);
        co_return ctx.result.value();
      }
      // this would never happen. Previous if-statement should work instead'
      completeDial(peer_id);
      co_return Error::HOST_UNREACHABLE;
    }
    auto addr = ctx.addr_queue.front();
    ctx.addr_queue.pop_front();

    if (auto tr = tmgr_->findBest(addr); nullptr != tr) {
      ctx.dialled = true;
      SL_TRACE(log_,
               "Dial to {} via {}",
               peer_id.toBase58().substr(46),
               addr.getStringAddress());
      outcome::result<std::shared_ptr<connection::CapableConnection>> result =
          co_await tr->dial(peer_id, addr);

      auto ctx_found = dialing_peers_.find(peer_id);
      if (dialing_peers_.end() == ctx_found) {
        SL_ERROR(log_,
                 "State inconsistency - uninteresting dial result for peer {}",
                 peer_id.toBase58());
        if (result.has_value() and not result.value()->isClosed()) {
          auto close_res = result.value()->close();
          BOOST_ASSERT(close_res);
        }
        co_return Error::STATE_INCONSISTENCY;
      }

      if (result.has_value()) {
        listener_->onConnection(result);
        completeDial(peer_id);
        co_return result;
      }

      addr_repo_->dialFailed(peer_id, addr);
      // store an error otherwise and reschedule one more rotate
      ctx_found->second.result = std::move(result);
      co_return co_await rotate(peer_id);
    } else {
      co_return co_await rotate(peer_id);
    }
  }

  void Dialer::completeDial(const peer::PeerId &peer_id) {
    if (auto ctx_found = dialing_peers_.find(peer_id);
        dialing_peers_.end() != ctx_found) {
      dialing_peers_.erase(ctx_found);
    }
  }

  CoroOutcome<std::shared_ptr<connection::Stream>> Dialer::newStream(
      std::shared_ptr<connection::CapableConnection> conn,
      StreamProtocols protocols) {
    BOOST_OUTCOME_CO_TRY(auto stream, conn->newStream());
    BOOST_OUTCOME_CO_TRY(
        co_await multiselect_->selectOneOf(protocols, stream, true, true));
    co_return stream;
  }

  CoroOutcome<std::shared_ptr<connection::Stream>> Dialer::newStream(
      const peer::PeerInfo &p, StreamProtocols protocols) {
    SL_TRACE(log_,
             "New stream to {} for {} (peer info)",
             p.id.toBase58().substr(46),
             fmt::join(protocols, " "));
    BOOST_OUTCOME_CO_TRY(auto conn, co_await dial(p));
    co_return co_await newStream(conn, protocols);
  }

  Dialer::Dialer(std::shared_ptr<protocol_muxer::ProtocolMuxer> multiselect,
                 std::shared_ptr<TransportManager> tmgr,
                 std::shared_ptr<ConnectionManager> cmgr,
                 std::shared_ptr<ListenerManager> listener,
                 std::shared_ptr<peer::AddressRepository> addr_repo
                 // std::shared_ptr<basic::Scheduler> scheduler
                 )
      : multiselect_(std::move(multiselect)),
        tmgr_{std::move(tmgr)},
        cmgr_{std::move(cmgr)},
        listener_{std::move(listener)},
        addr_repo_{std::move(addr_repo)},
        // scheduler_{std::move(scheduler)},
        log_{log::createLogger("Dialer")} {
    BOOST_ASSERT(multiselect_ != nullptr);
    BOOST_ASSERT(tmgr_ != nullptr);
    BOOST_ASSERT(cmgr_ != nullptr);
    BOOST_ASSERT(listener_ != nullptr);
    // BOOST_ASSERT(scheduler_ != nullptr);
    BOOST_ASSERT(log_ != nullptr);
  }

}  // namespace libp2p::network
