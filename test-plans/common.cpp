/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include "common.hpp"

#include <ifaddrs.h>
#include <boost/asio/ip/address_v4.hpp>
#include <thread>

timeval asTimeval(std::chrono::microseconds us) {
  timeval tv;
  auto s = std::chrono::duration_cast<std::chrono::seconds>(us);
  tv.tv_sec = s.count();
  tv.tv_usec = (us - s).count();
  return tv;
}

std::optional<std::string> getenv_opt(const char *name) {
  if (const char *v = std::getenv(name)) {
    return std::string{v};
  }
  return std::nullopt;
}

std::optional<bool> parseBool(std::string_view str) {
  if (str == "true") {
    return true;
  }
  if (str == "false") {
    return false;
  }
  return std::nullopt;
}

std::optional<std::string> get_first_network_ip(libp2p::log::Logger log) {
  std::optional<std::string> result;
  struct ifaddrs *ifaddr;

  if (getifaddrs(&ifaddr) == -1) {
    log->error("getifaddrs failed");
    return std::nullopt;
  }

  for (auto *ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
    if (ifa->ifa_addr == nullptr) {
      continue;
    }

    if ((ifa->ifa_flags & IFF_LOOPBACK) != 0
        or (ifa->ifa_flags & IFF_UP) == 0) {
      continue;
    }

    if (ifa->ifa_addr->sa_family == AF_INET) {
      struct sockaddr_in *addr = (struct sockaddr_in *)ifa->ifa_addr;
      result = boost::asio::ip::make_address_v4(ntohl(addr->sin_addr.s_addr))
                   .to_string();
      log->info(
          "Found network interface {} with IP {}", ifa->ifa_name, *result);
      break;
    }
  }

  freeifaddrs(ifaddr);
  return result;
}

TestTimeout::TestTimeout(Timeout timeout)
    : start_{Clock::now()}, timeout_{timeout} {}

TestTimeout::Remaining TestTimeout::remaining() const {
  auto now = Clock::now();
  auto deadline = start_ + timeout_;
  if (now > deadline) {
    return Remaining::zero();
  }
  return deadline - now;
}

Redis::Redis(libp2p::log::Logger log, ContextPtr ctx)
    : log_{std::move(log)}, ctx_{std::move(ctx)} {}

outcome::result<Redis::Address> Redis::parseAddress(std::string_view str) {
  auto colon_pos = str.find(":");
  if (colon_pos == str.npos) {
    return Address{str, 6379};
  }
  std::string host{str.substr(0, colon_pos)};
  BOOST_OUTCOME_TRY(auto port, parseInt<uint16_t>(str.substr(colon_pos + 1)));
  return Address{host, port};
}

outcome::result<std::shared_ptr<Redis>> Redis::connect(
    std::string_view address_str, std::chrono::microseconds timeout) {
  auto log = libp2p::log::createLogger("redis");
  BOOST_OUTCOME_TRY(auto address, parseAddress(address_str));
  ContextPtr ctx{redisConnectWithTimeout(
      address.first.c_str(), address.second, asTimeval(timeout))};
  if (ctx == nullptr) {
    SL_ERROR(log, "Failed to connect to redis");
    return Error::CONNECT_ERROR;
  }
  if (ctx->err != 0) {
    SL_ERROR(log, "Failed to connect to redis: {}", ctx->errstr);
    return Error::CONNECT_ERROR;
  }
  return std::make_shared<Redis>(log, std::move(ctx));
}

outcome::result<void> Redis::ping() {
  BOOST_OUTCOME_TRY(auto reply, tryReply(redisCommand(ctx_.get(), "PING")));
  if (replyStr(*reply) != "PONG") {
    return Error::PING_REPLY_ERROR;
  }
  return outcome::success();
}

outcome::result<std::string> Redis::blpop(const std::string &key,
                                          TimeoutDouble timeout) {
  BOOST_OUTCOME_TRY(
      auto reply,
      tryReply(redisCommand(
          ctx_.get(),
          "BLPOP %s %d",
          key.c_str(),
          static_cast<int>(
              std::chrono::duration_cast<std::chrono::seconds>(timeout)
                  .count()))));
  auto value = replyArray(*reply, 1);
  if (not value.has_value()) {
    return Error::BLPOP_REPLY_ERROR;
  }
  return std::string{replyStr(**value)};
}

outcome::result<void> Redis::rpush(const std::string &key,
                                   const std::string &value) {
  BOOST_OUTCOME_TRY(tryReply(
      redisCommand(ctx_.get(), "RPUSH %s %s", key.c_str(), value.c_str())));
  return outcome::success();
}

std::string_view Redis::replyStr(const redisReply &reply) {
  return {reply.str, reply.len};
}

std::optional<const redisReply *> Redis::replyArray(const redisReply &reply,
                                                    size_t index) {
  if (index >= reply.elements) {
    return std::nullopt;
  }
  return reply.element[index];
}

outcome::result<Redis::ReplyPtr> Redis::tryReply(void *reply_ptr) {
  if (reply_ptr == nullptr) {
    return Error::NO_REPLY;
  }
  ReplyPtr reply{static_cast<redisReply *>(reply_ptr)};
  if (reply->type == REDIS_REPLY_ERROR) {
    SL_ERROR(log_, "reply error: {}", replyStr(*reply));
    return Error::REPLY_ERROR;
  }
  return std::move(reply);
}

std::shared_ptr<Redis> testRedisConnect(libp2p::log::Logger log,
                                        std::string_view address,
                                        const TestTimeout &timeout) {
  auto redis_res = Redis::connect(address, timeout.remainingUs());
  if (not redis_res.has_value()) {
    SL_FATAL(log, "redis connect error: {}", redis_res.error());
  }
  auto &redis = redis_res.value();
  while (true) {
    auto ping_res = redis->ping();
    if (ping_res.has_value()) {
      break;
    }
    SL_WARN(log, "redis ping error: {}", ping_res.error());
    if (timeout.remaining().count() == 0) {
      SL_FATAL(log, "redis ping timeout");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds{100});
  }
  return redis;
}
