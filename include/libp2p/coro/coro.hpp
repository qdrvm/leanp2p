/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/awaitable.hpp>
#include <qtils/outcome.hpp>

namespace libp2p {
  /**
   * Return type for coroutine.
   *
   * Does not resume when:
   * - called directly outside executor, returns coroutine.
   * - `coroSpawn` called when not running inside executor,
   *   resumes on next executor tick.
   *     int main() {
   *       boost::asio::io_context io;
   *       coroSpawn(io, []() -> Coro<void> { co_return; }); // suspended
   *       io.run_one(); // resumes
   *       // may complete before next statement
   *     }
   * Resumes when:
   * - `coroSpawn` when running inside specified executor.
   *     post(executor, [] {
   *       coroSpawn(executor, []() -> Coro<void> { co_return; }) // resumes
   *       // may complete before next statement
   *     })
   *     co_await coroSpawn([]() -> Coro<void> { co_return; }) // resumes
   *     // may complete before next statement
   * - `co_await`
   *     co_await foo() // resumes
   *     // may complete before next statement
   * After resuming may complete before specified statement ends.
   *
   * Use `CORO_YIELD` explicitly to suspend coroutine until next executor tick.
   */
  template <typename T>
  using Coro = boost::asio::awaitable<T>;

  /**
   * Return type for coroutine returning outcome.
   */
  template <typename T>
  using CoroOutcome = Coro<outcome::result<T>>;
}  // namespace libp2p
