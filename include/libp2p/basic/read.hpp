/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/reader.hpp>

namespace libp2p {
  inline CoroOutcome<void> read(std::shared_ptr<basic::Reader> reader,
                                BytesOut out) {
    while (not out.empty()) {
      BOOST_OUTCOME_CO_TRY(auto n, co_await reader->readSome(out));
      if (n == 0) {
        throw std::logic_error{"libp2p::read zero bytes read"};
      }
      if (n > out.size()) {
        throw std::logic_error{"libp2p::read too much bytes read"};
      }
      out = out.subspan(n);
    }
    co_return outcome::success();
  }
}  // namespace libp2p
