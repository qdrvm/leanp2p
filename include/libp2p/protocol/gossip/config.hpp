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

  inline const ProtocolName kProtocolFloodsub = "/floodsub/1.0.0";

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

  struct Config {
    // Fixes default field values with boost::di.
    Config() = default;

    MessageIdFn message_id_fn = defaultMessageIdFn;
    ValidationMode validation_mode = ValidationMode::Strict;
    MessageAuthenticity message_authenticity = MessageAuthenticity::Signed;
    StreamProtocols protocols{
        kProtocolFloodsub,
    };

    /// Duplicates are prevented by storing message id's of known messages in an
    /// LRU time cache. This settings sets the time period that messages are
    /// stored in the cache. Duplicates can be received if duplicate messages
    /// are sent at a time greater than this setting apart. The default is 1
    /// minute.
    std::chrono::seconds duplicate_cache_time{60};
  };
}  // namespace libp2p::protocol::gossip
