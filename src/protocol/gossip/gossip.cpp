/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <generated/protocol/gossip/gossip.pb.h>
#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <boost/endian/conversion.hpp>
#include <libp2p/basic/encode_varint.hpp>
#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write_varint.hpp>
#include <libp2p/common/protobuf.hpp>
#include <libp2p/common/saturating.hpp>
#include <libp2p/common/weak_macro.hpp>
#include <libp2p/coro/spawn.hpp>
#include <libp2p/coro/yield.hpp>
#include <libp2p/crypto/crypto_provider.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <qtils/append.hpp>
#include <qtils/bytes.hpp>
#include <qtils/bytestr.hpp>
#include <qtils/option_take.hpp>

namespace libp2p::protocol::gossip {
  constexpr qtils::BytesN<14> kSigningContext{
      'l', 'i', 'b', 'p', '2', 'p', '-', 'p', 'u', 'b', 's', 'u', 'b', ':'};

  inline Bytes getSignable(gossipsub::pb::Message &pb_publish) {
    pb_publish.clear_signature();
    pb_publish.clear_key();
    auto signable = qtils::asVec(kSigningContext);
    qtils::append(signable, protobufEncode(pb_publish));
    return signable;
  }

  MessageId defaultMessageIdFn(const Message &message) {
    std::string str;
    static auto empty_from = PeerId::fromBytes(Bytes{0, 1, 0}).value();
    str += (message.from.has_value() ? *message.from : empty_from).toBase58();
    str += std::to_string(message.seqno.value_or(0));
    return qtils::asVec(qtils::str2byte(std::string_view{str}));
  }

  size_t Config::mesh_n_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n;
  }

  size_t Config::mesh_n_low_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n_low;
  }

  size_t Config::mesh_n_high_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n_high;
  }

  void Rpc::subscribe(TopicHash topic_hash, bool subscribe) {
    auto it = subscriptions.emplace(topic_hash, subscribe).first;
    if (it->second != subscribe) {
      subscriptions.erase(it);
    }
  }

  History::History(size_t slots) {
    assert(slots > 0);
    slots_.resize(slots);
  }

  void History::add(const MessageId &message_id) {
    slots_.front().emplace_back(message_id);
  }

  std::vector<MessageId> History::shift() {
    auto removed = std::move(slots_.back());
    slots_.pop_back();
    slots_.emplace_front();
    return removed;
  }

  std::vector<MessageId> History::get(size_t slots) {
    std::vector<MessageId> result;
    size_t i = 0;
    for (auto &slot : slots_) {
      if (i >= slots) {
        break;
      }
      result.append_range(slot);
      ++i;
    }
    return result;
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

  bool Peer::isFloodsub() const {
    return not peer_kind_.has_value()
        or peer_kind_.value() == PeerKind::Floodsub;
  }

  bool Peer::isGossipsub() const {
    return peer_kind_.has_value() and peer_kind_.value() >= PeerKind::Gossipsub;
  }

  bool Peer::isGossipsubv1_1() const {
    return peer_kind_.has_value()
       and peer_kind_.value() >= PeerKind::Gossipsubv1_1;
  }

  bool Peer::isGossipsubv1_2() const {
    return peer_kind_.has_value()
       and peer_kind_.value() >= PeerKind::Gossipsubv1_2;
  }

  Gossip::Gossip(std::shared_ptr<boost::asio::io_context> io_context,
                 std::shared_ptr<host::BasicHost> host,
                 std::shared_ptr<peer::IdentityManager> id_mgr,
                 std::shared_ptr<crypto::CryptoProvider> crypto_provider,
                 Config config)
      : io_context_{std::move(io_context)},
        host_{std::move(host)},
        id_mgr_{std::move(id_mgr)},
        crypto_provider_{std::move(crypto_provider)},
        config_{std::move(config)},
        publish_config_{
            .last_seq_no = static_cast<Seqno>(std::chrono::nanoseconds{
                std::chrono::system_clock::now().time_since_epoch()}
                                                  .count()),
        },
        duplicate_cache_{config.duplicate_cache_time} {
    assert(config_.message_id_fn);

    for (auto &protocol : config_.protocol_versions | std::views::keys) {
      protocols_.emplace_back(protocol);
    }
    std::ranges::sort(protocols_,
                      [&](const ProtocolName &l, const ProtocolName &r) {
                        return config_.protocol_versions.at(l)
                             > config_.protocol_versions.at(r);
                      });
  }

  StreamProtocols Gossip::getProtocolIds() const {
    return protocols_;
  }

  void Gossip::handle(StreamAndProtocol stream_and_protocol) {
    auto &[stream, protocol] = stream_and_protocol;
    auto peer_id = stream->remotePeerId();
    auto peer = getPeer(peer_id);
    updatePeerKind(peer, protocol);
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
    std::println("LocalPeerId {}", host_->getId().toBase58());
    host_->listenProtocol(shared_from_this());
    auto on_peer = [WEAK_SELF](std::weak_ptr<connection::CapableConnection>
                                   weak_connection) {
      WEAK_LOCK(connection);
      WEAK_LOCK(self);
      auto peer_id = connection->remotePeer();
      std::println("ConnectionEstablished {}", peer_id.toBase58());
      auto peer = self->getPeer(peer_id);
      coroSpawn(*self->io_context_, [self, connection, peer]() -> Coro<void> {
        auto stream_and_protocol_result =
            (co_await self->host_->newStream(connection, self->protocols_));
        if (not stream_and_protocol_result.has_value()) {
          // TODO: can't open out stream?
          co_return;
        }
        if (auto stream = qtils::optionTake(peer->stream_out_)) {
          (**stream).reset();
        }
        auto &[stream, protocol] = stream_and_protocol_result.value();
        self->updatePeerKind(peer, protocol);
        peer->stream_out_ = stream;
        if (not self->topics_.empty()) {
          auto &message = self->getBatch(peer);
          for (auto &topic_hash : self->topics_ | std::views::keys) {
            message.subscribe(topic_hash, true);
          }
        }
      });
    };
    on_peer_sub_ = host_->getBus()
                       .getChannel<event::network::OnNewConnectionChannel>()
                       .subscribe(on_peer);

    coroSpawn(*io_context_, [self{shared_from_this()}]() -> Coro<void> {
      co_await self->heartbeat();
    });
  }

  std::shared_ptr<Topic> Gossip::subscribe(TopicHash topic_hash) {
    auto topic_it = topics_.find(topic_hash);
    if (topic_it == topics_.end()) {
      auto topic = std::make_shared<Topic>(Topic{weak_from_this(),
                                                 topic_hash,
                                                 {*io_context_},
                                                 {config_.history_length}});
      topic_it = topics_.emplace(topic_hash, topic).first;
      for (auto &peer : peers_ | std::views::values) {
        if (peer->topics_.contains(topic_hash)) {
          topic->peers_.emplace(peer);
        }
        getBatch(peer).subscribe(topic_hash, true);
      }
      for (auto &peer : choose_peers_.choose(
               topic->peers_,
               [&](const PeerPtr &peer) {
                 return not topic->mesh_peers_.contains(peer);
               },
               config_.mesh_n_for_topic(topic_hash))) {
        graft(*topic, peer);
      }
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
    duplicate_cache_.insert(message_id);
    message_cache_.emplace(message_id, MessageCacheEntry{message});
    topic.history_.add(message_id);
    broadcast(topic, std::nullopt, message);
  }

  bool Gossip::onMessage(const std::shared_ptr<Peer> &peer, BytesIn encoded) {
    auto pb_message_result = protobufDecode<gossipsub::pb::RPC>(encoded);
    if (not pb_message_result.has_value()) {
      return false;
    }
    auto &pb_message = pb_message_result.value();

    for (auto &pb_subscribe : pb_message.subscriptions()) {
      auto topic_hash = qtils::asVec(qtils::str2byte(pb_subscribe.topic_id()));
      auto topic_it = topics_.find(topic_hash);
      if (pb_subscribe.subscribe()) {
        std::println("Subscribed {} {}",
                     peer->peer_id_.toBase58(),
                     qtils::byte2str(topic_hash));
        peer->topics_.emplace(topic_hash);
        if (topic_it != topics_.end()) {
          auto &topic = topic_it->second;
          topic->peers_.emplace(peer);

          if (peer->isGossipsub()
              and topic->mesh_peers_.size()
                      < config_.mesh_n_low_for_topic(topic_hash)
              and not topic->mesh_peers_.contains(peer)) {
            graft(*topic, peer);
          }
        }
      } else {
        peer->topics_.erase(topic_hash);
        if (topic_it != topics_.end()) {
          auto &topic = topic_it->second;
          topic->peers_.erase(peer);
        }
        remove_peer_from_mesh(topic_hash, peer, std::nullopt, false);
      }
    }

    for (auto &pb_publish : pb_message.publish()) {
      Message message;

      assert(config_.validation_mode == ValidationMode::Strict);
      auto from_result = PeerId::fromBytes(qtils::str2byte(pb_publish.from()));
      if (not from_result) {
        continue;
      }
      auto &from = from_result.value();
      message.from.emplace(from);

      message.data = qtils::asVec(qtils::str2byte(pb_publish.data()));

      if (pb_publish.seqno().size() != sizeof(Seqno)) {
        continue;
      }
      message.seqno = boost::endian::load_big_u64(
          qtils::str2byte(pb_publish.seqno().data()));

      message.topic = qtils::asVec(qtils::str2byte(pb_publish.topic()));

      gossipsub::pb::Message pb_signable = pb_publish;
      auto signable = getSignable(pb_signable);

      message.signature = qtils::asVec(qtils::str2byte(pb_publish.signature()));

      auto public_key = from.publicKey();
      if (not public_key.has_value()) {
        continue;
      }

      auto verify =
          crypto_provider_->verify(signable, *message.signature, *public_key);
      if (not verify.has_value() or not verify.value()) {
        continue;
      }

      auto topic_it = topics_.find(message.topic);
      if (topic_it == topics_.end()) {
        continue;
      }
      auto &topic = topic_it->second;
      auto message_id = config_.message_id_fn(message);
      if (not duplicate_cache_.insert(message_id)) {
        // TODO: mcache.observe_duplicate()
        continue;
      }
      topic->receive_channel_.send(message.data);
      message_cache_.emplace(message_id, MessageCacheEntry{message});
      topic->history_.add(message_id);
      broadcast(*topic, peer->peer_id_, message);
    }

    for (auto &pb_graft : pb_message.control().graft()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      auto topic_hash = qtils::asVec(qtils::str2byte(pb_graft.topic_id()));
      peer->topics_.emplace(topic_hash);

      auto topic_it = topics_.find(topic_hash);
      if (topic_it == topics_.end()) {
        continue;
      }
      auto &topic = topic_it->second;
      topic->peers_.emplace(peer);

      if (topic->mesh_peers_.contains(peer)) {
        continue;
      }
      if (topic->mesh_peers_.size()
          < config_.mesh_n_high_for_topic(topic_hash)) {
        topic->mesh_peers_.emplace(peer);
      } else {
        make_prune(topic_hash, peer);
      }
    }

    for (auto &pb_prune : pb_message.control().prune()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      auto topic_hash = qtils::asVec(qtils::str2byte(pb_prune.topic_id()));
      std::optional<Backoff> backoff;
      if (pb_prune.has_backoff()) {
        backoff = Backoff{pb_prune.backoff()};
      }
      remove_peer_from_mesh(topic_hash, peer, backoff, true);
    }

    for (auto &pb_ihave : pb_message.control().ihave()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      auto topic_hash = qtils::asVec(qtils::str2byte(pb_ihave.topic_id()));
      if (not topics_.contains(topic_hash)) {
        continue;
      }
      for (auto &pb_message : pb_ihave.message_ids()) {
        auto message_id = qtils::asVec(qtils::str2byte(pb_message));
        if (duplicate_cache_.contains(message_id)) {
          continue;
        }
        // TODO: gossip_promises
        // TODO: count_sent_iwant
        // TODO: max_ihave_length
        // TODO: shuffle
        // TODO: gossip_promises.add_promise
        getBatch(peer).iwant.emplace(message_id);
      }
    }

    for (auto &pb_iwant : pb_message.control().iwant()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      for (auto &pb_message : pb_iwant.message_ids()) {
        auto message_id = qtils::asVec(qtils::str2byte(pb_message));
        auto cache_it = message_cache_.find(message_id);
        if (cache_it != message_cache_.end()) {
          auto &count = cache_it->second.iwant[peer->peer_id_];
          ++count;
          if (count > config_.gossip_retransimission) {
            continue;
          }
          // TODO: dont_send
          getBatch(peer).publish.emplace_back(cache_it->second.message);
        }
      }
    }

    return true;
  }

  void Gossip::broadcast(Topic &topic,
                         std::optional<PeerId> from,
                         const Message &message) {
    auto publish = not from.has_value();
    auto add_peer = [&](PeerPtr peer) {
      if (from == peer->peer_id_) {
        return;
      }
      if (message.from == peer->peer_id_) {
        return;
      }
      getBatch(peer).publish.emplace_back(message);
    };
    for (auto &peer : topic.peers_) {
      if (peer->isFloodsub()) {
        add_peer(peer);
      }
    }
    for (auto &peer : topic.mesh_peers_) {
      add_peer(peer);
    }
    if (publish) {
      if (auto more =
              saturating_sub(config_.mesh_n_for_topic(topic.topic_hash_),
                             topic.mesh_peers_.size())) {
        for (auto &peer : choose_peers_.choose(
                 topic.peers_,
                 [&](const PeerPtr &peer) {
                   return not topic.mesh_peers_.contains(peer);
                 },
                 more)) {
          add_peer(peer);
        }
      }
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
    coroSpawn(*io_context_, [WEAK_SELF, peer]() -> Coro<void> {
      co_await coroYield();
      assert(peer->writing_);
      assert(peer->stream_out_.has_value());
      while (auto message = qtils::optionTake(peer->batch_)) {
        auto self = weak_self.lock();
        if (not self) {
          break;
        }

        gossipsub::pb::RPC pb_message;
        assert(not message->subscriptions.empty()
               or not message->publish.empty() or not message->graft.empty()
               or not message->prune.empty() or not message->ihave.empty()
               or not message->iwant.empty());

        for (auto &[topic_hash, subscribe] : message->subscriptions) {
          auto &pb_subscription = *pb_message.add_subscriptions();
          *pb_subscription.mutable_topic_id() = qtils::byte2str(topic_hash);
          pb_subscription.set_subscribe(subscribe);
        }

        for (auto &publish : message->publish) {
          auto &pb_publish = *pb_message.add_publish();
          assert(self->config_.message_authenticity
                 == MessageAuthenticity::Signed);

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

          Bytes signature;
          if (publish.signature.has_value()) {
            signature = *publish.signature;
          } else {
            auto signable = getSignable(pb_publish);
            signature =
                self->crypto_provider_
                    ->sign(signable, self->id_mgr_->getKeyPair().privateKey)
                    .value();
          }
          *pb_publish.mutable_signature() = qtils::byte2str(signature);
        }

        for (auto &topic_hash : message->graft) {
          auto &pb_graft = *pb_message.mutable_control()->add_graft();
          *pb_graft.mutable_topic_id() = qtils::byte2str(topic_hash);
        }

        for (auto &[topic_hash, backoff] : message->prune) {
          auto &pb_prune = *pb_message.mutable_control()->add_prune();
          *pb_prune.mutable_topic_id() = qtils::byte2str(topic_hash);
          if (backoff.has_value()) {
            pb_prune.set_backoff(backoff->count());
          }
        }

        for (auto &[topic_hash, messages] : message->ihave) {
          auto &pb_ihave = *pb_message.mutable_control()->add_ihave();
          *pb_ihave.mutable_topic_id() = qtils::byte2str(topic_hash);
          auto &pb_messages = *pb_ihave.mutable_message_ids();
          for (auto &message : messages) {
            *pb_messages.Add() = qtils::byte2str(message);
          }
        }

        if (not message->iwant.empty()) {
          auto &pb_messages =
              *pb_message.mutable_control()->add_iwant()->mutable_message_ids();
          for (auto &message : message->iwant) {
            *pb_messages.Add() = qtils::byte2str(message);
          }
        }

        auto encoded = protobufEncode(pb_message);
        assert(not encoded.empty());
        self.reset();
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

  void Gossip::updatePeerKind(const PeerPtr &peer,
                              const ProtocolName &protocol) {
    if (not peer->peer_kind_) {
      peer->peer_kind_ = config_.protocol_versions.at(protocol);
    }
  }

  void Gossip::graft(Topic &topic, const PeerPtr &peer) {
    assert(not topic.mesh_peers_.contains(peer));
    topic.mesh_peers_.emplace(peer);
    getBatch(peer).graft.emplace(topic.topic_hash_);
  }

  void Gossip::make_prune(const TopicHash &topic_hash, const PeerPtr &peer) {
    if (not peer->isGossipsub()) {
      return;
    }
    std::optional<Backoff> backoff;
    if (peer->isGossipsubv1_1()) {
      backoff = config_.prune_backoff;
      // TODO: update_backoff()
    }
    getBatch(peer).prune.emplace(topic_hash, backoff);
  }

  void Gossip::remove_peer_from_mesh(const TopicHash &topic_hash,
                                     const PeerPtr &peer,
                                     std::optional<Backoff> backoff,
                                     bool always_update_backoff) {
    auto peer_removed = false;
    auto topic_it = topics_.find(topic_hash);
    if (topic_it != topics_.end()) {
      auto &topic = topic_it->second;
      peer_removed = topic->mesh_peers_.erase(peer) != 0;
    }
    if (always_update_backoff or peer_removed) {
      // TODO: update_backoff()
    }
  }

  Coro<void> Gossip::heartbeat() {
    boost::asio::steady_timer timer{*io_context_};
    while (true) {
      for (auto &[topic_hash, topic] : topics_) {
        // TODO: remove negative score from mesh

        auto mesh_n = config_.mesh_n_for_topic(topic_hash);
        if (topic->mesh_peers_.size()
            < config_.mesh_n_low_for_topic(topic_hash)) {
          for (auto &peer : choose_peers_.choose(
                   topic->peers_,
                   [&](const PeerPtr &peer) {
                     return not topic->mesh_peers_.contains(peer);
                   },
                   saturating_sub(config_.mesh_n_for_topic(topic_hash),
                                  topic->mesh_peers_.size()))) {
            graft(*topic, peer);
          }
        } else if (topic->mesh_peers_.size()
                   > config_.mesh_n_high_for_topic(topic_hash)) {
          std::vector<PeerPtr> shuffled;
          shuffled.append_range(topic->mesh_peers_);
          choose_peers_.shuffle(shuffled);
          // TODO: sort score
          choose_peers_.shuffle(std::span{shuffled}.first(
              saturating_sub(shuffled.size(), config_.retain_scores)));
          for (auto &peer : shuffled) {
            if (topic->mesh_peers_.size() <= mesh_n) {
              break;
            }
            // TODO: outbound
            topic->mesh_peers_.erase(peer);
            make_prune(topic_hash, peer);
          }
        }
        if (topic->mesh_peers_.size()
            >= config_.mesh_n_low_for_topic(topic_hash)) {
          // TODO: outbound
        }
        // TODO: opportunistic graft
      }

      emit_gossip();
      for (auto &topic : topics_ | std::views::values) {
        for (auto &message_id : topic->history_.shift()) {
          message_cache_.erase(message_id);
        }
      }

      timer.expires_after(config_.heartbeat_interval);
      co_await timer.async_wait(boost::asio::use_awaitable);
    }
  }

  void Gossip::emit_gossip() {
    for (auto &[topic_hash, topic] : topics_) {
      auto message_ids = topic->history_.get(config_.history_gossip);
      if (message_ids.empty()) {
        continue;
      }
      for (auto &peer : choose_peers_.choose(
               topic->peers_,
               [&](const PeerPtr &peer) {
                 // TODO: score
                 return not topic->mesh_peers_.contains(peer);
               },
               [&](size_t n) {
                 return std::max<size_t>(config_.gossip_lazy,
                                         config_.gossip_factor * n);
               })) {
        auto peer_message_ids = message_ids;
        choose_peers_.shuffle(message_ids);
        if (peer_message_ids.size() > config_.max_ihave_length) {
          peer_message_ids.resize(config_.max_ihave_length);
        }
        getBatch(peer).ihave[topic_hash] = std::move(peer_message_ids);
      }
    }
  }
}  // namespace libp2p::protocol::gossip
