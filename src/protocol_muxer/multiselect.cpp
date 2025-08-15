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

  inline CoroOutcome<void> write1(std::shared_ptr<basic::Writer> connection,
                                  std::string_view protocol) {
    auto message = detail::createMessage(protocol).value();
    co_return co_await write(connection, message);
  }

  CoroOutcome<ProtocolName> selectOneOf(
      std::span<const ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream) {
    size_t i = 0;
    if (protocols.empty()) {
      co_return ProtocolMuxer::Error::NEGOTIATION_FAILED;
    }
    Bytes message;
    if (negotiate_multistream) {
      BOOST_OUTCOME_CO_TRY(co_await write1(connection, kProtocolId));
    }
    if (is_initiator) {
      BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocols[i]));
    }
    if (negotiate_multistream) {
      BOOST_OUTCOME_CO_TRY(auto protocol, co_await read1(connection, message));
      if (protocol != kProtocolId) {
        co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
      }
    }
    if (is_initiator) {
      while (true) {
        BOOST_OUTCOME_CO_TRY(auto protocol,
                             co_await read1(connection, message));
        if (protocol == kNA) {
          ++i;
          if (i >= protocols.size()) {
            co_return ProtocolMuxer::Error::NEGOTIATION_FAILED;
          }
          BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocols[i]));
        }
        if (protocol != protocols[i]) {
          co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
        }
        co_return protocol;
      }
    } else {
      while (true) {
        BOOST_OUTCOME_CO_TRY(auto protocol,
                             co_await read1(connection, message));
        if (std::ranges::contains(protocols, protocol)) {
          BOOST_OUTCOME_CO_TRY(co_await write1(connection, protocol));
          co_return protocol;
        }
        BOOST_OUTCOME_CO_TRY(co_await write1(connection, kNA));
      }
    }
  }

  CoroOutcome<peer::ProtocolName> Multiselect::selectOneOf(
      std::span<const peer::ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multistream) {
    co_return co_await multiselect::selectOneOf(
        protocols, connection, is_initiator, negotiate_multistream);
  }
}  // namespace libp2p::protocol_muxer::multiselect
