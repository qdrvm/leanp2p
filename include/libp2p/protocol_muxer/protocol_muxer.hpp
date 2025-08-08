/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <memory>

#include <libp2p/connection/stream.hpp>
#include <libp2p/peer/protocol.hpp>

namespace libp2p::protocol_muxer {
  /**
   * Allows to negotiate with the other side of the connection about the
   * protocols, which are going to be used in communication with it
   */
  class ProtocolMuxer {
   public:
    enum class Error {
      // cannot negotiate protocol
      NEGOTIATION_FAILED = 1,

      // error occured on this host's side
      INTERNAL_ERROR,

      // remote peer violated protocol
      PROTOCOL_VIOLATION,
    };

    /**
     * Coroutine version of selectOneOf
     * @param protocols - set of protocols, one of which should be chosen during
     * the negotiation
     * @param connection - connection for which the protocol is being chosen
     * @param is_initiator - true, if we initiated the connection and thus
     * taking lead in the Multiselect protocol; false otherwise
     * @param negotiate_multistream - true, if we need to negotiate multistream
     * itself, this happens with fresh raw connections
     * @return awaitable with chosen protocol or error
     */
    virtual CoroOutcome<peer::ProtocolName> selectOneOf(
        std::span<const peer::ProtocolName> protocols,
        std::shared_ptr<basic::ReadWriter> connection,
        bool is_initiator,
        bool negotiate_multistream) = 0;

    virtual ~ProtocolMuxer() = default;
  };
}  // namespace libp2p::protocol_muxer

OUTCOME_HPP_DECLARE_ERROR(libp2p::protocol_muxer, ProtocolMuxer::Error)
