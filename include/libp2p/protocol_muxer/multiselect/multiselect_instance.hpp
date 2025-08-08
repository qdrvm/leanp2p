/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/readwriter.hpp>
#include <libp2p/coro/coro.hpp>
#include <libp2p/protocol_muxer/multiselect.hpp>

#include "parser.hpp"

namespace soralog {
  class Logger;
}

namespace libp2p::protocol_muxer::multiselect {

  class Multiselect;

  /// Reusable instance of multiselect negotiation sessions
  class MultiselectInstance
      : public std::enable_shared_from_this<MultiselectInstance> {
   public:
    explicit MultiselectInstance(Multiselect &owner);

    /// Coroutine version of ProtocolMuxer API
    CoroOutcome<peer::ProtocolName> selectOneOf(
        std::span<const peer::ProtocolName> protocols,
        std::shared_ptr<basic::ReadWriter> connection,
        bool is_initiator,
        bool negotiate_multiselect);

   private:
    using Protocols = boost::container::small_vector<std::string, 4>;
    using Packet = std::shared_ptr<MsgBuf>;
    using Parser = detail::Parser;
    using MaybeResult = boost::optional<outcome::result<std::string>>;

    /// Coroutine versions of send and receive operations
    CoroOutcome<size_t> sendCoro(Packet packet);
    CoroOutcome<size_t> receiveCoro(size_t bytes_needed);
    Coro<MaybeResult> processMessagesCoro();

    /// Coroutine helper methods for protocol negotiation
    CoroOutcome<void> sendProtocolProposalCoro(
        std::shared_ptr<basic::ReadWriter> connection,
        bool multistream_negotiated,
        const std::string &protocol);

    CoroOutcome<peer::ProtocolName> processProtocolMessageCoro(
        std::shared_ptr<basic::ReadWriter> connection,
        bool is_initiator,
        bool multistream_negotiated,
        bool wait_for_protocol_reply,
        size_t current_protocol,
        boost::optional<size_t> &wait_for_reply_sent,
        const boost::container::small_vector<std::string, 4> &local_protocols,
        const Message &msg,
        boost::optional<std::shared_ptr<MsgBuf>> &na_response);

    /// Owner of this object, needed for reuse of instances
    Multiselect &owner_;

    /// Current round, helps enable Multiselect instance reuse (callbacks won't
    /// be passed to expired destination)
    size_t current_round_ = 0;

    /// List of protocols
    Protocols protocols_;

    /// Connection or stream
    std::shared_ptr<basic::ReadWriter> connection_;

    /// True for client-side instance
    bool is_initiator_ = false;

    /// True if multistream protocol version is negotiated (strict mode)
    bool multistream_negotiated_ = false;

    /// Client specific: true if protocol proposal was sent
    bool wait_for_protocol_reply_ = false;

    /// True if the dialog is closed, no more callbacks
    bool closed_ = false;

    /// Client specific: index of last protocol proposal sent
    size_t current_protocol_ = 0;

    /// Server specific: has value if negotiation was successful and
    /// the instance waits for write callback completion.
    /// Inside is index of protocol chosen
    boost::optional<size_t> wait_for_reply_sent_;

    /// Incoming messages parser
    Parser parser_;

    /// Read buffer
    std::shared_ptr<std::array<uint8_t, kMaxMessageSize>> read_buffer_;

    /// Write queue. Still needed because the underlying ReadWriter may not
    /// support buffered writes
    std::deque<Packet> write_queue_;

    /// True if waiting for write callback
    bool is_writing_ = false;

    /// Cache: serialized NA response
    boost::optional<Packet> na_response_;
  };

}  // namespace libp2p::protocol_muxer::multiselect
