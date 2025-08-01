/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <coroutine>
#include <functional>
#include <memory>
#include <vector>

#include <libp2p/basic/closeable.hpp>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/multi/multiaddress.hpp>

namespace libp2p::transport {

  /**
   * Async generator for yielding incoming connections
   * @tparam T - type of generated values
   */
  template <typename T>
  struct AsyncGenerator {
    struct promise_type {
      std::optional<T> current_value;
      std::exception_ptr exception;

      auto get_return_object() {
        return AsyncGenerator{
            std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_always initial_suspend() {
        return {};
      }
      std::suspend_always final_suspend() noexcept {
        return {};
      }

      void unhandled_exception() {
        exception = std::current_exception();
      }

      void return_void() {}
      auto yield_value(T value) {
        current_value.emplace(std::move(value));
        return std::suspend_always{};
      }
    };

    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle_;

    explicit AsyncGenerator(handle_type h) : handle_(h) {}

    AsyncGenerator(AsyncGenerator &&other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    ~AsyncGenerator() {
      if (handle_) {
        handle_.destroy();
      }
    }

    AsyncGenerator &operator=(AsyncGenerator &&other) noexcept {
      if (this != &other) {
        if (handle_) {
          handle_.destroy();
        }
        handle_ = std::exchange(other.handle_, {});
      }
      return *this;
    }

    T operator*() const {
      return *handle_.promise().current_value;
    }

    auto operator co_await() {
      struct awaiter {
        handle_type handle;

        bool await_ready() {
          return false;
        }
        auto await_suspend(std::coroutine_handle<> h) {
          handle.resume();
          return h;
        }
        T await_resume() {
          if (handle.promise().exception) {
            std::rethrow_exception(handle.promise().exception);
          }
          return std::move(*handle.promise().current_value);
        }
      };
      return awaiter{handle_};
    }
  };

  /**
   * Listens to the connections from the specified addresses and reacts when
   * receiving ones
   */
  class TransportListener : public basic::Closeable {
   public:
    using NoArgsCallback = void();
    using ErrorCallback = void(const std::error_code &);
    using MultiaddrCallback = void(const multi::Multiaddress &);
    using ConnectionCallback =
        void(outcome::result<std::shared_ptr<connection::CapableConnection>>);
    using HandlerFunc = std::function<ConnectionCallback>;

    ~TransportListener() override = default;

    /**
     * Switch the listener into 'listen' mode; it will react to every new
     * connection
     * @param address to listen to
     */
    virtual outcome::result<void> listen(
        const multi::Multiaddress &address) = 0;

    /**
     * @brief Returns true if this transport can listen on given multiaddress,
     * false otherwise.
     * @param address multiaddress
     */
    virtual bool canListen(const multi::Multiaddress &address) const = 0;

    /**
     * Get addresses, which this listener listens to
     * @return collection of those addresses
     */
    virtual outcome::result<multi::Multiaddress> getListenMultiaddr() const = 0;

    /**
     * Get the io_context of this listener
     * @return reference to the io_context
     */
    // virtual boost::asio::io_context &getContext() const = 0;

    /**
     * Asynchronously accept new connections as an async generator
     * @return AsyncGenerator that yields new CapableConnection results
     */
    virtual AsyncGenerator<
        outcome::result<std::shared_ptr<connection::CapableConnection>>>
    asyncAccept() = 0;
  };
}  // namespace libp2p::transport
