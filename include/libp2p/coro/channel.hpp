/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <boost/asio/experimental/channel.hpp>
#include <libp2p/coro/asio.hpp>
#include <libp2p/coro/coro.hpp>

namespace libp2p {
  template <typename T>
  class CoroOutcomeChannel {
   public:
    CoroOutcome<T> receive() {
      co_return (co_await channel_.async_receive(boost::asio::use_awaitable))
          .value();
    }

    void send(outcome::result<T> result) {
      channel_.try_send(boost::system::error_code{}, std::move(result));
    }

    boost::asio::experimental::channel<void(boost::system::error_code,
                                            std::optional<outcome::result<T>>)>
        channel_;
  };
}  // namespace libp2p
