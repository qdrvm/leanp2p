/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include <libp2p/basic/read.hpp>
#include <libp2p/basic/varint_prefix_reader.hpp>
#include <qtils/enum_error_code.hpp>

namespace libp2p {
  class EncodeVarint {
   public:
    EncodeVarint(uint64_t value) {
      do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) {
          byte |= 0x80;
        }
        buffer_.at(size_) = byte;
        ++size_;
      } while (value != 0);
    }

    operator BytesIn() const {
      return BytesIn{buffer_}.first(size_);
    }

   private:
    uint8_t size_ = 0;
    std::array<uint8_t, 10> buffer_{};
  };
}  // namespace libp2p
