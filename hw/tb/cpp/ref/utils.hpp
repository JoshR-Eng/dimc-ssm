#pragma once
#include <cstdint>

// Interpret the low `bits` of v as a two's-complement number
inline int64_t sext(uint64_t v, unsigned bits) {
    return int64_t(v << (64 - bits)) >> (64 - bits);
}

// Mask with the low `bits` set (bits in 1..64)
constexpr uint64_t low_mask(unsigned bits) {
    return bits >= 64 ? ~uint64_t(0) : (uint64_t(1) << bits) - 1;
}
