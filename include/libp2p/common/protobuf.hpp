/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/common/types.hpp>
#include <qtils/enum_error_code.hpp>
#include <qtils/outcome.hpp>

namespace libp2p {

  /**
   * Errors related to protobuf encoding/decoding.
   */
  enum class ProtobufError {
    DECODE_ERROR,
  };

  /**
   * Human-readable strings for `ProtobufError`.
   */
  Q_ENUM_ERROR_CODE(ProtobufError) {
    using E = decltype(e);
    switch (e) {
      case E::DECODE_ERROR:
        return "Error decoding protobuf";
    }
    abort();
  }

  /**
   * Encode a protobuf message-like `T` to `Bytes`.
   * Requirements on `T`: has `ByteSizeLong()` and `SerializeToArray(void*,
   * int)`. Throws `std::logic_error` if serialization fails or size exceeds
   * `INT_MAX`.
   */
  template <typename T>
  [[nodiscard]] Bytes protobufEncode(const T &t) {
    Bytes encoded;
    encoded.resize(t.ByteSizeLong());
    if (not t.SerializeToArray(encoded.data(), encoded.size())) {
      throw std::logic_error{"protobufEncode: SerializeToArray failed"};
    }
    return encoded;
  }

  /**
   * Decode a protobuf message-like `T` from `BytesIn`.
   * Requirements on `T`: default-constructible, has `ParseFromArray(const
   * void*, int)`. Returns `DECODE_ERROR` if parsing fails.
   */
  template <typename T>
  [[nodiscard]] inline outcome::result<T> protobufDecode(BytesIn encoded) {
    T t;
    if (not t.ParseFromArray(encoded.data(), encoded.size())) {
      return ProtobufError::DECODE_ERROR;
    }
    return t;
  }
}  // namespace libp2p
