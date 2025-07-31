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
    using WriteCallback = void(outcome::result<size_t> /*written bytes*/);
    using WriteCallbackFunc = std::function<WriteCallback>;

    virtual ~Writer() = default;

    virtual boost::asio::awaitable<std::error_code> writeSome(BytesIn in,
                                                              size_t bytes) = 0;
  };

}  // namespace libp2p::basic
