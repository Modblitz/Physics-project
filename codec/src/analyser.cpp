#include "analyser.hpp"
#include "fft.hpp"

#include <algorithm>
#include <cmath>
#include <functional>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

bool isPow2(size_t n) { return n != 0 && (n & (n - 1)) == 0; }
size_t prevPow2(size_t n) {
    size_t p = 1;
    while ((p << 1) <= n) p <<= 1;
    return p;
}

}

AudioFeatures analyse(const std::vector<int16_t>& samples, uint32_t sampleRate) {
    AudioFeatures r{};
    if (samples.empty() || sampleRate == 0) return r;

    const size_t N = samples.size();
    r.durationSeconds = (double)N / (double)sampleRate;

    double sumSq = 0.0, peak = 0.0;
    size_t zc = 0;
    for (size_t i = 0; i < N; ++i) {
        double x = (double)samples[i] / 32768.0;
        sumSq += x * x;
        double ax = std::fabs(x);
        if (ax > peak) peak = ax;
        if (i > 0 && ((samples[i - 1] >= 0) != (samples[i] >= 0))) ++zc;
    }
    r.peakAmplitude = peak;
    r.rmsAmplitude = std::sqrt(sumSq / (double)N);
    r.crestFactor = (r.rmsAmplitude > 1e-12) ? (peak / r.rmsAmplitude) : 0.0;
    r.zeroCrossingRate  = (N > 1) ? ((double)zc / (double)(N - 1)) : 0.0;

    double sxx = 0.0, sxy = 0.0;
    for (size_t i = 1; i < N; ++i) {
        double a = (double)samples[i - 1];
        double b = (double)samples[i];
        sxx += a * a;
        sxy += a * b;
    }
    r.lag1Autocorr = (sxx > 1e-6) ? (sxy / sxx) : 0.0;

    size_t fftN = 2048;
    if (fftN > N) fftN = prevPow2(N);
    if (fftN < 16) {

        return r;
    }
    if (!isPow2(fftN)) fftN = prevPow2(fftN);

    const size_t halfN = fftN / 2 + 1;
    std::vector<double> avgPower(halfN, 0.0);
    std::vector<Complex> buf(fftN);

    std::vector<double> win(fftN);
    for (size_t i = 0; i < fftN; ++i)
        win[i] = 0.5 - 0.5 * std::cos(2.0 * M_PI * (double)i / (double)fftN);

    size_t maxBlocks = (N >= fftN) ? std::min<size_t>(16, N / fftN) : 0;
    if (maxBlocks == 0) maxBlocks = 1;
    size_t hop = (maxBlocks > 1 && N > fftN) ? (N - fftN) / (maxBlocks - 1) : fftN;
    size_t numBlocks = 0;
    for (size_t b = 0; b < maxBlocks; ++b) {
        size_t start = b * hop;
        if (start + fftN > N) break;
        for (size_t k = 0; k < fftN; ++k) {
            double s = (double)samples[start + k] / 32768.0 * win[k];
            buf[k] = Complex(s, 0.0);
        }
        fft_inplace(buf, false);
        for (size_t i = 0; i < halfN; ++i)
            avgPower[i] += std::norm(buf[i]);
        ++numBlocks;
    }
    if (numBlocks == 0) return r;
    for (auto& p : avgPower) p /= (double)numBlocks;

    double pSum = 0.0, fW = 0.0;
    for (size_t i = 0; i < halfN; ++i) {
        pSum += avgPower[i];
        fW   += avgPower[i] * (double)i * (double)sampleRate / (double)fftN;
    }
    r.spectralCentroidHz = (pSum > 1e-30) ? (fW / pSum) : 0.0;

    double logSum = 0.0, linSum = 0.0;
    size_t cnt = 0;
    for (size_t i = 1; i < halfN; ++i) {
        double p = avgPower[i];
        if (p > 1e-30) {
            logSum += std::log(p);
            linSum += p;
            ++cnt;
        }
    }
    if (cnt > 0) {
        double gm = std::exp(logSum / (double)cnt);
        double am = linSum / (double)cnt;
        r.spectralFlatness = (am > 1e-30) ? (gm / am) : 0.0;
    }

    std::vector<double> sorted = avgPower;
    std::sort(sorted.begin(), sorted.end(), std::greater<double>());
    size_t topK = std::max<size_t>(1, halfN / 5);
    double topE = 0.0, allE = 0.0;
    for (size_t i = 0; i < halfN; ++i) {
        allE += sorted[i];
        if (i < topK) topE += sorted[i];
    }
    r.spectralConcentration = (allE > 1e-30) ? (topE / allE) : 0.0;

    return r;
}
