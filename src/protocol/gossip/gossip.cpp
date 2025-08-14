/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <generated/protocol/gossip/gossip.pb.h>
#include <boost/asio/io_context.hpp>
#include <boost/endian/conversion.hpp>
#include <libp2p/basic/encode_varint.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/common/protobuf.hpp>
#include <libp2p/common/weak_macro.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/coro/yield.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <qtils/bytes.hpp>
#include <qtils/bytestr.hpp>
#include <qtils/option_take.hpp>


namespace libp2p::protocol::gossip {
  MessageId defaultMessageIdFn(const Message &message) {
    std::string str;
    static auto empty_from = PeerId::fromBytes(Bytes{0, 1, 0}).value();
    str += (message.from.has_value() ? *message.from : empty_from).toBase58();
    str += std::to_string(message.seqno.value_or(0));
    return qtils::asVec(qtils::str2byte(std::string_view{str}));
  }

  void Rpc::subscribe(TopicHash topic_hash, bool subscribe) {
    auto it = subscriptions.emplace(topic_hash, subscribe).first;
    if (it->second != subscribe) {
      subscriptions.erase(it);
    }
  }

  CoroOutcome<Bytes> Topic::receive() {
    co_return co_await receive_channel_.receive();
  }

  void Topic::publish(BytesIn message) {
    if (auto gossip = weak_gossip_.lock()) {
      gossip->publish(*this, message);
    }
  }

  Peer::Peer(PeerId peer_id) : peer_id_{std::move(peer_id)} {}

  Gossip::Gossip(std::shared_ptr<boost::asio::io_context> io_context,
                 std::shared_ptr<host::BasicHost> host,
                 Config config)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        config_{std::move(config)},
        publish_config_{
            .last_seq_no = static_cast<Seqno>(std::chrono::nanoseconds{
                std::chrono::system_clock::now().time_since_epoch()}
                                                  .count()),
        } {
    assert(config_.message_id_fn);
  }

  peer::ProtocolName Gossip::getProtocolId() const {
    abort();
  }

  void Gossip::handle(StreamPtr stream) {
    auto peer_id = stream->remotePeerId();
    auto peer = getPeer(peer_id);
    peer->streams_in_.emplace(stream);
    coroSpawn(*io_context_, [WEAK_SELF, stream, peer]() -> Coro<void> {
      Bytes encoded;
      while (true) {
        auto r = co_await readVarintMessage(stream, encoded);
        if (not r.has_value()) {
          break;
        }
        auto self = weak_self.lock();
        if (not self) {
          break;
        }
        if (not self->onMessage(peer, encoded)) {
          break;
        }
      }
      peer->streams_in_.erase(stream);
    });
  }

  void Gossip::start() {
    for (auto &protocol : config_.protocols) {
      host_->listenProtocol(protocol, shared_from_this());
    }
    auto on_peer =
        [WEAK_SELF](
            std::weak_ptr<connection::CapableConnection> weak_connection) {
          WEAK_LOCK(connection);
          WEAK_LOCK(self);
          auto peer_id = connection->remotePeer();
          auto peer = self->getPeer(peer_id);
          coroSpawn(*self->io_context_,
                    [self, connection, peer]() -> Coro<void> {
                      auto stream_result = (co_await self->host_->newStream(
                          connection, self->config_.protocols));
                      if (not stream_result.has_value()) {
                        // TODO: can't open out stream?
                        co_return;
                      }
                      if (auto stream = qtils::optionTake(peer->stream_out_)) {
                        (**stream).reset();
                      }
                      peer->stream_out_ = stream_result.value();
                      if (not self->topics_.empty()) {
                        auto &message = self->getBatch(peer);
                        for (auto &p : self->topics_) {
                          message.subscribe(p.first, true);
                        }
                      }
                    });
        };
    on_peer_sub_ = host_->getBus()
                       .getChannel<event::network::OnNewConnectionChannel>()
                       .subscribe(on_peer);
  }

  std::shared_ptr<Topic> Gossip::subscribe(TopicHash topic_hash) {
    auto topic_it = topics_.find(topic_hash);
    if (topic_it == topics_.end()) {
      auto topic = std::make_shared<Topic>(
          Topic{weak_from_this(), topic_hash, {*io_context_}});
      topic_it = topics_.emplace(topic_hash, topic).first;
      for (auto &p : peers_) {
        getBatch(p.second).subscribe(topic_hash, true);
      }
      // TODO: mesh
    }
    return topic_it->second;
  }

  std::shared_ptr<Topic> Gossip::subscribe(std::string_view topic_hash) {
    return subscribe(qtils::asVec(qtils::str2byte(topic_hash)));
  }

  void Gossip::publish(Topic &topic, BytesIn data) {
    assert(config_.message_authenticity == MessageAuthenticity::Signed);
    Message message{
        host_->getId(),
        qtils::asVec(data),
        publish_config_.last_seq_no,
        topic.topic_hash_,
    };
    ++publish_config_.last_seq_no;

    auto message_id = config_.message_id_fn(message);
    if (duplicate_cache_.contains(message_id)) {
      return;
    }
    duplicate_cache_.emplace(message_id);
    // TODO: mcache
    broadcast(std::nullopt, message);
  }

  bool Gossip::onMessage(const std::shared_ptr<Peer> &peer, BytesIn encoded) {
    auto pb_message_result = protobufDecode<gossipsub::pb::RPC>(encoded);
    if (not pb_message_result.has_value()) {
      return false;
    }
    auto &pb_message = pb_message_result.value();

    for (auto &pb_subscribe : pb_message.subscriptions()) {
      auto topic_hash = qtils::asVec(qtils::str2byte(pb_subscribe.topic_id()));
      if (pb_subscribe.subscribe()) {
        peer->topics_.emplace(topic_hash);
        // TODO: mesh
      } else {
        peer->topics_.erase(topic_hash);
      }
    }

    for (auto &pb_publish : pb_message.publish()) {
      Message message;

      assert(config_.validation_mode == ValidationMode::Strict);
      auto from_result = PeerId::fromBytes(qtils::str2byte(pb_publish.from()));
      if (not from_result) {
        continue;
      }
      message.from.emplace(from_result.value());

      message.data = qtils::asVec(qtils::str2byte(pb_publish.data()));

      if (pb_publish.seqno().size() != sizeof(Seqno)) {
        continue;
      }
      message.seqno = boost::endian::load_big_u64(
          qtils::str2byte(pb_publish.seqno().data()));

      message.topic = qtils::asVec(qtils::str2byte(pb_publish.topic()));

      // TODO: signature

      auto topic_it = topics_.find(message.topic);
      if (topic_it == topics_.end()) {
        continue;
      }
      auto &topic = topic_it->second;
      auto message_id = config_.message_id_fn(message);
      if (not duplicate_cache_.emplace(message_id).second) {
        continue;
      }
      topic->receive_channel_.send(message.data);
      // TODO: mcache
      broadcast(peer->peer_id_, message);
    }

    return true;
  }

  void Gossip::broadcast(std::optional<PeerId> from, const Message &message) {
    for (auto &[peer_id, peer] : peers_) {
      if (not peer->topics_.contains(message.topic)) {
        continue;
      }
      // TODO: floodsub or mesh peers
      if (from == peer->peer_id_) {
        continue;
      }
      if (message.from == peer->peer_id_) {
        continue;
      }
      getBatch(peer).publish.emplace_back(message);
    }
  }

  std::shared_ptr<Peer> Gossip::getPeer(const PeerId &peer_id) {
    auto peer_it = peers_.find(peer_id);
    if (peer_it == peers_.end()) {
      peer_it = peers_.emplace(peer_id, std::make_shared<Peer>(peer_id)).first;
    }
    return peer_it->second;
  }

  Rpc &Gossip::getBatch(const std::shared_ptr<Peer> &peer) {
    if (not peer->batch_) {
      peer->batch_.emplace();
    }
    checkWrite(peer);
    return peer->batch_.value();
  }

  void Gossip::checkWrite(const std::shared_ptr<Peer> &peer) {
    if (peer->writing_) {
      return;
    }
    if (not peer->stream_out_.has_value()) {
      return;
    }
    if (not peer->batch_.has_value()) {
      return;
    }
    peer->writing_ = true;
    coroSpawn(*io_context_, [peer]() -> Coro<void> {
      co_await coroYield();
      assert(peer->writing_);
      assert(peer->stream_out_.has_value());
      while (auto message = qtils::optionTake(peer->batch_)) {
        gossipsub::pb::RPC pb_message;
        assert(not message->subscriptions.empty()
               or not message->publish.empty());

        for (auto &[topic_hash, subscribe] : message->subscriptions) {
          auto &pb_subscription = *pb_message.add_subscriptions();
          *pb_subscription.mutable_topic_id() = qtils::byte2str(topic_hash);
          pb_subscription.set_subscribe(subscribe);
        }

        for (auto &publish : message->publish) {
          auto &pb_publish = *pb_message.add_publish();
          assert(config_.message_authenticity == MessageAuthenticity::Signed);

          if (publish.from.has_value()) {
            *pb_publish.mutable_from() =
                qtils::byte2str(publish.from->toVector());
          }

          *pb_publish.mutable_data() = qtils::byte2str(publish.data);

          if (publish.seqno.has_value()) {
            auto &pb_seqno = *pb_publish.mutable_seqno();
            pb_seqno.resize(sizeof(Seqno));
            boost::endian::store_big_u64(qtils::str2byte(pb_seqno.data()),
                                         *publish.seqno);
          }

          *pb_publish.mutable_topic() = qtils::byte2str(publish.topic);

          // TODO: signature
        }

        auto encoded = protobufEncode(pb_message);
        assert(not encoded.empty());
        auto r =
            co_await writeVarintMessage(peer->stream_out_.value(), encoded);
        if (not r.has_value()) {
          peer->stream_out_.reset();
          break;
        }
      }
      peer->writing_ = false;
    });
  }
}  // namespace libp2p::protocol::gossip
