/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>

#include <libp2p/connection/secure_connection.hpp>

namespace libp2p::connection {

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

    template <typename U = T>
    boost::asio::awaitable<std::optional<U>> next() {
      if (!handle_ || handle_.done()) {
        co_return std::nullopt;
      }
      handle_.resume();
      if (handle_.done()) {
        co_return std::nullopt;
      }
      auto &promise = handle_.promise();
      if (promise.exception) {
        std::rethrow_exception(promise.exception);
      }
      std::optional<U> value = std::move(promise.current_value);
      promise.current_value.reset();
      co_return value;
    }

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

  struct Stream;

  /**
   * Connection that provides basic libp2p requirements to the connection: it is
   * both secured and muxed (streams can be created over that connection)
   */
  struct CapableConnection : public SecureConnection {
    using StreamHandler = void(outcome::result<std::shared_ptr<Stream>>);
    using StreamHandlerFunc = std::function<StreamHandler>;

    using NewStreamHandlerFunc = std::function<void(std::shared_ptr<Stream>)>;

    using ConnectionClosedCallback = std::function<void(
        const peer::PeerId &,
        const std::shared_ptr<connection::CapableConnection> &)>;

    ~CapableConnection() override = default;

    /**
     * Start to process incoming messages for this connection
     * @note non-blocking
     *
     * @note make sure onStream(..) was called, so that new streams are accepted
     * by this connection - call to start() will fail otherwise
     */
    virtual void start() = 0;

    /**
     * Stop processing incoming messages for this connection without closing the
     * connection itself
     * @note calling 'start' after 'close' is UB
     */
    virtual void stop() = 0;

    /**
     * @brief Opens new stream in a synchronous (optimistic) manner
     * @return Stream or error
     */
    virtual outcome::result<std::shared_ptr<Stream>> newStream() = 0;

    /**
     * @brief Opens new stream using this connection
     * @param cb - callback to be called, when a new stream is established or
     * error appears
     */
    virtual void newStream(StreamHandlerFunc cb) = 0;

    /**
     * @brief Opens new stream in a coroutine manner
     * @return Awaitable result of a new Stream or error
     */
    virtual boost::asio::awaitable<outcome::result<std::shared_ptr<Stream>>>
    newStreamCoroutine() = 0;

    virtual boost::asio::awaitable<
        outcome::result<std::shared_ptr<connection::Stream>>>
    acceptStream() = 0;
  };

}  // namespace libp2p::connection
