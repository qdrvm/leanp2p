/**
 * Copyright Quadrivium LLC
 * All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 */

#include <libp2p/crypto/sha/keccak.hpp>

namespace libp2p {
  constexpr size_t HASH_LEN = 32;
  constexpr size_t BLOCK_SIZE = 200 - HASH_LEN * 2;

  void Keccak::update(uint8_t byte) {
    int j = blockOff >> 3;
    state[j % 5][j / 5] ^= static_cast<uint64_t>(byte) << ((blockOff & 7) << 3);
    blockOff++;
    if (blockOff == BLOCK_SIZE) {
      absorb();
      blockOff = 0;
    }
  }

  Keccak &Keccak::update(qtils::BytesIn input) {
    for (auto &x : input) {
      update(x);
    }
    return *this;
  }

  Keccak::Hash32 Keccak::finalize() {
    Hash32 hash;
    // Final block and padding
    {
      int i = blockOff >> 3;
      state[i % 5][i / 5] ^= UINT64_C(0x01) << ((blockOff & 7) << 3);
      blockOff = BLOCK_SIZE - 1;
      int j = blockOff >> 3;
      state[j % 5][j / 5] ^= UINT64_C(0x80) << ((blockOff & 7) << 3);
      absorb();
    }
    // Uint64 array to bytes in little endian
    for (int i = 0; i < HASH_LEN; i++) {
      int j = i >> 3;
      hash[i] = static_cast<uint8_t>(state[j % 5][j / 5] >> ((i & 7) << 3));
    }
    return hash;
  }

  Keccak::Hash32 Keccak::hash() const {
    auto copy = *this;
    return copy.finalize();
  }

  Keccak::Hash32 Keccak::hash(qtils::BytesIn input) {
    return Keccak{}.update(input).hash();
  }

  void Keccak::absorb() {
    auto rotl64 = [](uint64_t x, uint8_t i) {
      return (x << i) | (x >> (64 - i));
    };
    constexpr size_t NUM_ROUNDS = 24;
    constexpr uint8_t ROTATION[5][5] = {
        {0, 36, 3, 41, 18},
        {1, 44, 10, 45, 2},
        {62, 6, 43, 15, 61},
        {28, 55, 25, 21, 56},
        {27, 20, 39, 8, 14},
    };
    uint64_t (*a)[5] = state;
    uint8_t r = 1;  // LFSR
    for (int i = 0; i < NUM_ROUNDS; i++) {
      // Theta step
      uint64_t c[5] = {};
      for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
          c[x] ^= a[x][y];
        }
      }
      for (int x = 0; x < 5; x++) {
        uint64_t d = c[(x + 4) % 5] ^ rotl64(c[(x + 1) % 5], 1);
        for (int y = 0; y < 5; y++) {
          a[x][y] ^= d;
        }
      }

      // Rho and pi steps
      uint64_t b[5][5];
      for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
          b[y][(x * 2 + y * 3) % 5] = rotl64(a[x][y], ROTATION[x][y]);
        }
      }

      // Chi step
      for (int x = 0; x < 5; x++) {
        for (int y = 0; y < 5; y++) {
          a[x][y] = b[x][y] ^ (~b[(x + 1) % 5][y] & b[(x + 2) % 5][y]);
        }
      }

      // Iota step
      for (int j = 0; j < 7; j++) {
        a[0][0] ^= static_cast<uint64_t>(r & 1) << ((1 << j) - 1);
        r = static_cast<uint8_t>((r << 1) ^ ((r >> 7) * 0x171));
      }
    }
  }
}  // namespace libp2p
