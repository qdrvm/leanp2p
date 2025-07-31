/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/protocol_muxer/multiselect/multiselect_instance.hpp>

#include <cctype>

#include <fmt/ranges.h>
#include <boost/asio/use_awaitable.hpp>

#include <libp2p/log/logger.hpp>
#include <libp2p/protocol_muxer/multiselect/serializing.hpp>
#include <libp2p/protocol_muxer/protocol_muxer.hpp>

namespace libp2p::protocol_muxer::multiselect {

  namespace {
    const log::Logger &log() {
      static log::Logger logger = log::createLogger("Multiselect");
      return logger;
    }
  }  // namespace

  MultiselectInstance::MultiselectInstance(Multiselect &owner)
      : owner_(owner) {}

  boost::asio::awaitable<outcome::result<peer::ProtocolName>>
  MultiselectInstance::selectOneOf(
      std::span<const peer::ProtocolName> protocols,
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool negotiate_multiselect) {
    assert(!protocols.empty());
    assert(connection);

    // Use local variables instead of class members for the coroutine
    // implementation
    bool multistream_negotiated = !negotiate_multiselect;
    bool wait_for_protocol_reply = false;
    size_t current_protocol = 0;
    boost::optional<size_t> wait_for_reply_sent;

    // Store protocols in a local variable instead of using the member variable
    boost::container::small_vector<std::string, 4> local_protocols(
        protocols.begin(), protocols.end());

    // Only reset the parser since it's used for state tracking
    parser_.reset();

    // Local read buffer - avoid using the shared class member
    auto read_buffer = std::make_shared<std::array<uint8_t, kMaxMessageSize>>();

    // Initial protocol negotiation
    if (is_initiator) {
      // Send the first protocol proposal
      auto result =
          co_await sendProtocolProposalCoro(connection,
                                            multistream_negotiated,
                                            local_protocols[current_protocol]);
      if (!result) {
        co_return result.error();
      }
      wait_for_protocol_reply = true;
    } else if (negotiate_multiselect) {
      // Send opening protocol ID (server side)
      auto msg = detail::createMessage(kProtocolId);
      if (!msg) {
        co_return msg.error();
      }
      auto packet = std::make_shared<MsgBuf>(msg.value());
      try {
        auto ec =
            co_await connection->writeSome(BytesIn(*packet), packet->size());
        if (ec) {
          co_return ec;
        }
      } catch (const std::exception &e) {
        log()->error("Error writing opening protocol ID: {}", e.what());
        co_return std::make_error_code(std::errc::io_error);
      }
    }

    // Cache for NA response - local to this coroutine
    boost::optional<std::shared_ptr<MsgBuf>> na_response;

    // Main negotiation loop
    while (true) {
      // Read data from the connection
      auto bytes_needed = parser_.bytesNeeded();
      if (bytes_needed > kMaxMessageSize) {
        SL_TRACE(log(),
                 "rejecting incoming traffic, too large message ({})",
                 bytes_needed);
        co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
      }

      BytesOut span(*read_buffer);
      span = span.first(static_cast<Parser::IndexType>(bytes_needed));

      try {
        auto read_result = co_await connection->read(span, bytes_needed);
        if (!read_result) {
          co_return read_result.error();
        }

        auto bytes_read = read_result.value();
        if (bytes_read > read_buffer->size()) {
          log()->error("selectOneOfCoro(): invalid state");
          co_return ProtocolMuxer::Error::INTERNAL_ERROR;
        }

        BytesIn data_span(*read_buffer);
        data_span = data_span.first(bytes_read);

        auto state = parser_.consume(data_span);
        if (state == Parser::kOverflow) {
          SL_TRACE(log(), "peer error: parser overflow");
          co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
        }
        if (state != Parser::kReady) {
          continue;  // Need more data
        }

        // Process the received messages
        for (const auto &msg : parser_.messages()) {
          switch (msg.type) {
            case Message::kProtocolName: {
              // Process protocol proposal/acceptance
              auto result =
                  co_await processProtocolMessageCoro(connection,
                                                      is_initiator,
                                                      multistream_negotiated,
                                                      wait_for_protocol_reply,
                                                      current_protocol,
                                                      wait_for_reply_sent,
                                                      local_protocols,
                                                      msg,
                                                      na_response);

              // If we got a protocol or an error, return it
              if (result) {
                co_return result;
              }
              break;
            }
            case Message::kRightProtocolVersion:
              multistream_negotiated = true;
              break;
            case Message::kNAMessage: {
              // Handle NA
              if (is_initiator) {
                if (current_protocol < local_protocols.size()) {
                  SL_DEBUG(log(),
                           "protocol {} was not accepted by peer",
                           local_protocols[current_protocol]);
                }

                // Try the next protocol
                ++current_protocol;

                if (current_protocol < local_protocols.size()) {
                  auto result = co_await sendProtocolProposalCoro(
                      connection,
                      multistream_negotiated,
                      local_protocols[current_protocol]);
                  if (!result) {
                    co_return result.error();
                  }
                  wait_for_protocol_reply = true;
                } else {
                  // No more protocols to propose
                  SL_DEBUG(log(),
                           "Failed to negotiate protocols: {}",
                           fmt::join(local_protocols, ", "));
                  co_return ProtocolMuxer::Error::NEGOTIATION_FAILED;
                }
              } else {
                // Server side
                SL_DEBUG(log(), "Unexpected NA received by server");
                co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
              }
              break;
            }
            case Message::kWrongProtocolVersion:
              co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
              break;
            default:
              co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
              break;
          }
        }

        // Reset parser for next messages
        parser_.reset();
      } catch (const std::exception &e) {
        log()->error("Error reading data: {}", e.what());
        co_return std::make_error_code(std::errc::io_error);
      }
    }

    // This should not be reached in normal operation
    co_return ProtocolMuxer::Error::INTERNAL_ERROR;
  }

  boost::asio::awaitable<outcome::result<void>>
  MultiselectInstance::sendProtocolProposalCoro(
      std::shared_ptr<basic::ReadWriter> connection,
      bool multistream_negotiated,
      const std::string &protocol) {
    // Create the protocol proposal message based on negotiation state
    outcome::result<MsgBuf> msg_res =
        outcome::failure(std::make_error_code(std::errc::invalid_argument));
    if (!multistream_negotiated) {
      std::array<std::string_view, 2> a({kProtocolId, protocol});
      msg_res = detail::createMessage(a, false);
    } else {
      msg_res = detail::createMessage(protocol);
    }

    if (!msg_res) {
      co_return msg_res.error();
    }

    // Send the message
    auto packet = std::make_shared<MsgBuf>(msg_res.value());
    try {
      auto ec =
          co_await connection->writeSome(BytesIn(*packet), packet->size());
      if (ec) {
        co_return ec;
      }
    } catch (const std::exception &e) {
      log()->error("Error writing protocol proposal: {}", e.what());
      co_return std::make_error_code(std::errc::io_error);
    }

    co_return outcome::success();
  }

  boost::asio::awaitable<outcome::result<peer::ProtocolName>>
  MultiselectInstance::processProtocolMessageCoro(
      std::shared_ptr<basic::ReadWriter> connection,
      bool is_initiator,
      bool multistream_negotiated,
      bool wait_for_protocol_reply,
      size_t current_protocol,
      boost::optional<size_t> &wait_for_reply_sent,
      const boost::container::small_vector<std::string, 4> &local_protocols,
      const Message &msg,
      boost::optional<std::shared_ptr<MsgBuf>> &na_response) {
    // Handle protocol name message
    if (is_initiator) {
      // Client side
      if (wait_for_protocol_reply) {
        if (current_protocol < local_protocols.size()
            && local_protocols[current_protocol] == msg.content) {
          // Successful client side negotiation
          co_return std::string(msg.content);
        }
      }
      co_return ProtocolMuxer::Error::PROTOCOL_VIOLATION;
    }

    // Server side
    size_t idx = 0;
    for (const auto &p : local_protocols) {
      if (p == msg.content) {
        // Successful server side negotiation
        wait_for_reply_sent = idx;

        // Send protocol acceptance
        auto accept_msg = detail::createMessage(msg.content);
        if (!accept_msg) {
          co_return accept_msg.error();
        }

        auto packet = std::make_shared<MsgBuf>(accept_msg.value());
        try {
          auto ec =
              co_await connection->writeSome(BytesIn(*packet), packet->size());
          if (ec) {
            co_return ec;
          }
          // Protocol negotiation successful
          co_return local_protocols[wait_for_reply_sent.value()];
        } catch (const std::exception &e) {
          log()->error("Error writing protocol acceptance: {}", e.what());
          co_return std::make_error_code(std::errc::io_error);
        }
      }
      ++idx;
    }

    // Not found, send NA
    SL_DEBUG(log(), "unknown protocol {} proposed by client", msg.content);
    if (!na_response) {
      auto na_msg = detail::createMessage(kNA);
      if (!na_msg) {
        co_return na_msg.error();
      }
      na_response = std::make_shared<MsgBuf>(na_msg.value());
    }

    try {
      auto ec = co_await connection->writeSome(BytesIn(*na_response.value()),
                                               na_response.value()->size());
      if (ec) {
        co_return ec;
      }
      // NA sent successfully, continue with protocol negotiation
      co_return outcome::success();
    } catch (const std::exception &e) {
      log()->error("Error sending NA response: {}", e.what());
      co_return std::make_error_code(std::errc::io_error);
    }
  }

}  // namespace libp2p::protocol_muxer::multiselect
