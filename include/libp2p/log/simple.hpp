/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/log/logger.hpp>
#include <print>
#include <soralog/impl/configurator_from_yaml.hpp>

namespace libp2p {
  void simpleLoggingSystem() {
    std::string yaml = R"(
    sinks:
      - name: console
        type: console
        color: true
        capacity: 4
        latency: 0
    groups:
      - name: main
        sink: console
        level: info
        is_fallback: true
        children:
          - name: libp2p
    )";
    auto logsys = std::make_shared<soralog::LoggingSystem>(
        std::make_shared<soralog::ConfiguratorFromYAML>(yaml));
    auto r = logsys->configure();
    if (not r.message.empty()) {
      std::println(stderr, "soralog error: {}", r.message);
    }
    if (r.has_error) {
      exit(EXIT_FAILURE);
    }
    libp2p::log::setLoggingSystem(logsys);
  }
}  // namespace libp2p
