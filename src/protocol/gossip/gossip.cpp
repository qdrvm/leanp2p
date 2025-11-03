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
#include <libp2p/coro/timer_loop.hpp>
#include <libp2p/coro/yield.hpp>
#include <libp2p/crypto/crypto_provider.hpp>
#include <libp2p/host/basic_host.hpp>
#include <libp2p/protocol/gossip/gossip.hpp>
#include <qtils/append.hpp>
#include <qtils/bytes.hpp>
#include <qtils/bytestr.hpp>
#include <qtils/option_take.hpp>

namespace libp2p::protocol::gossip {
  // Signing context prefix for message signing/verification (libp2p-pubsub:).
  constexpr qtils::ByteArr<14> kSigningContext{
      'l', 'i', 'b', 'p', '2', 'p', '-', 'p', 'u', 'b', 's', 'u', 'b', ':'};

  // Convert internal Message to protobuf RPC Message for wire format.
  inline void toProtobuf(gossipsub::pb::Message &pb_publish,
                         const Message &message) {
    pb_publish.Clear();

    if (message.from.has_value()) {
      *pb_publish.mutable_from() = qtils::byte2str(message.from->toVector());
    }

    *pb_publish.mutable_data() = qtils::byte2str(message.data);

    if (message.seqno.has_value()) {
      auto &pb_seqno = *pb_publish.mutable_seqno();
      pb_seqno.resize(sizeof(Seqno));
      boost::endian::store_big_u64(qtils::str2byte(pb_seqno.data()),
                                   *message.seqno);
    }

    *pb_publish.mutable_topic() = qtils::byte2str(message.topic);

    if (message.signature.has_value()) {
      *pb_publish.mutable_signature() = qtils::byte2str(*message.signature);
    }
  }

  // Build the signable bytes: context prefix + protobuf with signature/key
  // cleared.
  inline Bytes getSignable(gossipsub::pb::Message &pb_publish) {
    pb_publish.clear_signature();
    pb_publish.clear_key();
    auto signable = qtils::ByteVec(kSigningContext);
    qtils::append(signable, protobufEncode(pb_publish));
    return signable;
  }

  // Default message ID function (from + seqno) used for deduplication.
  MessageId defaultMessageIdFn(const Message &message) {
    std::string str;
    static auto empty_from = PeerId::fromBytes(Bytes{0, 1, 0}).value();
    str += (message.from.has_value() ? *message.from : empty_from).toBase58();
    str += std::to_string(message.seqno.value_or(0));
    return qtils::ByteVec(qtils::str2byte(std::string_view{str}));
  }

  // Mesh parameter accessors (topic-scoped overrides could be added later).
  size_t Config::mesh_n_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n;
  }

  size_t Config::mesh_n_low_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n_low;
  }

  size_t Config::mesh_n_high_for_topic(const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_n_high;
  }

  size_t Config::mesh_outbound_min_for_topic(
      const TopicHash &topic_hash) const {
    return default_mesh_params.mesh_outbound_min;
  }

  // Subscription batch helper: toggle subscribe/unsubscribe in a set-like map.
  void Rpc::subscribe(TopicHash topic_hash, bool subscribe) {
    auto it = subscriptions.emplace(topic_hash, subscribe).first;
    if (it->second != subscribe) {
      subscriptions.erase(it);
    }
  }

  // Sliding window of recent message IDs per topic for gossip emission.
  History::History(size_t slots) {
    assert(slots > 0);
    slots_.resize(slots);
  }

  void History::add(const MessageId &message_id) {
    slots_.front().emplace_back(message_id);
  }

  // Advance the window by one tick, returning expired IDs.
  std::vector<MessageId> History::shift() {
    auto removed = std::move(slots_.back());
    slots_.pop_back();
    slots_.emplace_front();
    return removed;
  }

  // Collect up to N slots worth of IDs for IHAVE.
  std::vector<MessageId> History::get(size_t slots) {
    std::vector<MessageId> result;
    size_t i = 0;
    for (auto &slot : slots_) {
      if (i >= slots) {
        break;
      }
      result.insert(result.end(), slot.begin(), slot.end());
      ++i;
    }
    return result;
  }

  // Topic backoff with discrete slots aligned to heartbeat ticks.
  TopicBackoff::TopicBackoff(const Config &config) {
    slots_.resize(config.prune_backoff / config.heartbeat_interval
                  + config.backoff_slack + 1);
  }

  // Remaining slots of backoff for a peer in this topic.
  size_t TopicBackoff::get(const PeerPtr &peer) {
    auto peer_it = peers_.find(peer);
    if (peer_it == peers_.end()) {
      return 0;
    }
    return saturating_sub(peer_it->second.first, slot_);
  }

  // Set/update backoff horizon for a peer, keeping the latest.
  void TopicBackoff::update(const PeerPtr &peer, size_t slots) {
    if (slots == 0) {
      return;
    }
    slots = std::min(slots, slots_.size() - 1);
    auto peer_it = peers_.find(peer);
    auto slot = slot_ + slots;
    auto insert = [&] {
      auto &list = slots_.at(slot % slots_.size());
      return std::make_pair(slot, list.insert(list.end(), peer));
    };
    if (peer_it != peers_.end()) {
      if (slot <= peer_it->second.first) {
        return;
      }
      slots_.at(peer_it->second.first % slots_.size())
          .erase(peer_it->second.second);
      peer_it->second = insert();
    } else {
      peers_.emplace(peer, insert());
    }
  }

  // Move to next backoff slot; release peers whose backoff expired.
  void TopicBackoff::shift() {
    auto &slot = slots_.at(slot_ % slots_.size());
    for (auto &peer : slot) {
      peers_.erase(peer);
    }
    slot.clear();
    ++slot_;
  }

  // Receive next message payload from a subscribed topic (for local consumer).
  CoroOutcome<Bytes> Topic::receive() {
    BOOST_OUTCOME_CO_TRY(auto message, co_await receiveMessage());
    co_return message.data;
  }

  CoroOutcome<Message> Topic::receiveMessage() {
    co_return co_await receive_channel_.receive();
  }

  // Publish a message to a topic: sign, dedupe, and broadcast.
  void Topic::publish(BytesIn message) {
    if (auto gossip = weak_gossip_.lock()) {
      gossip->publish(*this, message);
    }
  }

  // Count outbound connections present in the mesh for the topic.
  size_t Topic::meshOutCount() {
    size_t count = 0;
    for (auto &peer : mesh_peers_) {
      if (peer->out_) {
        ++count;
      }
    }
    return count;
  }

  // Peer classification helpers for protocol feature gating.
  Peer::Peer(PeerId peer_id, bool out)
      : peer_id_{std::move(peer_id)}, out_{out} {}

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

  // Construct Gossip, initialize caches/score, and build preferred protocol
  // list.
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
        duplicate_cache_{config.duplicate_cache_time},
        gossip_promises_{config_.iwant_followup_time},
        score_{config_.score} {
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

  // Protocol IDs to advertise for stream negotiation.
  StreamProtocols Gossip::getProtocolIds() const {
    return protocols_;
  }

  // Handle inbound stream: set peer kind, track stream, and read RPCs in a
  // loop.
  void Gossip::handle(std::shared_ptr<Stream> stream) {
    auto peer_id = stream->remotePeerId();
    auto peer_it = peers_.find(peer_id);
    if (peer_it == peers_.end()) {
      stream->reset();
      return;
    }
    auto peer = peer_it->second;
    updatePeerKind(peer, stream->protocol());
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

  // Start event listeners, timers, and open outbound streams on new
  // connections.
  void Gossip::start() {
    host_->listenProtocol(shared_from_this());
    auto on_peer_connected =
        [WEAK_SELF](
            std::weak_ptr<connection::CapableConnection> weak_connection) {
          WEAK_LOCK(connection);
          WEAK_LOCK(self);
          auto peer_id = connection->remotePeer();
          auto out = connection->isInitiator();
          auto peer_it = self->peers_.find(peer_id);
          if (peer_it == self->peers_.end()) {
            peer_it =
                self->peers_
                    .emplace(peer_id, std::make_shared<Peer>(peer_id, out))
                    .first;
            self->score_.connect(peer_id);
          }
          auto peer = peer_it->second;
          // Avoid creating multiple streams concurrently
          if (not peer->stream_out_.has_value() and not peer->is_connecting_) {
            peer->is_connecting_ = true;
            coroSpawn(
                *self->io_context_, [self, connection, peer]() -> Coro<void> {
                  auto stream_result = (co_await self->host_->newStream(
                      connection, self->protocols_));
                  peer->is_connecting_ = false;
                  if (not stream_result.has_value()) {
                    // TODO: can't open out stream?
                    co_return;
                  }
                  if (auto stream = qtils::optionTake(peer->stream_out_)) {
                    (**stream).reset();
                  }
                  auto &stream = stream_result.value();
                  self->updatePeerKind(peer, stream->protocol());
                  peer->stream_out_ = stream;
                  if (not self->topics_.empty()) {
                    auto &message = self->getBatch(peer);
                    for (auto &topic_hash : self->topics_ | std::views::keys) {
                      message.subscribe(topic_hash, true);
                    }
                  }
                });
          }
        };
    auto on_peer_disconnected = [WEAK_SELF](PeerId peer_id) {
      WEAK_LOCK(self);
      self->score_.disconnect(peer_id);
      auto peer_it = self->peers_.find(peer_id);
      if (peer_it != self->peers_.end()) {
        auto &peer = peer_it->second;
        for (auto &topic_hash : peer_it->second->topics_) {
          auto topic_it = self->topics_.find(topic_hash);
          if (topic_it != self->topics_.end()) {
            auto &topic = topic_it->second;
            topic->peers_.erase(peer);
            topic->mesh_peers_.erase(peer);
          }
        }
        self->peers_.erase(peer_it);
      }
    };
    on_peer_connected_sub_ =
        host_->getBus()
            .getChannel<event::network::OnNewConnectionChannel>()
            .subscribe(on_peer_connected);
    on_peer_disconnected_sub_ =
        host_->getBus()
            .getChannel<event::network::OnPeerDisconnectedChannel>()
            .subscribe(on_peer_disconnected);

    timerLoop(*io_context_, config_.heartbeat_interval, [WEAK_SELF] {
      WEAK_LOCK(self);
      self->heartbeat();
    });

    timerLoop(*io_context_, config_.score.decay_interval, [WEAK_SELF] {
      WEAK_LOCK(self);
      self->score_.onDecay();
    });
  }

  // Subscribe locally to a topic: create Topic, announce to peers, and seed
  // mesh.
  std::shared_ptr<Topic> Gossip::subscribe(TopicHash topic_hash) {
    auto topic_it = topics_.find(topic_hash);
    if (topic_it == topics_.end()) {
      auto topic = std::make_shared<Topic>(Topic{
          weak_from_this(),
          topic_hash,
          {*io_context_},
          {config_.history_length},
          {config_},
      });
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
                 return not topic->mesh_peers_.contains(peer)
                    and not is_backoff_with_slack(*topic, peer)
                    and not score_.below(peer->peer_id_, config_.score.zero);
               },
               config_.mesh_n_for_topic(topic_hash))) {
        graft(*topic, peer);
      }
    }
    return topic_it->second;
  }

  std::shared_ptr<Topic> Gossip::subscribe(std::string_view topic_hash) {
    return subscribe(qtils::ByteVec(qtils::str2byte(topic_hash)));
  }

  // Local publish path: create message (signed or anonymous), dedupe, and
  // broadcast to peers.
  void Gossip::publish(Topic &topic, BytesIn data) {
    auto message = std::make_shared<Message>(Message{
        .data = qtils::ByteVec{data},
        .topic = topic.topic_hash_,
    });
    switch (config_.message_authenticity) {
      // Only sign if in Signed mode
      case MessageAuthenticity::Signed: {
        message->from = host_->getId();

        message->seqno = publish_config_.last_seq_no;
        ++publish_config_.last_seq_no;

        gossipsub::pb::Message pb_signable;
        toProtobuf(pb_signable, *message);
        auto signable = getSignable(pb_signable);
        message->signature =
            crypto_provider_->sign(signable, id_mgr_->getKeyPair().privateKey)
                .value();
        break;
      }
      // In Anonymous mode, signature remains empty (std::nullopt)
      case MessageAuthenticity::Anonymous: {
        break;
      }
    }

    auto message_id = config_.message_id_fn(*message);
    if (duplicate_cache_.contains(message_id)) {
      return;
    }
    duplicate_cache_.insert(message_id);
    broadcast(topic, std::nullopt, message_id, message);
  }

  // Inbound RPC handler: subscriptions, publish, and control messages.
  bool Gossip::onMessage(const PeerPtr &peer, BytesIn encoded) {
    // Decode RPC; drop if malformed.
    auto pb_message_result = protobufDecode<gossipsub::pb::RPC>(encoded);
    if (not pb_message_result.has_value()) {
      return false;
    }
    auto &pb_message = pb_message_result.value();

    // Graylist gate: ignore peer below threshold.
    if (score_.below(peer->peer_id_, config_.score.graylist_threshold)) {
      return true;
    }

    // Handle SUBSCRIBE/UNSUBSCRIBE and opportunistic GRAFT on subscribe.
    for (auto &pb_subscribe : pb_message.subscriptions()) {
      auto topic_hash =
          qtils::ByteVec(qtils::str2byte(pb_subscribe.topic_id()));
      auto topic_it = topics_.find(topic_hash);
      if (pb_subscribe.subscribe()) {
        peer->topics_.emplace(topic_hash);
        if (topic_it != topics_.end()) {
          auto &topic = topic_it->second;
          topic->peers_.emplace(peer);

          if (peer->isGossipsub()
              and topic->mesh_peers_.size()
                      < config_.mesh_n_low_for_topic(topic_hash)
              and not topic->mesh_peers_.contains(peer)
              and not is_backoff_with_slack(*topic, peer)
              and not score_.below(peer->peer_id_, config_.score.zero)) {
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

    // Handle PUBLISH: verify signature (if strict mode), dedupe, deliver
    // locally, and relay.
    for (auto &pb_publish : pb_message.publish()) {
      auto message = std::make_shared<Message>();

      switch (config_.validation_mode) {
        case ValidationMode::Strict: {
          auto from_result =
              PeerId::fromBytes(qtils::str2byte(pb_publish.from()));
          if (not from_result) {
            continue;
          }
          auto &from = from_result.value();
          message->from.emplace(from);

          if (pb_publish.seqno().size() != sizeof(Seqno)) {
            continue;
          }
          message->seqno = boost::endian::load_big_u64(
              qtils::str2byte(pb_publish.seqno().data()));

          gossipsub::pb::Message pb_signable = pb_publish;
          auto signable = getSignable(pb_signable);

          message->signature =
              qtils::ByteVec(qtils::str2byte(pb_publish.signature()));

          auto public_key = from.publicKey();
          if (not public_key.has_value()) {
            continue;
          }

          auto verify = crypto_provider_->verify(
              signable, *message->signature, *public_key);
          if (not verify.has_value() or not verify.value()) {
            continue;
          }
          break;
        }
        case ValidationMode::Anonymous: {
          if (not pb_publish.from().empty()) {
            continue;
          }
          if (not pb_publish.seqno().empty()) {
            continue;
          }
          if (not pb_publish.signature().empty()) {
            continue;
          }
          break;
        }
      }

      message->data = qtils::ByteVec(qtils::str2byte(pb_publish.data()));

      message->topic = qtils::ByteVec(qtils::str2byte(pb_publish.topic()));

      message->received_from = peer->peer_id_;

      auto topic_it = topics_.find(message->topic);
      if (topic_it == topics_.end()) {
        continue;
      }
      auto &topic = topic_it->second;
      auto message_id = config_.message_id_fn(*message);
      if (not duplicate_cache_.insert(message_id)) {
        score_.duplicateMessage(peer->peer_id_, message_id, message->topic);
        continue;
      }
      score_.validateMessage(peer->peer_id_, message_id, message->topic);
      topic->receive_channel_.send(*message);
      score_.deliver_message(peer->peer_id_, message_id, message->topic);
      broadcast(*topic, peer->peer_id_, message_id, message);
    }

    // Handle GRAFT: accept (add to mesh) or PRUNE with backoff.
    for (auto &pb_graft : pb_message.control().graft()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      auto topic_hash = qtils::ByteVec(qtils::str2byte(pb_graft.topic_id()));
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
      auto accept = [&] {
        auto backoff = get_backoff_time(*topic, peer);
        if (backoff > Backoff::zero()) {
          score_.addPenalty(peer->peer_id_, 1);
          if (backoff + config_.graft_flood_threshold > config_.prune_backoff) {
            score_.addPenalty(peer->peer_id_, 1);
          }
          return false;
        }
        if (not peer->out_
            and topic->mesh_peers_.size()
                    > config_.mesh_n_high_for_topic(topic_hash)) {
          return false;
        }
        if (score_.below(peer->peer_id_, config_.score.zero)) {
          return false;
        }
        return true;
      }();
      if (accept) {
        topic->mesh_peers_.emplace(peer);
        score_.graft(peer->peer_id_, topic_hash);
      } else {
        make_prune(*topic, peer);
      }
    }

    // Handle PRUNE: remove from mesh and update backoff (v1.1+ honors backoff).
    for (auto &pb_prune : pb_message.control().prune()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      auto topic_hash = qtils::ByteVec(qtils::str2byte(pb_prune.topic_id()));
      std::optional<Backoff> backoff;
      if (pb_prune.has_backoff()) {
        backoff = Backoff{pb_prune.backoff()};
      }
      remove_peer_from_mesh(topic_hash, peer, backoff, true);
    }

    // Handle IHAVE: select a capped subset of unknown IDs and enqueue IWANTs.
    if (not handle_ihave(peer, pb_message)) {
      return false;
    }

    // Handle IWANT: serve up to retransmission cap; honor dont_send_ and score.
    for (auto &pb_iwant : pb_message.control().iwant()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      if (score_.below(peer->peer_id_, config_.score.gossip_threshold)) {
        continue;
      }
      for (auto &pb_message : pb_iwant.message_ids()) {
        auto message_id = qtils::ByteVec(qtils::str2byte(pb_message));
        auto cache_it = message_cache_.find(message_id);
        if (cache_it != message_cache_.end()) {
          auto &count = cache_it->second.iwant[peer->peer_id_];
          ++count;
          if (count > config_.gossip_retransimission) {
            continue;
          }
          if (peer->dont_send_.contains(message_id)) {
            continue;
          }
          getBatch(peer).publish.emplace_back(cache_it->second.message);
        }
      }
    }

    // Handle IDONTWANT: mark IDs to avoid sending to this peer.
    for (auto &pb_idontwant : pb_message.control().idontwant()) {
      if (not peer->isGossipsub()) {
        return false;
      }
      for (auto &pb_message : pb_idontwant.message_ids()) {
        auto message_id = qtils::ByteVec(qtils::str2byte(pb_message));
        peer->dont_send_.insert(message_id);
      }
    }

    return true;
  }

  // Fanout to peers with mesh/flood rules and optional IDONTWANT side channel.
  void Gossip::broadcast(Topic &topic,
                         std::optional<PeerId> from,
                         const MessageId &message_id,
                         const MessagePtr &message) {
    message_cache_.emplace(message_id, MessageCacheEntry{message});
    topic.history_.add(message_id);
    gossip_promises_.remove(message_id);

    auto publish = not from.has_value();
    auto send_idontwant = (not publish or config_.idontwant_on_publish);
    if (send_idontwant) {
      gossipsub::pb::Message pb_publish;
      toProtobuf(pb_publish, *message);
      if (pb_publish.ByteSizeLong()
          <= config_.idontwant_message_size_threshold) {
        send_idontwant = false;
      }
    }
    if (not publish and send_idontwant) {
      gossip_promises_.peers(message_id, [&](const PeerPtr &peer) {
        getBatch(peer).idontwant.emplace(message_id);
      });
    }
    auto add_peer = [&](PeerPtr peer) {
      if (from == peer->peer_id_) {
        return;
      }
      if (message->from == peer->peer_id_) {
        return;
      }
      if (score_.below(peer->peer_id_, config_.score.publish_threshold)) {
        return;
      }
      auto &batch = getBatch(peer);
      if (send_idontwant) {
        batch.idontwant.emplace(message_id);
      }
      batch.publish.emplace_back(message);
    };
    if (publish and config_.flood_publish) {
      for (auto &peer : topic.peers_) {
        add_peer(peer);
      }
    } else {
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
  }

  // Ensure a per-peer batch exists and schedule writer if idle.
  Rpc &Gossip::getBatch(const PeerPtr &peer) {
    if (not peer->batch_) {
      peer->batch_.emplace();
    }
    checkWrite(peer);
    return peer->batch_.value();
  }

  inline std::vector<qtils::ByteVec> splitBatch(const Rpc &message) {
    std::vector<qtils::ByteVec> pb_messages;

    if (not message.subscriptions.empty() or not message.graft.empty()
        or not message.prune.empty() or not message.ihave.empty()
        or not message.iwant.empty() or not message.idontwant.empty()) {
      gossipsub::pb::RPC pb_message;

      for (auto &[topic_hash, subscribe] : message.subscriptions) {
        auto &pb_subscription = *pb_message.add_subscriptions();
        *pb_subscription.mutable_topic_id() = qtils::byte2str(topic_hash);
        pb_subscription.set_subscribe(subscribe);
      }

      for (auto &topic_hash : message.graft) {
        auto &pb_graft = *pb_message.mutable_control()->add_graft();
        *pb_graft.mutable_topic_id() = qtils::byte2str(topic_hash);
      }

      for (auto &[topic_hash, backoff] : message.prune) {
        auto &pb_prune = *pb_message.mutable_control()->add_prune();
        *pb_prune.mutable_topic_id() = qtils::byte2str(topic_hash);
        if (backoff.has_value()) {
          pb_prune.set_backoff(backoff->count());
        }
      }

      for (auto &[topic_hash, messages] : message.ihave) {
        auto &pb_ihave = *pb_message.mutable_control()->add_ihave();
        *pb_ihave.mutable_topic_id() = qtils::byte2str(topic_hash);
        auto &pb_messages = *pb_ihave.mutable_message_ids();
        for (auto &message : messages) {
          *pb_messages.Add() = qtils::byte2str(message);
        }
      }

      if (not message.iwant.empty()) {
        auto &pb_messages =
            *pb_message.mutable_control()->add_iwant()->mutable_message_ids();
        for (auto &message : message.iwant) {
          *pb_messages.Add() = qtils::byte2str(message);
        }
      }

      if (not message.idontwant.empty()) {
        auto &pb_messages = *pb_message.mutable_control()
                                 ->add_idontwant()
                                 ->mutable_message_ids();
        for (auto &message : message.idontwant) {
          *pb_messages.Add() = qtils::byte2str(message);
        }
      }

      pb_messages.emplace_back(protobufEncode(pb_message));
    }

    for (auto &publish : message.publish) {
      gossipsub::pb::RPC pb_message;

      auto &pb_publish = *pb_message.add_publish();
      toProtobuf(pb_publish, *publish);

      pb_messages.emplace_back(protobufEncode(pb_message));
    }

    return pb_messages;
  }

  // Writer coroutine: build RPC from batch and send via varint-length framing.
  void Gossip::checkWrite(const PeerPtr &peer) {
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
        auto pb_messages = splitBatch(*message);

        for (auto &encoded : pb_messages) {
          assert(not encoded.empty());
          auto r =
              co_await writeVarintMessage(peer->stream_out_.value(), encoded);
          if (not r.has_value()) {
            peer->stream_out_.reset();
            break;
          }
        }
        if (not peer->stream_out_.has_value()) {
          break;
        }
      }
      peer->writing_ = false;
    });
  }

  // Record negotiated protocol features for the peer.
  void Gossip::updatePeerKind(const PeerPtr &peer,
                              const ProtocolName &protocol) {
    if (not peer->peer_kind_) {
      peer->peer_kind_ = config_.protocol_versions.at(protocol);
    }
  }

  // Add peer to mesh and enqueue GRAFT control message.
  void Gossip::graft(Topic &topic, const PeerPtr &peer) {
    assert(not topic.mesh_peers_.contains(peer));
    topic.mesh_peers_.emplace(peer);
    score_.graft(peer->peer_id_, topic.topic_hash_);
    getBatch(peer).graft.emplace(topic.topic_hash_);
  }

  // Enqueue PRUNE for peer (with backoff for v1.1+) and update score/backoff.
  void Gossip::make_prune(Topic &topic, const PeerPtr &peer) {
    if (not peer->isGossipsub()) {
      return;
    }
    score_.prune(peer->peer_id_, topic.topic_hash_);
    std::optional<Backoff> backoff;
    if (peer->isGossipsubv1_1()) {
      backoff = config_.prune_backoff;
      update_backoff(topic, peer, *backoff);
    }
    getBatch(peer).prune.emplace(topic.topic_hash_, backoff);
  }

  // Remove peer from mesh and update backoff if requested.
  void Gossip::remove_peer_from_mesh(const TopicHash &topic_hash,
                                     const PeerPtr &peer,
                                     std::optional<Backoff> backoff,
                                     bool always_update_backoff) {
    auto topic_it = topics_.find(topic_hash);
    if (topic_it != topics_.end()) {
      auto &topic = topic_it->second;
      auto peer_removed = topic->mesh_peers_.erase(peer) != 0;
      if (peer_removed) {
        score_.prune(peer->peer_id_, topic_hash);
      }
      if (always_update_backoff or peer_removed) {
        update_backoff(*topic, peer, backoff.value_or(config_.prune_backoff));
      }
    }
  }

  // Heartbeat maintenance: prune bad peers, balance mesh, opportunistic graft,
  // emit gossip, expire history/cache, and clear expired dont_send_ marks.
  void Gossip::heartbeat() {
    ++heartbeat_ticks_;
    for (auto &topic : topics_ | std::views::values) {
      topic->backoff_.shift();
    }

    count_received_ihave_.clear();
    count_sent_iwant_.clear();

    apply_iwant_penalties();

    for (auto &[topic_hash, topic] : topics_) {
      for (auto peer_it = topic->mesh_peers_.begin();
           peer_it != topic->mesh_peers_.end();) {
        auto &peer = *peer_it;
        if (score_.below(peer->peer_id_, config_.score.zero)) {
          make_prune(*topic, peer);
          peer_it = topic->mesh_peers_.erase(peer_it);
        } else {
          ++peer_it;
        }
      }

      auto mesh_n = config_.mesh_n_for_topic(topic_hash);
      auto mesh_outbound_min = config_.mesh_outbound_min_for_topic(topic_hash);
      if (topic->mesh_peers_.size()
          < config_.mesh_n_low_for_topic(topic_hash)) {
        for (auto &peer : choose_peers_.choose(
                 topic->peers_,
                 [&](const PeerPtr &peer) {
                   return not topic->mesh_peers_.contains(peer)
                      and not is_backoff_with_slack(*topic, peer)
                      and not score_.below(peer->peer_id_, config_.score.zero);
                 },
                 saturating_sub(config_.mesh_n_for_topic(topic_hash),
                                topic->mesh_peers_.size()))) {
          graft(*topic, peer);
        }
      } else if (topic->mesh_peers_.size()
                 > config_.mesh_n_high_for_topic(topic_hash)) {
        std::vector<PeerPtr> shuffled;
        shuffled.insert(shuffled.end(),
                        topic->mesh_peers_.begin(),
                        topic->mesh_peers_.end());
        choose_peers_.shuffle(shuffled);
        std::ranges::sort(shuffled, [&](const PeerPtr &l, const PeerPtr &r) {
          return score_.score(r->peer_id_) < score_.score(l->peer_id_);
        });
        choose_peers_.shuffle(std::span{shuffled}.first(
            saturating_sub(shuffled.size(), config_.retain_scores)));
        auto outbound = topic->meshOutCount();
        for (auto &peer : shuffled) {
          if (topic->mesh_peers_.size() <= mesh_n) {
            break;
          }
          if (peer->out_) {
            if (outbound <= mesh_outbound_min) {
              continue;
            }
            --outbound;
          }
          topic->mesh_peers_.erase(peer);
          make_prune(*topic, peer);
        }
      }
      if (topic->mesh_peers_.size()
          >= config_.mesh_n_low_for_topic(topic_hash)) {
        if (auto more =
                saturating_sub(mesh_outbound_min, topic->meshOutCount())) {
          for (auto &peer : choose_peers_.choose(
                   topic->peers_,
                   [&](const PeerPtr &peer) {
                     return peer->out_ and not topic->mesh_peers_.contains(peer)
                        and not is_backoff_with_slack(*topic, peer)
                        and not score_.below(peer->peer_id_,
                                             config_.score.zero);
                   },
                   more)) {
            graft(*topic, peer);
          }
        }
      }
      if (heartbeat_ticks_ % config_.opportunistic_graft_ticks == 0
          and topic->mesh_peers_.size() > 1) {
        std::vector<double> scores;
        scores.reserve(topic->mesh_peers_.size());
        for (auto &peer : topic->mesh_peers_) {
          scores.emplace_back(score_.score(peer->peer_id_));
        }
        std::ranges::sort(scores);
        auto middle = scores.size() / 2;
        auto median = scores.size() % 2 == 0
                        ? (scores[middle - 1] + scores[middle + 1]) / 2
                        : scores[middle];
        if (median < config_.score.opportunistic_graft_threshold) {
          for (auto &peer : choose_peers_.choose(
                   topic->peers_,
                   [&](const PeerPtr &peer) {
                     return not topic->mesh_peers_.contains(peer)
                        and not is_backoff_with_slack(*topic, peer)
                        and score_.score(peer->peer_id_) > median;
                   },
                   config_.opportunistic_graft_peers)) {
            graft(*topic, peer);
          }
        }
      }
    }

    emit_gossip();
    for (auto &topic : topics_ | std::views::values) {
      for (auto &message_id : topic->history_.shift()) {
        message_cache_.erase(message_id);
      }
    }

    auto now = time_cache::Clock::now();
    for (auto &peer : peers_ | std::views::values) {
      peer->dont_send_.clearExpired(now);
    }
  }

  // Emit IHAVE to non-mesh peers above gossip threshold with recent message
  // IDs.
  void Gossip::emit_gossip() {
    for (auto &[topic_hash, topic] : topics_) {
      auto message_ids = topic->history_.get(config_.history_gossip);
      if (message_ids.empty()) {
        continue;
      }
      for (auto &peer : choose_peers_.choose(
               topic->peers_,
               [&](const PeerPtr &peer) {
                 return not topic->mesh_peers_.contains(peer)
                    and not score_.below(peer->peer_id_,
                                         config_.score.gossip_threshold);
               },
               [&](size_t n) {
                 return std::max<size_t>(config_.gossip_lazy,
                                         config_.gossip_factor * n);
               })) {
        auto peer_message_ids = message_ids;
        // Shuffle the per‑peer copy, not the shared vector.
        choose_peers_.shuffle(peer_message_ids);
        if (peer_message_ids.size() > config_.max_ihave_length) {
          peer_message_ids.resize(config_.max_ihave_length);
        }
        getBatch(peer).ihave[topic_hash] = std::move(peer_message_ids);
      }
    }
  }

  // Update per-topic backoff for a peer using heartbeat ticks + slack.
  void Gossip::update_backoff(Topic &topic,
                              const PeerPtr &peer,
                              Backoff backoff) {
    topic.backoff_.update(
        peer, backoff / config_.heartbeat_interval + config_.backoff_slack);
  }

  // Compute remaining backoff time for a peer in a topic.
  Backoff Gossip::get_backoff_time(Topic &topic, const PeerPtr &peer) {
    return saturating_sub(topic.backoff_.get(peer), config_.backoff_slack)
         * config_.heartbeat_interval;
  }

  // Check if peer is still in backoff (with slack) for a topic.
  bool Gossip::is_backoff_with_slack(Topic &topic, const PeerPtr &peer) {
    return topic.backoff_.get(peer) > 0;
  }

  // Apply penalties to peers that didn't deliver promised messages in time.
  void Gossip::apply_iwant_penalties() {
    for (auto &[peer, count] : gossip_promises_.clearExpired()) {
      score_.addPenalty(peer->peer_id_, count);
    }
  }

  // Process IHAVE set: cap per-tick, filter by score/backoff, and enqueue
  // IWANT.
  bool Gossip::handle_ihave(const PeerPtr &peer,
                            const gossipsub::pb::RPC &pb_message) {
    auto &pb_ihaves = pb_message.control().ihave();
    if (pb_ihaves.empty()) {
      return true;
    }
    if (not peer->isGossipsub()) {
      return false;
    }
    auto &peer_have = count_received_ihave_[peer];
    ++peer_have;
    if (peer_have > config_.max_ihave_messages) {
      return true;
    }
    auto want_it = count_sent_iwant_.find(peer);
    if (want_it != count_sent_iwant_.end()
        and want_it->second >= config_.max_ihave_length) {
      return true;
    }
    std::unordered_set<MessageId, qtils::BytesStdHash> want_set;
    for (auto &pb_ihave : pb_ihaves) {
      if (score_.below(peer->peer_id_, config_.score.gossip_threshold)) {
        continue;
      }
      auto topic_hash = qtils::ByteVec(qtils::str2byte(pb_ihave.topic_id()));
      if (not topics_.contains(topic_hash)) {
        continue;
      }
      for (auto &pb_message : pb_ihave.message_ids()) {
        auto message_id = qtils::ByteVec(qtils::str2byte(pb_message));
        if (duplicate_cache_.contains(message_id)) {
          continue;
        }
        if (gossip_promises_.contains(message_id)) {
          continue;
        }
        want_set.emplace(message_id);
      }
    }
    if (want_set.empty()) {
      return true;
    }
    if (want_it == count_sent_iwant_.end()) {
      want_it = count_sent_iwant_.emplace(peer, 0).first;
    }
    auto count =
        std::min(want_set.size(),
                 saturating_sub(config_.max_ihave_length, want_it->second));
    std::vector<MessageId> want;
    want.reserve(want_set.size());
    for (auto &message_id : want_set) {
      want.emplace_back(message_id);
    }
    choose_peers_.shuffle(want);
    if (want.size() > count) {
      want.resize(count);
    }
    want_it->second += count;
    auto &batch = getBatch(peer);
    for (auto &message_id : want) {
      gossip_promises_.add(message_id, peer);
      batch.iwant.emplace(message_id);
    }
    return true;
  }
}  // namespace libp2p::protocol::gossip
