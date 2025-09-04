/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

namespace libp2p {
  auto saturating_sub(const auto &a, const auto &b) {
    return a >= b ? a - b : 0;
  }
}  // namespace libp2p
