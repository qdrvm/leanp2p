/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/types.hpp>
#include <libp2p/peer/peer_id.hpp>
#include <libp2p/peer/stream_protocols.hpp>

namespace libp2p::protocol::gossip {
  using TopicHash = Bytes;
  using MessageId = Bytes;
  using Seqno = uint64_t;
  using Backoff = std::chrono::seconds;

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
    // TODO...
  };

  enum class MessageAuthenticity {
    Signed,
    // TODO...
  };

  struct Message {
    std::optional<PeerId> from;
    Bytes data;
    std::optional<Seqno> seqno;
    TopicHash topic;
    std::optional<Bytes> signature;
  };

  using MessageIdFn = std::function<MessageId(const Message &)>;

  MessageId defaultMessageIdFn(const Message &message);

  struct TopicMeshConfig {
    size_t mesh_n = 6;
    size_t mesh_n_low = 5;
    size_t mesh_n_high = 12;
    size_t mesh_outbound_min = 2;
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

    std::chrono::seconds heartbeat_interval{1};

    size_t retain_scores = 4;

    size_t history_length = 5;
    size_t history_gossip = 3;

    size_t gossip_retransimission = 3;

    size_t gossip_lazy = 6;
    double gossip_factor = 0.25;

    size_t max_ihave_length = 5000;

    bool flood_publish = true;

    size_t idontwant_message_size_threshold = 1000;
    bool idontwant_on_publish = false;

    std::chrono::seconds iwant_followup_time{3};
  };
}  // namespace libp2p::protocol::gossip
