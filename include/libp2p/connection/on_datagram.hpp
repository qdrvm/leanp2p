/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <functional>
#include <qtils/byte_vec.hpp>

namespace libp2p::connection {
  struct CapableConnection;
}  // namespace libp2p::connection

namespace libp2p {
  using OnDatagram = std::function<void(
      std::shared_ptr<connection::CapableConnection>, qtils::ByteVec)>;

  struct OnDatagramConfig {
    OnDatagramConfig() = default;

    bool enable_datagram = false;
    OnDatagram on_datagram;
  };
  using OnDatagramConfigPtr = std::shared_ptr<OnDatagramConfig>;
}  // namespace libp2p
