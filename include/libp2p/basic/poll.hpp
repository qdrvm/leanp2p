/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <memory>
#include <optional>

#include <libp2p/outcome/outcome.hpp>

namespace libp2p {
  /**
   * Invoked once when a pending `PollFuture` becomes ready.
   */
  using PollWaker = std::function<void()>;

  /**
   * Single-consumer future for operations which complete out of band.
   *
   * `poll` returns `std::nullopt` while the operation is still in progress;
   * in that case `waker` is stored and invoked exactly once, when the
   * terminal result becomes available. Otherwise it returns the terminal
   * result, and every subsequent call returns that same result.
   *
   * An empty `waker` registers nothing, so `poll({})` only reads the current
   * state.
   *
   * Dropping the future does not cancel the operation; use `cancel()`.
   */
  class PollFuture {
   public:
    virtual ~PollFuture() = default;

    virtual std::optional<outcome::result<void>> poll(PollWaker waker) = 0;

    /**
     * Give up on the operation. Has no effect once it has completed.
     * Does not invoke the waker.
     */
    virtual void cancel() {}
  };

  /**
   * `PollFuture` which already holds its terminal result.
   */
  class ReadyPollFuture : public PollFuture {
   public:
    explicit ReadyPollFuture(outcome::result<void> result)
        : result_{std::move(result)} {}

    std::optional<outcome::result<void>> poll(PollWaker) override {
      return result_;
    }

   private:
    outcome::result<void> result_;
  };

  inline std::shared_ptr<PollFuture> makeReadyPollFuture(
      outcome::result<void> result) {
    return std::make_shared<ReadyPollFuture>(std::move(result));
  }
}  // namespace libp2p
