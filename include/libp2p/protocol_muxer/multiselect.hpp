/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "protocol_muxer.hpp"

namespace libp2p::protocol_muxer::multiselect {
  CoroOutcome<ProtocolName> selectOneOf(
      std::span<const ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream);

  /// Multiselect protocol implementation of ProtocolMuxer
  class Multiselect : public protocol_muxer::ProtocolMuxer {
   public:
    /// Implements coroutine version of ProtocolMuxer API
    CoroOutcome<peer::ProtocolName> selectOneOf(
        std::span<const peer::ProtocolName> protocols,
        std::shared_ptr<basic::ReadWriter> connection,
        bool is_initiator,
        bool negotiate_multistream) override;
  };
}  // namespace libp2p::protocol_muxer::multiselect
