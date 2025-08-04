/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/awaitable.hpp>
#include <functional>

#include <libp2p/common/types.hpp>
#include <libp2p/outcome/outcome.hpp>

namespace libp2p::basic {

  struct Writer {
    virtual ~Writer() = default;

    virtual boost::asio::awaitable<outcome::result<size_t>> writeSome(
        BytesIn in, size_t bytes) = 0;
  };

}  // namespace libp2p::basic
