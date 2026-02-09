/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/coro/channel.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/event/bus.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/protocol/base_protocol.hpp>
#include <libp2p/protocol/gossip/config.hpp>
#include <libp2p/protocol/gossip/score.hpp>
#include <libp2p/protocol/gossip/time_cache.hpp>
#include <qtils/bytes_std_hash.hpp>
#include <random>
#include <unordered_set>

namespace gossipsub::pb {
  class RPC;
}  // namespace gossipsub::pb

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
  /**
   * Peer object for Gossipsub protocol.
   * Represents a remote node and its state in the mesh.
   */
  class Peer;
  /**
   * Main Gossipsub protocol handler.
   * Manages mesh, topics, peer scoring, message cache, and protocol logic.
   */
  class Gossip;

  using PeerPtr = std::shared_ptr<Peer>;
  using StreamPtr = std::shared_ptr<connection::Stream>;
  using MessagePtr = std::shared_ptr<Message>;

  /**
   * Tracks last sequence number for signed publishing.
   */
  struct PublishConfigSigning {
    Seqno last_seq_no;
  };

  /**
   * Outgoing RPC batch for a peer.
   * Used to accumulate messages to send in a single RPC.
   */
  struct Rpc {
    /**
     * Add or remove a subscription toggle for topic_hash.
     */
    void subscribe(TopicHash topic_hash, bool subscribe);
    // Subscribed topics and their state
    std::unordered_map<TopicHash, bool, qtils::BytesStdHash> subscriptions;
    // Messages to publish
    std::vector<MessagePtr> publish;
    // Topics to graft (join mesh)
    std::unordered_set<TopicHash, qtils::BytesStdHash> graft;
    // Topics to prune (leave mesh), with optional backoff
    std::unordered_map<TopicHash, std::optional<Backoff>, qtils::BytesStdHash>
        prune;
    // IHAVE: topic -> message IDs to gossip
    std::unordered_map<TopicHash, std::vector<MessageId>, qtils::BytesStdHash>
        ihave;
    // IWANT: message IDs requested from peer
    std::unordered_set<MessageId, qtils::BytesStdHash> iwant;
    // IDONTWANT: message IDs we don't want from peer
    std::unordered_set<MessageId, qtils::BytesStdHash> idontwant;
  };

  /**
   * History of recently seen message IDs for a topic.
   * Used for gossip and duplicate detection.
   */
  class History {
   public:
    /**
     * Create a sliding window with 'slots' time buckets.
     */
    History(size_t slots);
    /**
     * Add a message ID to the newest bucket.
     */
    void add(const MessageId &message_id);
    /**
     * Advance the window, returning expired IDs from the oldest bucket.
     */
    std::vector<MessageId> shift();
    /**
     * Collect up to 'slots' buckets worth of IDs from newest to oldest.
     */
    std::vector<MessageId> get(size_t slots);
    // slots_: deque of message ID vectors, oldest at back
    std::deque<std::vector<MessageId>> slots_;
  };

  /**
   * Entry in the message cache: stores the message and IWANT counters per peer.
   */
  struct MessageCacheEntry {
    MessagePtr message;
    bool validated;
    std::unordered_map<PeerId, size_t> iwant;
  };

  /**
   * Per-topic backoff tracker for mesh management.
   * Used to prevent rapid re-grafting of peers.
   */
  struct TopicBackoff {
    TopicBackoff(const Config &config);
    /** Remaining backoff slots for peer in this topic. */
    size_t get(const PeerPtr &peer);
    /** Set/update backoff horizon (in slots) for peer. */
    void update(const PeerPtr &peer, size_t slots);
    /** Move to next slot and release expired peers. */
    void shift();
    using Slot = std::list<PeerPtr>;
    std::vector<Slot> slots_;  // circular buffer of slots
    size_t slot_ = 0;          // current slot index
    std::unordered_map<PeerPtr, std::pair<size_t, Slot::iterator>> peers_;
  };

  /**
   * Topic object: tracks mesh peers, history, backoff, and receive channel.
   */
  class Topic {
   public:
    /** Receive next message published to this topic (awaitable). */
    CoroOutcome<Bytes> receive();
    CoroOutcome<MessagePtr> receiveMessage();
    /** Publish a payload to this topic (signed locally). */
    void publish(BytesIn message);
    /** Count outbound peers currently in the mesh. */
    size_t meshOutCount() const;

    std::weak_ptr<Gossip> weak_gossip_;
    TopicHash topic_hash_;
    bool validate_ = false;
    bool publish_only_ = false;
    CoroOutcomeChannel<MessagePtr> receive_channel_;
    History history_;
    TopicBackoff backoff_;
    std::unordered_set<PeerPtr> peers_;       // all peers subscribed
    std::unordered_set<PeerPtr> mesh_peers_;  // peers in mesh
    time_cache::Time last_publish_{};
  };

  /**
   * Peer state for Gossipsub: tracks streams, topics, and mesh membership.
   */
  class Peer {
   public:
    Peer(PeerId peer_id, bool out);
    /** True if peer speaks only Floodsub (no gossipsub features). */
    bool isFloodsub() const;
    /** True if peer supports Gossipsub (v1.0+). */
    bool isGossipsub() const;
    /** True if peer supports Gossipsub v1.1 features. */
    bool isGossipsubv1_1() const;
    /** True if peer supports Gossipsub v1.2 features. */
    bool isGossipsubv1_2() const;
    PeerId peer_id_;
    bool out_;
    std::optional<PeerKind> peer_kind_;
    std::unordered_set<TopicHash, qtils::BytesStdHash> topics_;
    std::optional<StreamPtr> stream_out_;
    std::unordered_set<StreamPtr> streams_in_;
    std::optional<Rpc> batch_;
    bool writing_ = false;
    bool is_connecting_ = false;
    IDontWantCache<MessageId, qtils::BytesStdHash> dont_send_;
  };

  /**
   * Peer selection and shuffling utilities for mesh and gossip.
   */
  class ChoosePeers {
   public:
    /**
     * Choose a subset of peers matching predicate, shuffled, up to get_count().
     */
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
    /**
     * Choose a fixed number of peers matching predicate, shuffled.
     */
    std::deque<PeerPtr> choose(auto &&all,
                               const std::invocable<PeerPtr> auto &predicate,
                               size_t count) {
      return choose(std::forward<decltype(all)>(all), predicate, [&](size_t) {
        return count;
      });
    }
    /**
     * Shuffle a range of peers in-place.
     */
    void shuffle(auto &&r) {
      std::ranges::shuffle(std::forward<decltype(r)>(r), random_);
    }

   private:
    std::default_random_engine random_;
  };

  /**
   * Main Gossipsub protocol implementation.
   * Handles mesh maintenance, peer scoring, message propagation, and RPC
   * batching.
   */
  class Gossip : public std::enable_shared_from_this<Gossip>,
                 public BaseProtocol {
   public:
    /** Construct with host, crypto, config, and io context. */
    Gossip(std::shared_ptr<boost::asio::io_context> io_context,
           std::shared_ptr<host::BasicHost> host,
           std::shared_ptr<peer::IdentityManager> id_mgr,
           std::shared_ptr<crypto::CryptoProvider> crypto_provider,
           Config config);

    // BaseProtocol
    /** Advertise supported protocol IDs for stream negotiation. */
    StreamProtocols getProtocolIds() const override;
    /** Handle an inbound stream and start reading RPC frames. */
    void handle(std::shared_ptr<Stream> stream) override;

    /** Start event listeners, heartbeat, and score timers. */
    void start();

    /** Subscribe locally to a topic by hash (creates Topic if new). */
    std::shared_ptr<Topic> subscribe(TopicHash topic_hash, bool validate);
    /** Subscribe locally to a topic by string name. */
    std::shared_ptr<Topic> subscribe(std::string_view topic_hash,
                                     bool validate);

    /**
     * Publish message to topic.
     * Creates publish only topic if `subscribe` was not called before.
     */
    void publish(const TopicHash &topic_hash, BytesIn data);

    /** Publish a payload to a topic (signed, deduped, broadcast). */
    void publish(Topic &topic, BytesIn data);

    void validate(MessagePtr message, ValidationResult result);

    /**
     * Broadcast a message to peers following mesh/flood rules and optional
     * IDONTWANT side channel.
     */
    void broadcast(Topic &topic,
                   std::optional<PeerId> from,
                   const MessagePtr &message);

    /** Decode and process an inbound RPC frame from a peer. */
    bool onMessage(const PeerPtr &peer, BytesIn encoded);

    /** Get or create per-peer batch and schedule writer if idle. */
    Rpc &getBatch(const PeerPtr &peer);

    /** Ensure writer coroutine is running to flush the batch. */
    void checkWrite(const PeerPtr &peer);

    /** Record negotiated protocol features for the peer. */
    void updatePeerKind(const PeerPtr &peer, const ProtocolName &protocol);

    /** Add peer to mesh and enqueue GRAFT. */
    void graft(Topic &topic, const PeerPtr &peer);

    /** Enqueue PRUNE (with backoff for v1.1+) and update score/backoff. */
    void make_prune(Topic &topic, const PeerPtr &peer);

    /** Remove peer from mesh; optionally update backoff unconditionally. */
    void remove_peer_from_mesh(const TopicHash &topic_hash,
                               const PeerPtr &peer,
                               std::optional<Backoff> backoff,
                               bool always_update_backoff);

   private:
    /** Periodic maintenance: mesh balancing, gossip emission, cleanup. */
    void heartbeat();

    /** Compose and enqueue IHAVE gossip to eligible peers. */
    void emit_gossip();

    /** Update per-topic backoff using ticks + slack. */
    void update_backoff(Topic &topic, const PeerPtr &peer, Backoff backoff);
    /** Remaining backoff duration for a peer in a topic. */
    Backoff get_backoff_time(Topic &topic, const PeerPtr &peer);
    /** Whether peer is still in backoff (with slack) for a topic. */
    bool is_backoff_with_slack(Topic &topic, const PeerPtr &peer);

    /** Apply penalties to peers that failed IWANT promises. */
    void apply_iwant_penalties();

    /** Process IHAVE set and enqueue IWANTs under caps/thresholds. */
    bool handle_ihave(const PeerPtr &peer,
                      const gossipsub::pb::RPC &pb_message);

    std::shared_ptr<Topic> getOrCreateTopic(const TopicHash &topic_hash,
                                            bool validate,
                                            bool publish_only);

    // Dependencies and state
    std::shared_ptr<boost::asio::io_context> io_context_;
    std::shared_ptr<host::BasicHost> host_;
    std::shared_ptr<peer::IdentityManager> id_mgr_;
    std::shared_ptr<crypto::CryptoProvider> crypto_provider_;
    Config config_;
    StreamProtocols protocols_;
    event::Handle on_peer_connected_sub_;
    event::Handle on_peer_disconnected_sub_;
    std::unordered_map<TopicHash, std::shared_ptr<Topic>, qtils::BytesStdHash>
        topics_;
    std::unordered_map<PeerId, PeerPtr> peers_;
    PublishConfigSigning publish_config_;
    DuplicateCache<MessageId, qtils::BytesStdHash> duplicate_cache_;
    ChoosePeers choose_peers_;
    std::unordered_map<MessageId, MessageCacheEntry, qtils::BytesStdHash>
        message_cache_;
    GossipPromises<PeerPtr> gossip_promises_;
    Score score_;
    log::Logger logger_ = log::createLogger("Gossip");
    size_t heartbeat_ticks_ = 0;
    std::unordered_map<PeerPtr, size_t> count_received_ihave_;
    std::unordered_map<PeerPtr, size_t> count_sent_iwant_;
  };
}  // namespace libp2p::protocol::gossip
