/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/protocol/identify.hpp>

#include <generated/protocol/identify/identify.pb.h>
#include <boost/asio/io_context.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/common/protobuf.hpp>
#include <libp2p/common/weak_macro.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/coro/yield.hpp>
#include <libp2p/crypto/key_marshaller.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/peer/identity_manager.hpp>
#include <qtils/bytestr.hpp>

namespace libp2p::protocol {
  Identify::Identify(std::shared_ptr<boost::asio::io_context> io_context,
                     std::shared_ptr<host::BasicHost> host,
                     std::shared_ptr<peer::IdentityManager> id_mgr,
                     IdentifyConfig config)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        id_mgr_{std::move(id_mgr)},
        config_{std::move(config)} {}

  StreamProtocols Identify::getProtocolIds() const {
    return {"/ipfs/id/1.0.0"};
  }

  void Identify::handle(std::shared_ptr<connection::Stream> stream) {
    libp2p_identify_pb::Identify pb_message;
    *pb_message.mutable_protocolversion() = config_.protocol_version;
    *pb_message.mutable_agentversion() = config_.agent_version;
    *pb_message.mutable_publickey() =
        qtils::byte2str(crypto::marshaller::KeyMarshaller{nullptr}
                            .marshal(id_mgr_->getKeyPair().publicKey)
                            .value()
                            .key);
    for (auto &address : config_.listen_addresses) {
      *pb_message.add_listenaddrs() =
          qtils::byte2str(address.getBytesAddress());
    }
    *pb_message.mutable_observedaddr() =
        qtils::byte2str(stream->remoteMultiaddr().value().getBytesAddress());
    for (auto &protocol : host_->getSupportedProtocols()) {
      pb_message.add_protocols(protocol);
    }
    coroSpawn(*io_context_,
              [stream, encoded{protobufEncode(pb_message)}]() -> Coro<void> {
                std::ignore = co_await writeVarintMessage(stream, encoded);
                stream->reset();
              });
  }

  void Identify::start() {
    host_->listenProtocol(shared_from_this());
    auto on_peer_connected =
        [WEAK_SELF](
            std::weak_ptr<connection::CapableConnection> weak_connection) {
          WEAK_LOCK(connection);
          WEAK_LOCK(self);
          coroSpawn(*self->io_context_, [self, connection]() -> Coro<void> {
            co_await self->recv_identify(connection);
          });
        };
    on_peer_connected_sub_ =
        host_->getBus()
            .getChannel<event::network::OnNewConnectionChannel>()
            .subscribe(on_peer_connected);
  }

  Coro<void> Identify::recv_identify(
      std::shared_ptr<connection::CapableConnection> connection) {
    auto peer_id = connection->remotePeer();
    co_await coroYield();
    auto stream_result =
        co_await host_->newStream(connection, getProtocolIds());
    if (not stream_result.has_value()) {
      co_return;
    }
    auto &stream = stream_result.value();
    std::ignore = co_await [&]() -> CoroOutcome<void> {
      Bytes encoded;
      BOOST_OUTCOME_CO_TRY(co_await readVarintMessage(stream, encoded));
      BOOST_OUTCOME_CO_TRY(
          auto pb_message,
          protobufDecode<libp2p_identify_pb::Identify>(encoded));
      BOOST_OUTCOME_CO_TRY(
          auto observed_address,
          Multiaddress::create(qtils::str2byte(pb_message.observedaddr())));
      IdentifyInfo message{
          peer_id,
          pb_message.protocolversion(),
          pb_message.agentversion(),
          {},
          observed_address,
          {},
      };
      for (auto &pb_address : pb_message.listenaddrs()) {
        BOOST_OUTCOME_CO_TRY(auto address,
                             Multiaddress::create(qtils::str2byte(pb_address)));
        message.listen_addresses.emplace_back(address);
      }
      for (auto &protocol : pb_message.protocols()) {
        message.protocols.emplace_back(protocol);
      }

      auto &peer_repo = host_->getPeerRepository();
      peer_repo.getProtocolRepository()
          .addProtocols(peer_id, message.protocols)
          .value();
      auto &address_repo = peer_repo.getAddressRepository();
      address_repo
          .upsertAddresses(
              peer_id, message.listen_addresses, peer::ttl::kRecentlyConnected)
          .value();
      auto &user_agent_repo = peer_repo.getUserAgentRepository();
      user_agent_repo.setUserAgent(peer_id, message.agent_version);

      host_->getBus().getChannel<OnIdentifyChannel>().publish(message);
      co_return outcome::success();
    }();
    stream->reset();
  }
}  // namespace libp2p::protocol
