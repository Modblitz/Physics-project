#include "mdct.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Mdct::Mdct(int N) : N_(N), cos_((size_t)N * 2 * N) {
    const int M = 2 * N;
    const double n0 = 0.5 + (double)N / 2.0;
    for (int k = 0; k < N; ++k) {
        const double kk = (double)k + 0.5;
        double* row = &cos_[(size_t)k * M];
        for (int n = 0; n < M; ++n)
            row[n] = std::cos(M_PI / (double)N * ((double)n + n0) * kk);
    }
}

void Mdct::forward(const double* in, double* out) const {
    const int M = 2 * N_;
    const double scale = std::sqrt(2.0 / (double)N_);
    for (int k = 0; k < N_; ++k) {
        const double* row = &cos_[(size_t)k * M];
        double s = 0.0;
        for (int n = 0; n < M; ++n) s += in[n] * row[n];
        out[k] = s * scale;
    }
}

void Mdct::inverse(const double* in, double* out) const {
    const int M = 2 * N_;
    const double scale = std::sqrt(2.0 / (double)N_);
    for (int n = 0; n < M; ++n) out[n] = 0.0;
    for (int k = 0; k < N_; ++k) {
        const double c = in[k];
        if (c == 0.0) continue;
        const double* row = &cos_[(size_t)k * M];
        for (int n = 0; n < M; ++n) out[n] += c * row[n];
    }
    for (int n = 0; n < M; ++n) out[n] *= scale;
}
