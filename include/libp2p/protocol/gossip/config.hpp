/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/types.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/peer/stream_protocols.hpp>
#include <qtils/bytes_std_hash.hpp>
#include <unordered_map>
#include <unordered_set>

namespace libp2p::protocol::gossip {
  using TopicHash = Bytes;
  using MessageId = Bytes;
  using Seqno = uint64_t;
  using Backoff = std::chrono::seconds;

  constexpr double kDefaultDecayToZero = 0.1;

  inline const ProtocolName kProtocolFloodsub = "/floodsub/1.0.0";
  inline const ProtocolName kProtocolGossipsub = "/meshsub/1.0.0";
  inline const ProtocolName kProtocolGossipsubv1_1 = "/meshsub/1.1.0";
  inline const ProtocolName kProtocolGossipsubv1_2 = "/meshsub/1.2.0";

  enum class PeerKind : uint8_t {
    NotSupported,
    Floodsub,
    Gossipsub,
    Gossipsubv1_1,
    Gossipsubv1_2,
  };

  enum class ValidationMode {
    Strict,
    Anonymous,
  };

  enum class MessageAuthenticity {
    Signed,
    Anonymous,
  };

  struct Message {
    std::optional<PeerId> from;
    Bytes data;
    std::optional<Seqno> seqno;
    TopicHash topic;
    std::optional<Bytes> signature;

    std::optional<PeerId> received_from;
    std::optional<MessageId> message_id;
    std::unordered_set<PeerId> duplicate_peers;
  };

  enum class ValidationResult {
    Accept,
    Reject,
    Ignore,
  };

  using MessageIdFn = std::function<MessageId(const Message &)>;

  MessageId defaultMessageIdFn(const Message &message);

  struct TopicMeshConfig {
    size_t mesh_n = 6;
    size_t mesh_n_low = 5;
    size_t mesh_n_high = 12;
    size_t mesh_outbound_min = 2;
  };

  struct TopicScoreParams {
    double topic_weight = 0.5;
    double time_in_mesh_weight = 1;
    std::chrono::milliseconds time_in_mesh_quantum{1};
    double time_in_mesh_cap = 3600;
    double first_message_deliveries_weight = 1;
    double first_message_deliveries_decay = 0.5;
    double first_message_deliveries_cap = 2000;
    double mesh_message_deliveries_weight = -1;
    double mesh_message_deliveries_decay = 0.5;
    double mesh_message_deliveries_cap = 100;
    double mesh_message_deliveries_threshold = 20;
    std::chrono::milliseconds mesh_message_deliveries_window{10};
    std::chrono::seconds mesh_message_deliveries_activation{5};
    double mesh_failure_penalty_weight = -1;
    double mesh_failure_penalty_decay = 0.5;
    double invalid_message_deliveries_weight = -1;
    double invalid_message_deliveries_decay = 0.3;
  };

  struct ScoreConfig {
    double zero = 0;
    /// The score threshold below which gossip propagation is suppressed;
    /// should be negative.
    double gossip_threshold = -10;
    /// The score threshold below which we shouldn't publish when using flood
    /// publishing (also applies to fanout peers); should be negative and <=
    /// `gossip_threshold`.
    double publish_threshold = -50;
    /// The score threshold below which message processing is suppressed
    /// altogether, implementing an effective graylist according to peer score;
    /// should be negative and
    /// <= `publish_threshold`.
    double graylist_threshold = -80;
    /// The median mesh score threshold before triggering opportunistic
    /// grafting; this should have a small positive value.
    double opportunistic_graft_threshold = 20;

    std::unordered_map<TopicHash, TopicScoreParams, qtils::BytesStdHash> topics;
    double topic_score_cap = 3600;
    double app_specific_weight = 10;
    double behaviour_penalty_weight = -10;
    double behaviour_penalty_threshold = 0;
    double behaviour_penalty_decay = 0.2;
    std::chrono::milliseconds decay_interval = std::chrono::seconds{1};
    double decay_to_zero = kDefaultDecayToZero;
    std::chrono::seconds retain_score{3600};
    double slow_peer_weight = -0.2;
    double slow_peer_threshold = 0.0;
    double slow_peer_decay = 0.2;

    bool valid() const {
      if (gossip_threshold > 0) {
        return false;
      }
      if (publish_threshold > 0 or publish_threshold > gossip_threshold) {
        return false;
      }
      if (graylist_threshold > 0 or graylist_threshold > publish_threshold) {
        return false;
      }
      if (opportunistic_graft_threshold < 0) {
        return false;
      }
      return true;
    }
  };

  struct Config {
    // Fixes default field values with boost::di.
    Config() = default;

    size_t mesh_n_for_topic(const TopicHash &topic_hash) const;
    size_t mesh_n_low_for_topic(const TopicHash &topic_hash) const;
    size_t mesh_n_high_for_topic(const TopicHash &topic_hash) const;
    size_t mesh_outbound_min_for_topic(const TopicHash &topic_hash) const;

    MessageIdFn message_id_fn = defaultMessageIdFn;
    ValidationMode validation_mode = ValidationMode::Strict;
    MessageAuthenticity message_authenticity = MessageAuthenticity::Signed;

    /// Protocol versions
    std::unordered_map<ProtocolName, PeerKind> protocol_versions{
        {kProtocolFloodsub, PeerKind::Floodsub},
        {kProtocolGossipsub, PeerKind::Gossipsub},
        {kProtocolGossipsubv1_1, PeerKind::Gossipsubv1_1},
        {kProtocolGossipsubv1_2, PeerKind::Gossipsubv1_2},
    };

    /// Duplicates are prevented by storing message id's of known messages in an
    /// LRU time cache. This settings sets the time period that messages are
    /// stored in the cache. Duplicates can be received if duplicate messages
    /// are sent at a time greater than this setting apart. The default is 1
    /// minute.
    std::chrono::seconds duplicate_cache_time{60};

    TopicMeshConfig default_mesh_params;

    Backoff prune_backoff{60};
    Backoff unsubscribe_backoff{10};
    size_t backoff_slack = 1;
    Backoff graft_flood_threshold{10};

    std::chrono::seconds heartbeat_interval{1};
    /// Time to live for fanout peers (default is 60 seconds).
    std::chrono::seconds fanout_ttl{60};

    size_t retain_scores = 4;

    size_t history_length = 5;
    size_t history_gossip = 3;

    size_t gossip_retransimission = 3;

    size_t gossip_lazy = 6;
    double gossip_factor = 0.25;

    size_t max_ihave_length = 5000;
    size_t max_ihave_messages = 10;

    bool flood_publish = true;

    size_t idontwant_message_size_threshold = 1000;
    bool idontwant_on_publish = false;

    std::chrono::seconds iwant_followup_time{3};

    size_t opportunistic_graft_ticks = 60;
    size_t opportunistic_graft_peers = 2;

    ScoreConfig score;
  };
}  // namespace libp2p::protocol::gossip
