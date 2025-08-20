/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/coro/channel.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/protocol/base_protocol.hpp>
#include <libp2p/protocol/gossip/config.hpp>
#include <libp2p/protocol/gossip/time_cache.hpp>
#include <qtils/bytes_std_hash.hpp>
#include <random>
#include <unordered_set>

namespace boost::asio {
  class io_context;
}  // namespace boost::asio

namespace libp2p::crypto {
  class CryptoProvider;
}  // namespace libp2p::crypto

namespace libp2p::host {
  class BasicHost;
}  // namespace libp2p::host

namespace libp2p::peer {
  class IdentityManager;
}  // namespace libp2p::peer

namespace libp2p::protocol::gossip {
  class Peer;
  class Gossip;

  using PeerPtr = std::shared_ptr<Peer>;
  using StreamPtr = std::shared_ptr<connection::Stream>;
  using MessagePtr = std::shared_ptr<Message>;

  struct PublishConfigSigning {
    Seqno last_seq_no;
  };

  struct Rpc {
    void subscribe(TopicHash topic_hash, bool subscribe);

    std::unordered_map<TopicHash, bool, qtils::BytesStdHash> subscriptions;
    std::vector<MessagePtr> publish;
    std::unordered_set<TopicHash, qtils::BytesStdHash> graft;
    std::unordered_map<TopicHash, std::optional<Backoff>, qtils::BytesStdHash>
        prune;
    std::unordered_map<TopicHash, std::vector<MessageId>, qtils::BytesStdHash>
        ihave;
    std::unordered_set<MessageId, qtils::BytesStdHash> iwant;
  };

  class History {
   public:
    History(size_t slots);
    void add(const MessageId &message_id);
    std::vector<MessageId> shift();
    std::vector<MessageId> get(size_t slots);

    std::deque<std::vector<MessageId>> slots_;
  };

  struct MessageCacheEntry {
    MessagePtr message;
    std::unordered_map<PeerId, size_t> iwant;
  };

  struct TopicBackoff {
    TopicBackoff(const Config &config);
    size_t get(const PeerPtr &peer);
    void update(const PeerPtr &peer, size_t slots);
    void shift();

    using Slot = std::list<PeerPtr>;
    std::vector<Slot> slots_;
    size_t slot_ = 0;
    std::unordered_map<PeerPtr, std::pair<size_t, Slot::iterator>> peers_;
  };

  class Topic {
   public:
    CoroOutcome<Bytes> receive();

    void publish(BytesIn message);

    std::weak_ptr<Gossip> weak_gossip_;
    TopicHash topic_hash_;
    CoroOutcomeChannel<Bytes> receive_channel_;
    History history_;
    TopicBackoff backoff_;
    std::unordered_set<PeerPtr> peers_;
    std::unordered_set<PeerPtr> mesh_peers_;
  };

  class Peer {
   public:
    Peer(PeerId peer_id);

    bool isFloodsub() const;
    bool isGossipsub() const;
    bool isGossipsubv1_1() const;
    bool isGossipsubv1_2() const;

    PeerId peer_id_;
    std::optional<PeerKind> peer_kind_;
    std::unordered_set<TopicHash, qtils::BytesStdHash> topics_;
    std::optional<StreamPtr> stream_out_;
    std::unordered_set<StreamPtr> streams_in_;
    std::optional<Rpc> batch_;
    bool writing_ = false;
  };

  class ChoosePeers {
   public:
    std::deque<PeerPtr> choose(auto &&all,
                               const std::invocable<PeerPtr> auto &predicate,
                               const std::invocable<size_t> auto &get_count) {
      std::deque<PeerPtr> chosen;
      for (auto &peer : all) {
        if (peer->isGossipsub() and predicate(peer)) {
          chosen.emplace_back(peer);
        }
      }
      std::ranges::shuffle(chosen, random_);
      auto count = get_count(chosen.size());
      if (chosen.size() > count) {
        chosen.resize(count);
      }
      return chosen;
    }
    std::deque<PeerPtr> choose(auto &&all,
                               const std::invocable<PeerPtr> auto &predicate,
                               size_t count) {
      return choose(std::forward<decltype(all)>(all), predicate, [&](size_t) {
        return count;
      });
    }

    void shuffle(auto &&r) {
      std::ranges::shuffle(std::forward<decltype(r)>(r), random_);
    }

   private:
    std::default_random_engine random_;
  };

  class Gossip : public std::enable_shared_from_this<Gossip>,
                 public BaseProtocol {
   public:
    Gossip(std::shared_ptr<boost::asio::io_context> io_context,
           std::shared_ptr<host::BasicHost> host,
           std::shared_ptr<peer::IdentityManager> id_mgr,
           std::shared_ptr<crypto::CryptoProvider> crypto_provider,
           Config config);

    // BaseProtocol
    StreamProtocols getProtocolIds() const override;
    void handle(StreamAndProtocol stream) override;

    void start();

    std::shared_ptr<Topic> subscribe(TopicHash topic_hash);
    std::shared_ptr<Topic> subscribe(std::string_view topic_hash);

    void publish(Topic &topic, BytesIn data);

    void broadcast(Topic &topic,
                   std::optional<PeerId> from,
                   const MessagePtr &message);

    bool onMessage(const std::shared_ptr<Peer> &peer, BytesIn encoded);

    std::shared_ptr<Peer> getPeer(const PeerId &peer_id);

    Rpc &getBatch(const std::shared_ptr<Peer> &peer);

    void checkWrite(const std::shared_ptr<Peer> &peer);

    void updatePeerKind(const PeerPtr &peer, const ProtocolName &protocol);

    void graft(Topic &topic, const PeerPtr &peer);

    void make_prune(Topic &topic, const PeerPtr &peer);

    void remove_peer_from_mesh(const TopicHash &topic_hash,
                               const PeerPtr &peer,
                               std::optional<Backoff> backoff,
                               bool always_update_backoff);

   private:
    Coro<void> heartbeat();

    void emit_gossip();

    void update_backoff(Topic &topic, const PeerPtr &peer, Backoff backoff);
    Backoff get_backoff_time(Topic &topic, const PeerPtr &peer);
    bool is_backoff_with_slack(Topic &topic, const PeerPtr &peer);

    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<host::BasicHost> host_;
    std::shared_ptr<peer::IdentityManager> id_mgr_;
    std::shared_ptr<crypto::CryptoProvider> crypto_provider_;
    Config config_;
    StreamProtocols protocols_;
    event::Handle on_peer_sub_;
    std::unordered_map<TopicHash, std::shared_ptr<Topic>, qtils::BytesStdHash>
        topics_;
    std::unordered_map<PeerId, std::shared_ptr<Peer>> peers_;
    PublishConfigSigning publish_config_;
    DuplicateCache<MessageId, qtils::BytesStdHash> duplicate_cache_;
    ChoosePeers choose_peers_;
    std::unordered_map<MessageId, MessageCacheEntry, qtils::BytesStdHash>
        message_cache_;
  };
}  // namespace libp2p::protocol::gossip
