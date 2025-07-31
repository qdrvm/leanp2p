/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/log/logger.hpp>
#include <libp2p/protocol_muxer/multiselect/multiselect_instance.hpp>

namespace libp2p::protocol_muxer::multiselect {

  namespace {
#ifndef WITHOUT_TRACE_LOG_MESSAGE
    const log::Logger &log() {
      static log::Logger logger = log::createLogger("Multiselect");
      return logger;
    }
#endif

    constexpr size_t kMaxCacheSize = 8;
  }  // namespace

  boost::asio::awaitable<outcome::result<peer::ProtocolName>>
  Multiselect::selectOneOf(std::span<const peer::ProtocolName> protocols,
                           std::shared_ptr<basic::ReadWriter> connection,
                           bool is_initiator,
                           bool negotiate_multistream) {
    // Create instance and delegate to its coroutine implementation
    auto instance = getInstance();

    // Get the result from the coroutine implementation
    auto result = co_await instance->selectOneOf(
        protocols, std::move(connection), is_initiator, negotiate_multistream);

    // Return the instance to the cache regardless of result
    active_instances_.erase(instance);
    if (cache_.size() < kMaxCacheSize) {
      cache_.emplace_back(std::move(instance));
    }

    // Return the result
    co_return result;
  }

  Multiselect::Instance Multiselect::getInstance() {
    Instance instance;
    if (cache_.empty()) {
      instance = std::make_shared<MultiselectInstance>(*this);
    } else {
      SL_TRACE(log(),
               "cache: {}->{}, active {}->{}",
               cache_.size(),
               cache_.size() - 1,
               active_instances_.size(),
               active_instances_.size() + 1);
      instance = std::move(cache_.back());
      cache_.pop_back();
    }
    active_instances_.insert(instance);
    return instance;
  }

}  // namespace libp2p::protocol_muxer::multiselect
