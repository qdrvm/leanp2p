/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/protocol_muxer/multiselect.hpp>

#include <libp2p/basic/read_varint.hpp>
#include <libp2p/basic/write.hpp>
#include <libp2p/log/logger.hpp>
#include <libp2p/protocol_muxer/multiselect/common.hpp>
#include <libp2p/protocol_muxer/multiselect/serializing.hpp>
#include <qtils/bytestr.hpp>

namespace libp2p::protocol_muxer::multiselect {
  namespace {
    const log::Logger &log() {
      static log::Logger logger = log::createLogger("Multiselect");
      return logger;
    }
  }  // namespace

  // Read a single varint-length-delimited line (ending with '\n') from the
  // connection and return it without the trailing newline. If a newline is not
  // present, treat it as a protocol violation.
  inline CoroOutcome<ProtocolName> read1(
      std::shared_ptr<basic::Reader> connection, Bytes &message) {
    BOOST_OUTCOME_CO_TRY(
        co_await readVarintMessage(connection, message, kMaxMessageSize));
    if (message.empty() or message.back() != kNewLine) {
      co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
    }
    co_return std::string{
        qtils::byte2str(message).substr(0, message.size() - 1)};
  }

  // Serialize and write a single varint-length-delimited line with trailing
  // '\n'. Payload can be a protocol id or a control message like "na".
  inline CoroOutcome<void> write1(std::shared_ptr<basic::Writer> connection,
                                  std::string_view protocol) {
    auto message = detail::createMessage(protocol).value();
    co_return co_await write(connection, message);
  }

  // Perform multiselect negotiation to choose one protocol from the provided
  // list. Optionally performs multistream handshake first.
  // Initiator: proposes protocols in order until accepted or list is exhausted.
  // Responder: accepts a supported protocol by echoing it back, otherwise sends
  // "na" to request another proposal.
  CoroOutcome<ProtocolName> selectOneOf(
      std::span<const ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream) {
    // Index of the currently proposed protocol on the initiator side.
    size_t i = 0;
    if (protocols.empty()) {
      co_return ProtocolMuxer::Error::NEGOTIATION_FAILED;
    }
    // Temporary buffer for inbound messages.
    Bytes message;
    if (negotiate_multistream) {
      // Initiate multistream handshake by sending the multistream protocol id.
      BOOST_OUTCOME_CO_TRY(co_await write1(connection, kProtocolId));
    }
    if (is_initiator) {
      // Propose the first protocol immediately.
      BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocols[i]));
    }
    if (negotiate_multistream) {
      // Expect the peer to echo the multistream protocol id.
      BOOST_OUTCOME_CO_TRY(auto protocol, co_await read1(connection, message));
      if (protocol != kProtocolId) {
        co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
      }
    }
    if (is_initiator) {
      while (true) {
        // Read responder's decision: either the accepted protocol or "na".
        BOOST_OUTCOME_CO_TRY(auto protocol,
                             co_await read1(connection, message));
        if (protocol == kNA) {
          // Proposal rejected: advance to the next protocol, if any.
          ++i;
          if (i >= protocols.size()) {
            co_return ProtocolMuxer::Error::NEGOTIATION_FAILED;
          }
          BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocols[i]));
        }
        // Any response other than the exact protocol we proposed is a violation.
        if (protocol != protocols[i]) {
          co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
        }
        // Peer accepted: negotiation succeeded.
        co_return protocol;
      }
    } else {
      while (true) {
        // Read initiator's proposal and check if we support it.
        BOOST_OUTCOME_CO_TRY(auto protocol,
                             co_await read1(connection, message));
        if (std::ranges::contains(protocols, protocol)) {
          // Accept by echoing the proposed protocol back.
          BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocol));
          co_return protocol;
        }
        // Not supported: reply with "na" and wait for the next proposal.
        BOOST_OUTCOME_CO_TRY(co_await write1(connection, kNA));
      }
    }
  }

  CoroOutcome<peer::ProtocolName> Multiselect::selectOneOf(
      std::span<const peer::ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream) {
    // Thin wrapper delegating to the free-function implementation above.
    co_return co_await multiselect::selectOneOf(
        protocols, connection, is_initiator, negotiate_multistream);
  }
}  // namespace libp2p::protocol_muxer::multiselect
