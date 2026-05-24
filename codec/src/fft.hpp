#pragma once
#include <complex>
#include <vector>

using Complex = std::complex<double>;

void fft_inplace(std::vector<Complex>& a, bool invert);
