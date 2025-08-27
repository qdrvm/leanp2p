/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/read.hpp>
#include <libp2p/basic/varint_prefix_reader.hpp>
#include <qtils/enum_error_code.hpp>

namespace libp2p {

  /**
   * Error codes for varint reading.
   */
  enum class VarintError {
    DECODE_SIZE,
    MESSAGE_TOO_LONG,
  };

  /**
   * Human-readable strings for `VarintError`.
   */
  Q_ENUM_ERROR_CODE(VarintError) {
    using E = decltype(e);
    switch (e) {
      case E::DECODE_SIZE:
        return "Error decoding varint";
      case E::MESSAGE_TOO_LONG:
        return "Message too long";
    }
    abort();
  }

  /**
   * Read a LEB128 \(`varint`\) prefix from `reader`.
   *
   * Reads one byte at a time until `VarintPrefixReader` reports `kReady`
   * or an error. On success, returns the decoded `uint64_t` value.
   *
   * Returns:
   * - `VarintError::DECODE_SIZE` if the prefix is malformed or incomplete.
   */
  inline CoroOutcome<uint64_t> readVarint(
      std::shared_ptr<basic::Reader> reader) {
    basic::VarintPrefixReader varint;
    std::array<uint8_t, 1> byte{};  // single-byte buffer for incremental read

    // Feed bytes until the state machine has a complete value or an error.
    while (varint.state() == varint.kUnderflow) {
      BOOST_OUTCOME_CO_TRY(co_await read(reader, byte));
      varint.consume(byte[0]);
    }

    // Any terminal state other than kReady is a decode error.
    if (varint.state() != varint.kReady) {
      co_return VarintError::DECODE_SIZE;
    }
    co_return varint.value();
  }

  /**
   * Read a length-prefixed message.
   *
   * Steps:
   * 1. Read a varint length prefix via `readVarint`.
   * 2. Validate it against `max_size` \(`32 MiB` by default\).
   * 3. Resize `message` and read exactly that many bytes into it.
   *
   * Returns:
   * - `VarintError::MESSAGE_TOO_LONG` if the decoded size exceeds `max_size`.
   * - Propagates errors from `readVarint` and `read`.
   */
  inline CoroOutcome<void> readVarintMessage(
      std::shared_ptr<basic::Reader> reader,
      Bytes &message,
      size_t max_size = (size_t{32} << 20)) {
    // Decode the length prefix.
    BOOST_OUTCOME_CO_TRY(auto size, co_await readVarint(reader));

    // Guard against excessive allocation and reads.
    if (size > max_size) {
      co_return VarintError::MESSAGE_TOO_LONG;
    }
    // Allocate and read the message payload.
    message.resize(size);
    BOOST_OUTCOME_CO_TRY(co_await read(reader, message));
    co_return outcome::success();
  }

}  // namespace libp2p
