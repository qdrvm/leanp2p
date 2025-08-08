/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <concepts>

#include <boost/asio/use_awaitable.hpp>

#include <libp2p/coro/coro.hpp>

namespace libp2p {
  template <typename T>
  using CoroHandler = std::conditional_t<
      std::is_void_v<T>,
      boost::asio::detail::awaitable_handler<typename Coro<T>::executor_type>,
      boost::asio::detail::awaitable_handler<typename Coro<T>::executor_type,
                                             T>>;

  /**
   * Create handler for coroutine.
   * Coroutine may complete earlier than handler returns.
   */
  template <typename T>
  Coro<T> coroHandler(std::invocable<CoroHandler<T> &&> auto &&f) {
    co_await [&](auto *frame) {
      f(CoroHandler<T>{frame->detach_thread()});
      return nullptr;
    };
    abort();
  }
}  // namespace libp2p
