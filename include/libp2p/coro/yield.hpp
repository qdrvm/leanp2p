/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/post.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <libp2p/coro/coro.hpp>

namespace libp2p {
  /**
   * Yields execution to allow other coroutines to run.
   *
   * This function posts a continuation to the current executor, effectively
   * yielding control and allowing the event loop to process other pending work.
   * Thread switch operation always completes, so it can't leak `shared_ptr`.
   *
   * @return A coroutine that completes after yielding execution
   */
  inline Coro<void> coroYield() {
    co_await boost::asio::post(co_await boost::asio::this_coro::executor,
                               boost::asio::use_awaitable);
  }
}  // namespace libp2p
