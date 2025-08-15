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
  enum class ProtobufError {
    DECODE_ERROR,
  };
  Q_ENUM_ERROR_CODE(ProtobufError) {
    using E = decltype(e);
    switch (e) {
      case E::DECODE_ERROR:
        return "Error decoding protobuf";
    }
    abort();
  }

  template <typename T>
  Bytes protobufEncode(const T &t) {
    Bytes encoded;
    encoded.resize(t.ByteSizeLong());
    if (not t.SerializeToArray(encoded.data(), encoded.size())) {
      throw std::logic_error{"protobufEncode"};
    }
    return encoded;
  }

  template <typename T>
  outcome::result<T> protobufDecode(BytesIn encoded) {
    T t;
    if (not t.ParseFromArray(encoded.data(), encoded.size())) {
      return ProtobufError::DECODE_ERROR;
    }
    return t;
  }
}  // namespace libp2p
