/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/io_context.hpp>
#include <boost/asio/steady_timer.hpp>
#include <libp2p/coro/spawn.hpp>

namespace libp2p {
  void timerLoop(boost::asio::io_context &io_context,
                 std::chrono::milliseconds delay,
                 auto f) {
    coroSpawn(io_context,
              [&io_context, delay, f{std::move(f)}]() -> Coro<void> {
                boost::asio::steady_timer timer{io_context};
                while (true) {
                  if constexpr (std::is_void_v<decltype(f())>) {
                    f();
                  } else if (not f()) {
                    break;
                  }
                  timer.expires_after(delay);
                  co_await timer.async_wait(boost::asio::use_awaitable);
                }
              });
  }
}  // namespace libp2p
