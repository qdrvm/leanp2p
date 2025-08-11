/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <format>

template <typename T>
  requires requires(const T &t) { format_as(t); }
struct std::formatter<T>
    : std::formatter<
          std::remove_cvref_t<decltype(format_as(std::declval<const T &>()))>> {
  auto format(const T &t, std::format_context &ctx) const {
    auto &&v = format_as(t);
    return std::formatter<std::remove_cvref_t<decltype(v)>>::format(v, ctx);
  }
};
