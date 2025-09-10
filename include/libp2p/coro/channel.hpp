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
      auto result = co_await channel_.async_receive(boost::asio::use_awaitable);
      co_return result.value();
    }

    void send(outcome::result<T> result) {
      channel_.async_send(boost::system::error_code{},
                          std::move(result),
                          [](boost::system::error_code ec) {
                            if (ec) {
                              throw std::system_error{ec};
                            }
                          });
    }

    boost::asio::experimental::channel<void(boost::system::error_code,
                                            std::optional<outcome::result<T>>)>
        channel_;
  };
}  // namespace libp2p
