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
  enum class VarintError {
    DECODE_SIZE,
    MESSAGE_TOO_LONG,
  };
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

  inline CoroOutcome<uint64_t> readVarint(
      std::shared_ptr<basic::Reader> reader) {
    basic::VarintPrefixReader varint;
    while (varint.state() == varint.kUnderflow) {
      std::array<uint8_t, 1> byte;
      BOOST_OUTCOME_CO_TRY(co_await read(reader, byte));
      varint.consume(byte.at(0));
    }
    if (varint.state() != varint.kReady) {
      co_return VarintError::DECODE_SIZE;
    }
    co_return varint.value();
  }

  inline CoroOutcome<void> readVarintMessage(
      std::shared_ptr<basic::Reader> reader,
      Bytes &message,
      size_t max_size = 32 << 20) {
    BOOST_OUTCOME_CO_TRY(auto size, co_await readVarint(reader));
    if (size > max_size) {
      co_return VarintError::MESSAGE_TOO_LONG;
    }
    message.resize(size);
    BOOST_OUTCOME_CO_TRY(co_await read(reader, message));
    co_return outcome::success();
  }
}  // namespace libp2p
