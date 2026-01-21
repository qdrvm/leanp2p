/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/protocol/gossip/config.hpp>
#include <libp2p/protocol/gossip/time_cache.hpp>
#include <unordered_map>
#include <unordered_set>
#include <variant>

namespace libp2p::protocol::gossip::score {
  using Duration = std::chrono::milliseconds;
  using Clock = std::chrono::steady_clock;
  using Time = Clock::time_point;

  constexpr std::chrono::seconds kTimeCacheDuration{120};

  /// The reason a Gossipsub message has been rejected.
  enum class RejectReason {
    /// The message failed the configured validation during decoding.
    ValidationError,
    /// The message source is us.
    SelfOrigin,
    /// The peer that sent the message was blacklisted.
    BlackListedPeer,
    /// The source (from field) of the message was blacklisted.
    BlackListedSource,
    /// The validation was ignored.
    ValidationIgnored,
    /// The validation failed.
    ValidationFailed,
  };

  struct DeliveryStatusUnknown {};
  struct DeliveryStatusValid {
    Time time;
  };
  struct DeliveryStatusInvalid {};
  struct DeliveryStatusIgnored {};
  using DeliveryStatus = std::variant<DeliveryStatusUnknown,
                                      DeliveryStatusValid,
                                      DeliveryStatusInvalid,
                                      DeliveryStatusIgnored>;

  struct DeliveryRecord {
    DeliveryStatus status{};           // default Unknown
    Time first_seen{};                 // set on first sighting
    std::unordered_set<PeerId> peers;  // peers seen before validation
  };

  struct MeshActive {
    Time graft_time;
    Duration mesh_time;
  };

  struct TopicStats {
    std::optional<MeshActive> mesh_active;
    double first_message_deliveries = 0;
    bool mesh_message_deliveries_active = false;
    double mesh_message_deliveries = 0;
    double mesh_failure_penalty = 0;
    double invalid_message_deliveries = 0;
  };

  struct PeerStats {
    std::optional<Time> expires_at;
    std::unordered_map<TopicHash, TopicStats, qtils::BytesStdHash> topics;
    double behaviour_penalty = 0;
    double application_score = 0;
    double slow_peer_penalty = 0;
  };

  class Score {
   public:
    // Construct with immutable parameters; delivery records are retained for a
    // short window.
    explicit Score(ScoreConfig params)
        : params_{std::move(params)}, deliveries_{kTimeCacheDuration} {}

    // Fast threshold check helper.
    bool below(const PeerId &peer_id, double threshold) {
      return score(peer_id) < threshold;
    }

    PeerStats *getPeerStats(const PeerId &peer_id) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return nullptr;
      }
      return &it->second;
    }

    // Compute the full score for a peer by aggregating topic weights and global
    // terms. Terms correspond to spec: P1 (time in mesh), P2 (first msg
    // deliveries), P3 (mesh delivery deficit), P3b (mesh failure penalty), P4
    // (invalids^2), P5 (app), P7 (behaviour penalty^2), plus slow peer penalty.
    double score(const PeerId &peer_id) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return 0;
      }
      auto &peer_stats = it->second;
      double score = 0;
      for (auto &[topic, topic_stats] : peer_stats.topics) {
        auto topic_it = params_.topics.find(topic);
        if (topic_it == params_.topics.end()) {
          continue;
        }
        auto &topic_params = topic_it->second;
        double topic_score = 0;
        if (topic_stats.mesh_active.has_value()) {
          auto as_secs_f64 = [](auto t) {
            return static_cast<double>(
                std::chrono::duration_cast<std::chrono::seconds>(t).count());
          };
          auto p1 =
              std::min(as_secs_f64(topic_stats.mesh_active->mesh_time)
                           / as_secs_f64(topic_params.time_in_mesh_quantum),
                       topic_params.time_in_mesh_cap);
          topic_score += p1 * topic_params.time_in_mesh_weight;
        }
        auto p2 = std::min(topic_stats.first_message_deliveries,
                           topic_params.first_message_deliveries_cap);
        topic_score += p2 * topic_params.first_message_deliveries_weight;
        if (topic_stats.mesh_message_deliveries_active
            and topic_stats.mesh_message_deliveries
                    < topic_params.mesh_message_deliveries_threshold) {
          auto deficit = topic_params.mesh_message_deliveries_threshold
                       - topic_stats.mesh_message_deliveries;
          auto p3 = deficit * deficit;
          topic_score += p3 * topic_params.mesh_message_deliveries_weight;
        }
        auto p3b = topic_stats.mesh_failure_penalty;
        topic_score += p3b * topic_params.mesh_failure_penalty_weight;
        auto p4 = topic_stats.invalid_message_deliveries
                * topic_stats.invalid_message_deliveries;
        topic_score += p4 * topic_params.invalid_message_deliveries_weight;
        score += topic_score * topic_params.topic_weight;
      }
      if (params_.topic_score_cap > 0 and score > params_.topic_score_cap) {
        score = params_.topic_score_cap;
      }
      auto p5 = peer_stats.application_score;
      score += p5 * params_.app_specific_weight;
      if (peer_stats.behaviour_penalty > params_.behaviour_penalty_threshold) {
        auto excess =
            peer_stats.behaviour_penalty - params_.behaviour_penalty_threshold;
        auto p7 = excess * excess;
        score += p7 * params_.behaviour_penalty_weight;
      }
      if (peer_stats.slow_peer_penalty > params_.slow_peer_threshold) {
        auto excess =
            peer_stats.slow_peer_penalty - params_.slow_peer_threshold;
        score += excess * params_.slow_peer_weight;
      }
      return score;
    }

    void addPenalty(const PeerId &peer_id, size_t count) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      it->second.behaviour_penalty += static_cast<double>(count);
    }

    void graft(const PeerId &peer_id, const TopicHash &topic) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      if (auto *topic_stats = statsOrDefault(it->second, topic)) {
        topic_stats->mesh_active = MeshActive{
            .graft_time = Clock::now(),
            .mesh_time = {},
        };
        topic_stats->mesh_message_deliveries_active = false;
      }
    }

    void prune(const PeerId &peer_id, const TopicHash &topic) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      if (auto *topic_stats = statsOrDefault(it->second, topic)) {
        auto &threshold =
            params_.topics.at(topic).mesh_message_deliveries_threshold;
        if (topic_stats->mesh_message_deliveries_active
            and topic_stats->mesh_message_deliveries < threshold) {
          auto deficit = threshold - topic_stats->mesh_message_deliveries;
          topic_stats->mesh_failure_penalty += deficit * deficit;
        }
        topic_stats->mesh_message_deliveries_active = false;
        topic_stats->mesh_active.reset();
      }
    }

    // Called when a duplicate of msg_id was received from peer_id on topic.
    void duplicateMessage(const PeerId &peer_id,
                          const MessageId &msg_id,
                          const TopicHash &topic) {
      auto &record = deliveries_.getOrDefault(msg_id);
      if (record.peers.contains(peer_id)) {
        return;
      }
      if (std::holds_alternative<DeliveryStatusUnknown>(record.status)) {
        record.peers.emplace(peer_id);
      } else if (auto *valid =
                     std::get_if<DeliveryStatusValid>(&record.status)) {
        record.peers.emplace(peer_id);
        mark_duplicate_message_delivery(peer_id, topic, valid->time);
      } else if (std::holds_alternative<DeliveryStatusInvalid>(record.status)) {
        markInvalidMessageDelivery(peer_id, topic);
      }
    }

    // Mark message as valid at current time, update first/mesh deliveries.
    void validateMessage(const PeerId &peer_id,
                         const MessageId &msg_id,
                         const TopicHash &topic) {
      deliveries_.getOrDefault(msg_id);
    }

    void deliver_message(const PeerId &from,
                         const MessageId &msg_id,
                         const TopicHash &topic_hash) {
      mark_first_message_delivery(from, topic_hash);
      auto &record = deliveries_.getOrDefault(msg_id);
      if (not std::holds_alternative<DeliveryStatusUnknown>(record.status)) {
        return;
      }
      record.status = DeliveryStatusValid{Clock::now()};
      for (auto &peer : record.peers) {
        if (peer != from) {
          mark_duplicate_message_delivery(peer, topic_hash, std::nullopt);
        }
      }
    }

    void rejectInvalidMessage(const PeerId &from, const TopicHash &topic_hash) {
      markInvalidMessageDelivery(from, topic_hash);
    }

    // Reject a message.
    void rejectMessage(const PeerId &from,
                       const MessageId &message_id,
                       const TopicHash &topic_hash,
                       RejectReason reason) {
      // these messages are not tracked, but the peer is penalized as they are
      // invalid
      if (reason == RejectReason::ValidationError
          or reason == RejectReason::SelfOrigin) {
        rejectInvalidMessage(from, topic_hash);
        return;
      }
      // we ignore those messages, so do nothing.
      if (reason == RejectReason::BlackListedPeer
          or reason == RejectReason::BlackListedSource) {
        return;
      }
      auto &record = deliveries_.getOrDefault(message_id);
      // Multiple peers can now reject the same message as we track which peers
      // send us the message. If we have already updated the status, return.
      if (not std::holds_alternative<DeliveryStatusUnknown>(record.status)) {
        return;
      }
      if (reason == RejectReason::ValidationIgnored) {
        // we were explicitly instructed by the validator to ignore the message
        // but not penalize the peer
        record.status = DeliveryStatusIgnored{};
        record.peers.clear();
        return;
      }
      // mark the message as invalid and penalize peers that have already
      // forwarded it.
      record.status = DeliveryStatusInvalid{};
      // release the delivery time tracking map to free some memory early
      auto peers = std::exchange(record.peers, {});
      markInvalidMessageDelivery(from, topic_hash);
      for (auto &peer_id : peers) {
        markInvalidMessageDelivery(peer_id, topic_hash);
      }
    }

    void connect(const PeerId &peer_id) {
      peer_stats_[peer_id].expires_at.reset();
    }

    void disconnect(const PeerId &peer_id) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      if (score(peer_id) > 0) {
        peer_stats_.erase(peer_id);
        return;
      }
      for (auto &[topic, topic_stats] : it->second.topics) {
        topic_stats.first_message_deliveries = 0;
        auto topic_it = params_.topics.find(topic);
        if (topic_it != params_.topics.end()) {
          auto threshold = topic_it->second.mesh_message_deliveries_threshold;
          if (topic_stats.mesh_active.has_value()
              and topic_stats.mesh_message_deliveries_active
              and topic_stats.mesh_message_deliveries < threshold) {
            auto deficit = threshold - topic_stats.mesh_message_deliveries;
            topic_stats.mesh_failure_penalty += deficit * deficit;
          }
        }
        topic_stats.mesh_active.reset();
        topic_stats.mesh_message_deliveries_active = false;
      }
      it->second.expires_at = Clock::now() + params_.retain_score;
    }

    void onDecay() {
      auto now = Clock::now();
      for (auto it = peer_stats_.begin(); it != peer_stats_.end();) {
        auto &peer_stats = it->second;
        if (it->second.expires_at.has_value()) {
          if (*it->second.expires_at < now) {
            it = peer_stats_.erase(it);
          } else {
            ++it;
          }
        } else {
          ++it;
          for (auto &[topic, topic_stats] : peer_stats.topics) {
            auto topic_it = params_.topics.find(topic);
            if (topic_it == params_.topics.end()) {
              continue;
            }
            auto &topic_params = topic_it->second;
            topic_stats.first_message_deliveries *=
                topic_params.first_message_deliveries_decay;
            if (topic_stats.first_message_deliveries < params_.decay_to_zero) {
              topic_stats.first_message_deliveries = 0.0;
            }
            topic_stats.mesh_message_deliveries *=
                topic_params.mesh_message_deliveries_decay;
            if (topic_stats.mesh_message_deliveries < params_.decay_to_zero) {
              topic_stats.mesh_message_deliveries = 0.0;
            }
            topic_stats.mesh_failure_penalty *=
                topic_params.mesh_failure_penalty_decay;
            if (topic_stats.mesh_failure_penalty < params_.decay_to_zero) {
              topic_stats.mesh_failure_penalty = 0.0;
            }
            topic_stats.invalid_message_deliveries *=
                topic_params.invalid_message_deliveries_decay;
            if (topic_stats.invalid_message_deliveries
                < params_.decay_to_zero) {
              topic_stats.invalid_message_deliveries = 0.0;
            }
            if (topic_stats.mesh_active.has_value()) {
              topic_stats.mesh_active->mesh_time =
                  std::chrono::duration_cast<Duration>(
                      now - topic_stats.mesh_active->graft_time);
              if (topic_stats.mesh_active->mesh_time
                  > topic_params.mesh_message_deliveries_activation) {
                topic_stats.mesh_message_deliveries_active = true;
              }
            }
          }
          peer_stats.behaviour_penalty *= params_.behaviour_penalty_decay;
          if (peer_stats.behaviour_penalty < params_.decay_to_zero) {
            peer_stats.behaviour_penalty = 0;
          }
          peer_stats.slow_peer_penalty *= params_.slow_peer_decay;
          if (peer_stats.slow_peer_penalty < params_.decay_to_zero) {
            peer_stats.slow_peer_penalty = 0;
          }
        }
      }
    }

   private:
    TopicStats *statsOrDefault(PeerStats &peer, const TopicHash &topic) const {
      auto it = peer.topics.find(topic);
      if (it == peer.topics.end() && params_.topics.contains(topic)) {
        it = peer.topics.emplace(topic, TopicStats{}).first;
      }
      return it == peer.topics.end() ? nullptr : &it->second;
    }

    /// Increments the "first message deliveries" counter for all scored topics
    /// the message is published in, as well as the "mesh message deliveries"
    /// counter, if the peer is in the mesh for the topic.
    void mark_first_message_delivery(const PeerId &peer_id,
                                     const TopicHash &topic_hash) {
      if (auto *peer_stats = getPeerStats(peer_id)) {
        if (auto *topic_stats = statsOrDefault(*peer_stats, topic_hash)) {
          auto &topic_params = params_.topics.at(topic_hash);
          topic_stats->first_message_deliveries =
              std::min(topic_stats->first_message_deliveries + 1.0,
                       topic_params.first_message_deliveries_cap);
          if (topic_stats->mesh_active) {
            topic_stats->mesh_message_deliveries =
                std::min(topic_stats->mesh_message_deliveries + 1.0,
                         topic_params.mesh_message_deliveries_cap);
          }
        }
      }
    }

    void mark_duplicate_message_delivery(const PeerId &peer_id,
                                         const TopicHash &topic,
                                         std::optional<Time> validated_time) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      if (auto *topic_stats = statsOrDefault(it->second, topic)) {
        if (topic_stats->mesh_active) {
          auto &topic_params = params_.topics.at(topic);
          if (validated_time) {
            auto now = Clock::now();
            auto window_time =
                *validated_time + topic_params.mesh_message_deliveries_window;
            if (now > window_time) {
              return;
            }
            topic_stats->mesh_message_deliveries =
                std::min(topic_stats->mesh_message_deliveries + 1,
                         topic_params.mesh_message_deliveries_cap);
          }
        }
      }
    }

    void markInvalidMessageDelivery(const PeerId &peer_id,
                                    const TopicHash &topic) {
      auto it = peer_stats_.find(peer_id);
      if (it == peer_stats_.end()) {
        return;
      }
      if (auto *topic_stats = statsOrDefault(it->second, topic)) {
        topic_stats->invalid_message_deliveries += 1;
      }
    }

    ScoreConfig params_;
    std::unordered_map<PeerId, PeerStats> peer_stats_;
    TimeCache<MessageId, DeliveryRecord, qtils::BytesStdHash> deliveries_;
  };
}  // namespace libp2p::protocol::gossip::score

namespace libp2p::protocol::gossip {
  using score::Score;
}  // namespace libp2p::protocol::gossip
