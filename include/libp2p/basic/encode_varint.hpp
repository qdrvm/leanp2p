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
  /**
   * Encodes a uint64_t as unsigned LEB128 (varint).
   * - Uses 7 bits per byte; MSB is the continuation flag.
   * - A uint64_t fits in at most 10 bytes.
   * - Provides a cheap view (BytesIn) of the encoded bytes.
   */
  class EncodeVarint {
   public:
    /**
     * Encode value into an internal fixed-size buffer.
     * The encoding is little-endian LEB128: least significant 7-bit groups
     * first.
     */
    EncodeVarint(uint64_t value) {
      // Defensive: u64 always fits into <= 10 bytes.
      do {
        uint8_t byte = value & 0x7f;
        value >>= 7;
        if (value != 0) {
          byte |= 0x80;  // set continuation bit
        }
        assert(size_ < buffer_.size());  // bounds check
        buffer_[size_] = byte;
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
