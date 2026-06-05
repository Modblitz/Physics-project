#pragma once
#include "bitstream.hpp"
#include <cstdint>
#include <stdexcept>

namespace gol {

inline uint32_t zigzag_encode(int32_t v) {
    return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31);
}
inline int32_t zigzag_decode(uint32_t u) {
    return (int32_t)((u >> 1) ^ -(int32_t)(u & 1u));
}

inline void write_uint(BitWriter& bw, uint32_t u, int k) {
    uint32_t q = u >> k;
    for (uint32_t i = 0; i < q; ++i) bw.write_bit(1);
    bw.write_bit(0);
    if (k > 0) bw.write_bits(u & ((1u << k) - 1u), k);
}
inline uint32_t read_uint(BitReader& br, int k) {
    uint32_t q = 0;
    while (br.read_bit() == 1u) {
        if (++q > 100000000u) throw std::runtime_error("golomb: runaway code");
    }
    uint32_t rem = (k > 0) ? br.read_bits(k) : 0u;
    return (q << k) | rem;
}

inline void write_int(BitWriter& bw, int32_t v, int k) {
    write_uint(bw, zigzag_encode(v), k);
}
inline int32_t read_int(BitReader& br, int k) {
    return zigzag_decode(read_uint(br, k));
}

inline int choose_k_uint(const uint32_t* vals, int n) {
    if (n <= 0) return 0;
    uint64_t sum = 0;
    for (int i = 0; i < n; ++i) sum += vals[i];
    double mean = (double)sum / (double)n;
    int guess = 0;
    while ((1u << (guess + 1)) < (mean + 1.0) && guess < 28) ++guess;
    int best_k = 0;
    uint64_t best_cost = UINT64_MAX;
    int lo = guess - 2; if (lo < 0) lo = 0;
    int hi = guess + 2; if (hi > 30) hi = 30;
    for (int k = lo; k <= hi; ++k) {
        uint64_t cost = 0;
        for (int i = 0; i < n; ++i) cost += (vals[i] >> k) + 1u + (uint32_t)k;
        if (cost < best_cost) { best_cost = cost; best_k = k; }
    }
    return best_k;
}

}
