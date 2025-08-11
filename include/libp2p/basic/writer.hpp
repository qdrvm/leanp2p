/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/types.hpp>
#include <libp2p/coro/coro.hpp>

namespace libp2p::basic {

  struct Writer {
    virtual ~Writer() = default;

    virtual CoroOutcome<size_t> writeSome(BytesIn in) = 0;
  };

}  // namespace libp2p::basic
