// https://github.com/nayuki/Bitcoin-Cryptography-Library/blob/master/cpp/Keccak256.hpp
// Not using https://vcpkg.io/en/package/keccak-tiny from vcpkg because it only
// exports sha3, not keccak

#pragma once

#include <qtils/byte_arr.hpp>

namespace libp2p {
  class Keccak {
   public:
    using Hash32 = qtils::ByteArr<32>;

    void update(uint8_t byte);
    Keccak &update(qtils::BytesIn input);
    Hash32 finalize();
    Hash32 hash() const;
    static Hash32 hash(qtils::BytesIn input);

   private:
    void absorb();

    uint64_t state[5][5] = {};
    size_t blockOff = 0;
  };
}  // namespace libp2p
