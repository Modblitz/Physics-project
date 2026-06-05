#include "filterbank.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {
double sine_win(int i, int len) { return std::sin(M_PI / len * (i + 0.5)); }
}

Filterbank::Filterbank(int N, int NS, int nShort)
    : N_(N), NS_(NS), nShort_(nShort),
      base_((N - NS) / 2),
      mdctLong_(N), mdctShort_(NS),
      wLong_(2 * N), wShort_(2 * NS), wStart_(2 * N), wStop_(2 * N) {
    const int M = 2 * N, MS = 2 * NS;

    for (int i = 0; i < M; ++i) wLong_[i] = sine_win(i, M);
    for (int i = 0; i < MS; ++i) wShort_[i] = sine_win(i, MS);

    int sfall = base_ + N;
    for (int i = 0; i < M; ++i) {
        if (i < N) wStart_[i] = wLong_[i];
        else if (i < sfall) wStart_[i] = 1.0;
        else if (i < sfall + NS) wStart_[i] = wShort_[NS + (i - sfall)];
        else wStart_[i] = 0.0;
    }
    int srise = base_;
    for (int i = 0; i < M; ++i) {
        if (i < srise) wStop_[i] = 0.0;
        else if (i < srise + NS) wStop_[i] = wShort_[i - srise];
        else if (i < N) wStop_[i] = 1.0;
        else wStop_[i] = wLong_[i];
    }
}

const std::vector<double>& Filterbank::longWin(WinType t) const {
    switch (t) {
        case WIN_START: return wStart_;
        case WIN_STOP: return wStop_;
        default: return wLong_;
    }
}

void Filterbank::forward(WinType t, const double* in, double* coef) const {
    if (t == WIN_SHORT) {
        std::vector<double> seg(2 * NS_);
        for (int b = 0; b < nShort_; ++b) {
            const double* src = in + base_ + b * NS_;
            for (int j = 0; j < 2 * NS_; ++j) seg[j] = src[j] * wShort_[j];
            mdctShort_.forward(seg.data(), coef + (size_t)b * NS_);
        }
    } else {
        const std::vector<double>& w = longWin(t);
        std::vector<double> tmp(2 * N_);
        for (int i = 0; i < 2 * N_; ++i) tmp[i] = in[i] * w[i];
        mdctLong_.forward(tmp.data(), coef);
    }
}

void Filterbank::inverse(WinType t, const double* coef, double* out) const {
    const int M = 2 * N_;
    if (t == WIN_SHORT) {
        for (int i = 0; i < M; ++i) out[i] = 0.0;
        std::vector<double> seg(2 * NS_);
        for (int b = 0; b < nShort_; ++b) {
            mdctShort_.inverse(coef + (size_t)b * NS_, seg.data());
            double* dst = out + base_ + b * NS_;
            for (int j = 0; j < 2 * NS_; ++j) dst[j] += seg[j] * wShort_[j];
        }
    } else {
        const std::vector<double>& w = longWin(t);
        mdctLong_.inverse(coef, out);
        for (int i = 0; i < M; ++i) out[i] *= w[i];
    }
}
