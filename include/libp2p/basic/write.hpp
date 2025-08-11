/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/writer.hpp>

namespace libp2p {
  inline CoroOutcome<void> write(std::shared_ptr<basic::Writer> writer,
                                 BytesIn in) {
    while (not in.empty()) {
      BOOST_OUTCOME_CO_TRY(auto n, co_await writer->writeSome(in));
      if (n == 0) {
        throw std::logic_error{"libp2p::write zero bytes written"};
      }
      if (n > in.size()) {
        throw std::logic_error{"libp2p::write too much bytes written"};
      }
      in = in.subspan(n);
    }
    co_return outcome::success();
  }
}  // namespace libp2p
