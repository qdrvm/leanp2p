/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <hiredis/hiredis.h>
#include <charconv>
#include <chrono>
#include <cstdlib>
#include <libp2p/log/logger.hpp>
#include <optional>
#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>
#include <string>

#define TRY_OR_SL_FATAL(r, log, format, ...)                        \
  ({                                                                \
    auto __r = (r);                                                 \
    if (not __r.has_value()) {                                      \
      SL_FATAL(log, format, __VA_ARGS__ __VA_OPT__(, ) __r.error()) \
    }                                                               \
    __r.value();                                                    \
  })

struct redisContext;

timeval asTimeval(std::chrono::microseconds us);

template <typename T>
outcome::result<T> parseInt(std::string_view str) {
  T num;
  auto r = std::from_chars(str.data(), str.data() + str.size(), num);
  if (r.ec != std::errc{}) {
    return make_error_code(r.ec);
  }
  if (r.ptr - str.data() != str.size()) {
    return std::errc::invalid_argument;
  }
  return num;
}

std::optional<std::string> getenv_opt(const char *name);

std::optional<bool> parseBool(std::string_view str);

std::optional<std::string> get_first_network_ip(libp2p::log::Logger log);

struct TestTimeout {
  using Clock = std::chrono::steady_clock;
  using Timeout = std::chrono::seconds;
  using Remaining = std::chrono::nanoseconds;

  TestTimeout(Timeout timeout);

  Remaining remaining() const;

  std::chrono::microseconds remainingUs() const {
    return std::chrono::duration_cast<std::chrono::microseconds>(remaining());
  }
  std::chrono::milliseconds remainingMs() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(remaining());
  }

  Clock::time_point start_;
  Timeout timeout_;
};

struct redisContextDeleter {
  static void operator()(redisContext *ptr) {
    if (ptr != nullptr) {
      redisFree(ptr);
    }
  }
};

struct redisReplyDeleter {
  static void operator()(redisReply *ptr) {
    if (ptr != nullptr) {
      freeReplyObject(ptr);
    }
  }
};

class Redis {
 public:
  using ContextPtr = std::unique_ptr<redisContext, redisContextDeleter>;
  using ReplyPtr = std::unique_ptr<redisReply, redisReplyDeleter>;

  using TimeoutDouble = std::chrono::duration<double>;

  enum class Error {
    CONNECT_ERROR,
    PARSE_PORT_ERROR,
    NO_REPLY,
    REPLY_ERROR,
    PING_REPLY_ERROR,
    BLPOP_REPLY_ERROR,
  };
  Q_ENUM_ERROR_CODE_FRIEND(Error) {
    using E = decltype(e);
    switch (e) {
      case E::CONNECT_ERROR:
        return "CONNECT_ERROR";
      case E::PARSE_PORT_ERROR:
        return "PARSE_PORT_ERROR";
      case E::NO_REPLY:
        return "NO_REPLY";
      case E::REPLY_ERROR:
        return "REPLY_ERROR";
      case E::PING_REPLY_ERROR:
        return "PING_REPLY_ERROR";
      case E::BLPOP_REPLY_ERROR:
        return "BLPOP_REPLY_ERROR";
    }
    abort();
  }

  Redis(libp2p::log::Logger log, ContextPtr ctx);

  using Address = std::pair<std::string, uint16_t>;
  static outcome::result<Address> parseAddress(std::string_view str);

  static outcome::result<std::shared_ptr<Redis>> connect(
      std::string_view address_str, std::chrono::microseconds timeout);

  outcome::result<void> ping();
  outcome::result<std::string> blpop(const std::string &key,
                                     TimeoutDouble timeout);
  outcome::result<void> rpush(const std::string &key, const std::string &value);

  static std::string_view replyStr(const redisReply &reply);
  static std::optional<const redisReply *> replyArray(const redisReply &reply,
                                                      size_t index);

 private:
  outcome::result<ReplyPtr> tryReply(void *reply_ptr);

  libp2p::log::Logger log_;
  ContextPtr ctx_;
};

std::shared_ptr<Redis> testRedisConnect(libp2p::log::Logger log,
                                        std::string_view address,
                                        const TestTimeout &timeout);
