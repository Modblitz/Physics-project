#include "psycho.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstdio>

namespace {

double hz_to_bark(double f) {
    return 13.0 * std::atan(0.00076 * f)
         + 3.5 * std::atan((f / 7500.0) * (f / 7500.0));
}

double ath_db(double f) {
    double k = f / 1000.0;
    if (k < 0.02) k = 0.02;
    return 3.64 * std::pow(k, -0.8)
         - 6.5 * std::exp(-0.6 * (k - 3.3) * (k - 3.3))
         + 1e-3 * std::pow(k, 4.0);
}

double spread_db(double dz) {
    double t = dz + 0.474;
    return 15.81 + 7.5 * t - 17.5 * std::sqrt(1.0 + t * t);
}

constexpr double kAthRefDb = -6.0;

}

Psycho::Psycho(int N, double sampleRate) : N_(N) {
    binBand_.resize(N);
    const double binHz = sampleRate / (2.0 * N);

    constexpr double kBandBark = 0.5;
    std::vector<double> binBark(N);
    bandStart_.clear();
    bandStart_.push_back(0);
    binBark[0] = hz_to_bark(0.5 * binHz);
    binBand_[0] = 0;
    int lastSlot = (int)(binBark[0] / kBandBark);
    for (int k = 1; k < N; ++k) {
        binBark[k] = hz_to_bark(((double)k + 0.5) * binHz);
        int slot = (int)(binBark[k] / kBandBark);
        if (slot > lastSlot) {
            bandStart_.push_back(k);
            lastSlot = slot;
        }
        binBand_[k] = (int)bandStart_.size() - 1;
    }
    bandStart_.push_back(N);
    numBands_ = (int)bandStart_.size() - 1;

    bandBark_.resize(numBands_);
    athPower_.resize(numBands_);
    for (int b = 0; b < numBands_; ++b) {
        int s = bandStart_[b], e = bandStart_[b + 1];
        double fc = ((double)(s + e) * 0.5) * binHz;
        bandBark_[b] = hz_to_bark(fc);
        athPower_[b] = std::pow(10.0, (ath_db(fc) - kAthRefDb) / 10.0);
    }

    spread_.assign(numBands_, std::vector<double>(numBands_, 0.0));
    spreadNorm_.assign(numBands_, 0.0);
    for (int i = 0; i < numBands_; ++i) {
        for (int j = 0; j < numBands_; ++j) {
            double g = std::pow(10.0, spread_db(bandBark_[i] - bandBark_[j]) / 10.0);
            spread_[i][j] = g;
            spreadNorm_[i] += g;
        }
    }
}

void Psycho::thresholds(const double* X, double quality, double maskCapDb,
                        std::vector<double>& thr) const {
    const int B = numBands_;
    std::vector<double> E(B);

    for (int b = 0; b < B; ++b) {
        int s = bandStart_[b], e = bandStart_[b + 1];
        double sum = 0.0;
        for (int k = s; k < e; ++k) sum += X[k] * X[k];
        E[b] = sum / (double)(e - s);
    }

    double total = 0.0;
    for (int b = 0; b < B; ++b) total += E[b];
    double floor = total > 0.0 ? total * 1e-6 / B : 1e-12;
    double logSum = 0.0, linSum = 0.0;
    for (int b = 0; b < B; ++b) {
        double e = E[b] + floor;
        logSum += std::log(e);
        linSum += e;
    }
    double sfm = std::exp(logSum / B) / (linSum / B);
    double sfm_db = 10.0 * std::log10(std::max(sfm, 1e-12));
    double alpha = std::min(1.0, std::max(0.0, sfm_db / -60.0));

    thr.assign(B, 0.0);
    for (int i = 0; i < B; ++i) {
        double spread = 0.0;
        for (int j = 0; j < B; ++j) spread += E[j] * spread_[i][j];
        spread /= spreadNorm_[i];

        double offset_db = alpha * (14.5 + bandBark_[i]) + (1.0 - alpha) * 5.5;
        double t = spread * std::pow(10.0, -offset_db / 10.0);

        double cap = E[i] * std::pow(10.0, maskCapDb / 10.0);
        if (cap > 0.0) t = std::min(t, cap);

        t = std::max(t, athPower_[i]);
        thr[i] = t * quality;
    }
}
