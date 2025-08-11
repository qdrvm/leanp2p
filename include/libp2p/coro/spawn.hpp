/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/co_spawn.hpp>
#include <libp2p/coro/coro.hpp>

namespace libp2p {
  template <typename T>
  concept CoroSpawnExecutor =
      boost::asio::is_executor<T>::value
      || boost::asio::execution::is_executor<T>::value
      || std::is_convertible_v<T, boost::asio::execution_context &>;

  void coroSpawn(CoroSpawnExecutor auto &&executor, Coro<void> &&coro) {
    boost::asio::co_spawn(std::forward<decltype(executor)>(executor),
                          std::move(coro),
                          [](std::exception_ptr e) {
                            if (e != nullptr) {
                              std::rethrow_exception(e);
                            }
                          });
  }

  template <typename T>
  void coroSpawn(CoroSpawnExecutor auto &&executor, Coro<T> &&coro) {
    coroSpawn(std::forward<decltype(executor)>(executor),
              [coro{std::move(coro)}]() mutable -> Coro<void> {
                std::ignore = co_await std::move(coro);
              });
  }

  /**
   * Start coroutine on specified executor.
   * Spawning on same executor would execute coroutine immediately,
   * so coroutine may complete before `coroSpawn` returns.
   * Prevents dangling lambda capture in `coroSpawn([capture] { ... })`.
   * `co_spawn([capture] { ... })` doesn't work
   * because lambda is destroyed after returning coroutine object.
   * `co_spawn([](args){ ... }(capture))`
   * works because arguments are stored in coroutine state.
   */
  void coroSpawn(CoroSpawnExecutor auto &&executor, auto &&f) {
    coroSpawn(std::forward<decltype(executor)>(executor),
              [](std::remove_cvref_t<decltype(f)> f) -> Coro<void> {
                if constexpr (std::is_void_v<decltype(f().await_resume())>) {
                  co_await f();
                } else {
                  std::ignore = co_await f();
                }
              }(std::forward<decltype(f)>(f)));
  }

  /**
   * `coroSpawn` with current coroutine executor.
   */
  Coro<void> coroSpawn(auto f) {
    coroSpawn(co_await boost::asio::this_coro::executor, std::move(f));
    co_return;
  }
}  // namespace libp2p
