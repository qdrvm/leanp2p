/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "protocol_muxer.hpp"

namespace libp2p::protocol_muxer::multiselect {
  /**
   * Select one protocol from a list using the multiselect negotiation.
   *
   * Inputs:
   * - protocols: ordered list of preferred protocols to try.
   * - connection: underlying stream implementing Reader/Writer.
   * - is_initiator: true if we propose protocols; false if we respond.
   * - negotiate_multistream: if true, perform multistream handshake first.
   *
   * Returns: agreed protocol name on success or an error on failure/violation.
   */
  CoroOutcome<ProtocolName> selectOneOf(
      std::span<const ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream);

  /// Multiselect protocol implementation of ProtocolMuxer
  class Multiselect : public protocol_muxer::ProtocolMuxer {
   public:
    /**
     * Implements the ProtocolMuxer API using multiselect negotiation.
     * Mirrors the free function above and delegates to it.
     */
    CoroOutcome<peer::ProtocolName> selectOneOf(
        std::span<const peer::ProtocolName> protocols,
        std::shared_ptr<basic::ReadWriter> connection,
        bool is_initiator,
        bool negotiate_multistream) override;
  };
}  // namespace libp2p::protocol_muxer::multiselect
