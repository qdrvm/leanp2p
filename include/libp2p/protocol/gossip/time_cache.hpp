/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <chrono>
#include <deque>
#include <libp2p/protocol/gossip/config.hpp>
#include <qtils/bytes_std_hash.hpp>
#include <qtils/empty.hpp>
#include <unordered_map>

namespace libp2p::protocol::gossip::time_cache {
  using Ttl = std::chrono::milliseconds;
  using Clock = std::chrono::steady_clock;
  using Time = Clock::time_point;

  // Time-based TTL cache used for gossipsub message duplicate checks.
  // TTL semantics: entries expire a fixed time after insertion; reads do not
  // extend TTL.
  template <typename K, typename V, typename H = std::hash<K>>
  class TimeCache {
   public:
    // Construct cache with a given TTL duration.
    explicit TimeCache(Ttl ttl) : ttl_{ttl} {}

    // Number of live entries.
    [[nodiscard]] size_t size() const {
      return map_.size();
    }

    // Fast containment check (does not affect TTL).
    bool contains(const K &key) const {
      return map_.contains(key);
    }

    // Remove all entries that have expired by 'now'.
    void clearExpired(Time now = Clock::now()) {
      while (not expirations_.empty() and expirations_.front().first <= now) {
        map_.erase(expirations_.front().second);
        expirations_.pop_front();
      }
    }

    // Get value for key, inserting a default and scheduling its expiration if
    // absent.
    V &getOrDefault(const K &key, Time now = Clock::now()) {
      clearExpired(now);
      auto it = map_.find(key);
      if (it == map_.end()) {
        it = map_.emplace(key, V()).first;
        expirations_.emplace_back(now + ttl_, it);
      }
      return it->second;
    }

    // Remove the oldest (nearest-to-expire) entry; throws if empty.
    void popFront() {
      if (expirations_.empty()) {
        throw std::logic_error{"TimeCache::popFront empty"};
      }
      map_.erase(expirations_.front().second);
      expirations_.pop_front();
    }

   private:
    using Map = std::unordered_map<K, V, H>;

    Ttl ttl_;
    Map map_;
    std::deque<std::pair<Clock::time_point, typename Map::iterator>>
        expirations_;
  };

  // DuplicateCache: TTL-backed set for duplicate suppression.
  template <typename K, typename H = std::hash<K>>
  class DuplicateCache {
   public:
    // Items live for 'ttl' and expire independently of access.
    explicit DuplicateCache(Ttl ttl) : cache_{ttl} {}

    // Check if key is present.
    bool contains(const K &key) const {
      return cache_.contains(key);
    }

    // Insert key if absent; returns true if inserted, false if duplicate.
    bool insert(const K &key, Time now = Clock::now()) {
      cache_.clearExpired(now);
      if (cache_.contains(key)) {
        return false;
      }
      cache_.getOrDefault(key);
      return true;
    }

   private:
    TimeCache<K, qtils::Empty, H> cache_;
  };

  // IDontWantCache: bounded-capacity TTL set used to avoid re-sending messages.
  template <typename K, typename H = std::hash<K>>
  class IDontWantCache {
    static constexpr size_t kCapacity = 10000;
    static constexpr Ttl kTtl = std::chrono::seconds{3};

   public:
    // Purge expired entries.
    void clearExpired(Time now = Clock::now()) {
      cache_.clearExpired(now);
    }

    // Check if key is in the set.
    bool contains(const K &key) const {
      return cache_.contains(key);
    }

    // Insert key if absent; evicts the oldest element if capacity is reached.
    void insert(const K &key) {
      if (cache_.contains(key)) {
        return;
      }
      if (cache_.size() >= kCapacity) {
        cache_.popFront();
      }
      cache_.getOrDefault(key);
    }

   private:
    TimeCache<K, qtils::Empty, H> cache_{kTtl};
  };

  // GossipPromises: track peers who promised to deliver a message within TTL.
  template <typename Peer>
  class GossipPromises {
   public:
    // Create with a TTL for promises.
    explicit GossipPromises(Ttl ttl) : ttl_{ttl} {}

    // Check if we track promises for message_id.
    [[nodiscard]] bool contains(const MessageId &message_id) const {
      return map_.contains(message_id);
    }

    // Record a promise from 'peer' for 'message_id', expiring at now + ttl.
    void add(const MessageId &message_id,
             const Peer &peer,
             Time now = Clock::now()) {
      map_[message_id].emplace(peer, now + ttl_);
    }

    // Stop tracking promises for message_id.
    void remove(const MessageId &message_id) {
      map_.erase(message_id);
    }

    // Visit current peers that promised the message.
    void peers(const MessageId &message_id, const auto &f) {
      auto it = map_.find(message_id);
      if (it != map_.end()) {
        for (auto &p : it->second) {
          f(p.first);
        }
      }
    }

    // Expire promises, returning a histogram: peer -> number of expirations.
    auto clearExpired(Time now = Clock::now()) {
      std::unordered_map<Peer, size_t> result;
      for (auto it1 = map_.begin(); it1 != map_.end();) {
        auto &map2 = it1->second;
        for (auto it2 = map2.begin(); it2 != map2.end();) {
          if (it2->second < now) {
            ++result[it2->first];
            it2 = map2.erase(it2);
          } else {
            ++it2;
          }
        }
        if (map2.empty()) {
          it1 = map_.erase(it1);
        } else {
          ++it1;
        }
      }
      return result;
    }

   private:
    Ttl ttl_;
    std::unordered_map<MessageId,
                       std::unordered_map<Peer, Time>,
                       qtils::BytesStdHash>
        map_;
  };
}  // namespace libp2p::protocol::gossip::time_cache

namespace libp2p::protocol::gossip {
  using time_cache::DuplicateCache;
  using time_cache::GossipPromises;
  using time_cache::IDontWantCache;
  using time_cache::TimeCache;
}  // namespace libp2p::protocol::gossip
