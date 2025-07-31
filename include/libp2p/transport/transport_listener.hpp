/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

// #include <boost/signals2/connection.hpp>
#include <functional>
#include <memory>
#include <vector>
#include <coroutine>

#include <libp2p/basic/closeable.hpp>
#include <libp2p/connection/capable_connection.hpp>
#include <libp2p/multi/multiaddress.hpp>

namespace libp2p::transport {

  /**
   * Async generator for yielding incoming connections
   */
  template<typename T>
  class AsyncGenerator {
  public:
    struct promise_type {
      T current_value;
      std::exception_ptr exception_;

      AsyncGenerator get_return_object() {
        return AsyncGenerator{std::coroutine_handle<promise_type>::from_promise(*this)};
      }

      std::suspend_always initial_suspend() { return {}; }
      std::suspend_always final_suspend() noexcept { return {}; }

      std::suspend_always yield_value(T value) {
        current_value = std::move(value);
        return {};
      }

      void return_void() {}

      void unhandled_exception() {
        exception_ = std::current_exception();
      }

      boost::asio::awaitable<bool> await_transform(boost::asio::awaitable<T>&& awaitable) {
        try {
          current_value = co_await std::move(awaitable);
          co_return true;
        } catch (...) {
          exception_ = std::current_exception();
          co_return false;
        }
      }
    };

    using handle_type = std::coroutine_handle<promise_type>;

    explicit AsyncGenerator(handle_type h) : handle_(h) {}

    AsyncGenerator(AsyncGenerator&& other) noexcept : handle_(std::exchange(other.handle_, {})) {}

    ~AsyncGenerator() {
      if (handle_) {
        handle_.destroy();
      }
    }

    AsyncGenerator& operator=(AsyncGenerator&& other) noexcept {
      if (this != &other) {
        if (handle_) {
          handle_.destroy();
        }
        handle_ = std::exchange(other.handle_, {});
      }
      return *this;
    }

    AsyncGenerator(const AsyncGenerator&) = delete;
    AsyncGenerator& operator=(const AsyncGenerator&) = delete;

    boost::asio::awaitable<bool> next() {
      if (!handle_ || handle_.done()) {
        co_return false;
      }

      handle_.resume();

      if (handle_.promise().exception_) {
        std::rethrow_exception(handle_.promise().exception_);
      }

      co_return !handle_.done();
    }

    T current() const {
      return handle_.promise().current_value;
    }

    bool done() const {
      return !handle_ || handle_.done();
    }

  private:
    handle_type handle_;
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
    virtual boost::asio::io_context &getContext() const = 0;

    /**
     * Asynchronously accept new connections as an async generator
     * @return AsyncGenerator that yields new CapableConnection results
     */
    virtual AsyncGenerator<outcome::result<std::shared_ptr<connection::CapableConnection>>>
    asyncAccept() = 0;
  };
}  // namespace libp2p::transport
