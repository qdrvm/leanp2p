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
  };
}  // namespace libp2p::protocol::gossip
