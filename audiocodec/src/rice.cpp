#include "rice.hpp"

#include <cstdint>
#include <stdexcept>

namespace rice {

uint32_t zigzag_encode(int32_t v) {
    return ((uint32_t)v << 1) ^ (uint32_t)(v >> 31);
}
int32_t zigzag_decode(uint32_t u) {
    return (int32_t)((u >> 1) ^ -(int32_t)(u & 1u));
}

namespace {

uint64_t rice_cost(uint32_t u, int k) {
    return (uint64_t)((u >> k) + 1u + (uint32_t)k);
}

}

int choose_k(const int32_t* residuals, int n) {
    uint64_t sum_abs = 0;
    for (int i = 0; i < n; ++i) {
        sum_abs += (uint64_t)zigzag_encode(residuals[i]);
    }
    double mean = (double)sum_abs / (double)n;
    int k_guess = 0;
    if (mean > 1.0) {
        double m = mean;
        while (m >= 2.0) { m *= 0.5; ++k_guess; }
    }

    int best_k = 0;
    uint64_t best_cost = UINT64_MAX;
    int lo = k_guess - 2; if (lo < 0) lo = 0;
    int hi = k_guess + 2; if (hi > kMaxK) hi = kMaxK;
    for (int k = lo; k <= hi; ++k) {
        uint64_t cost = 0;
        for (int i = 0; i < n; ++i) {
            cost += rice_cost(zigzag_encode(residuals[i]), k);
        }
        if (cost < best_cost) {
            best_cost = cost;
            best_k = k;
        }
    }
    return best_k;
}

void write_residual(BitWriter& bw, int32_t r, int k) {
    uint32_t u = zigzag_encode(r);
    uint32_t q = u >> k;
    uint32_t rem = (k > 0) ? (u & ((1u << k) - 1u)) : 0u;
    for (uint32_t i = 0; i < q; ++i) bw.write_bit(1);
    bw.write_bit(0);
    if (k > 0) bw.write_bits(rem, k);
}

int32_t read_residual(BitReader& br, int k) {
    uint32_t q = 0;
    while (br.read_bit() == 1u) {
        ++q;
        if (q > 1000000u) {
            throw std::runtime_error("rice: runaway code :(");
        }
    }
    uint32_t rem = (k > 0) ? br.read_bits(k) : 0u;
    uint32_t u = (q << k) | rem;
    return zigzag_decode(u);
}

}
