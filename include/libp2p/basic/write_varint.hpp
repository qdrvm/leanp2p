/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/encode_varint.hpp>
#include <libp2p/basic/write.hpp>

namespace libp2p {

  /**
   * Write a length\-prefixed message.
   * Steps:
   * 1. Varint\-encode the message length.
   * 2. Write the length prefix.
   * 3. Write the message payload.
   *
   * Precondition: `message.size()` fits into `uint64_t`.
   */
  inline CoroOutcome<void> writeVarintMessage(
      std::shared_ptr<basic::Writer> writer, BytesIn message) {
    // Write prefix, then payload
    BOOST_OUTCOME_CO_TRY(co_await write(writer, EncodeVarint{message.size()}));
    BOOST_OUTCOME_CO_TRY(co_await write(writer, message));
    co_return outcome::success();
  }
}  // namespace libp2p
