#pragma once
#include "bitstream.hpp"
#include <cstdint>

namespace rice {

constexpr int kKBits = 5;
constexpr int kMaxK  = 17;

uint32_t zigzag_encode(int32_t v);
int32_t  zigzag_decode(uint32_t u);

int choose_k(const int32_t* residuals, int n);

void write_residual(BitWriter& bw, int32_t r, int k);
int32_t read_residual(BitReader& br, int k);

}
