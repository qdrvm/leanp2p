/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>

namespace libp2p {
  using PollWaker = std::function<void()>;

  class PollFuture {
   public:
    virtual ~PollFuture() = default;

    virtual bool poll(PollWaker waker) = 0;
  };
}  // namespace libp2p
