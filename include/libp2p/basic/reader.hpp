/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <vector>

#include <boost/asio/awaitable.hpp>
#include <libp2p/common/types.hpp>
#include <libp2p/outcome/outcome.hpp>

namespace libp2p::basic {

  struct Reader {
    virtual ~Reader() = default;

    /**
     * @brief Defers reporting result or error to callback to avoid reentrancy
     * (i.e. callback will not be called before initiator function returns)
     * @param res read result
     * @param cb callback
     */

    virtual boost::asio::awaitable<outcome::result<size_t>> read(
        BytesOut out, size_t bytes) = 0;

    virtual boost::asio::awaitable<outcome::result<size_t>> readSome(
        BytesOut out, size_t bytes) = 0;
  };

}  // namespace libp2p::basic
