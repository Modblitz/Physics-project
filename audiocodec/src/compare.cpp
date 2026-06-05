#include "compare.hpp"
#include "fft.hpp"
#include "wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

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

}

int run_compare(const std::string& orig_path, const std::string& dec_path) {
    Audio a, b;
    std::string err;
    if (!readWav(orig_path, a, err)) { std::fprintf(stderr, "orig: %s\n", err.c_str()); return 1; }
    if (!readWav(dec_path, b, err))  { std::fprintf(stderr, "dec:  %s\n", err.c_str()); return 1; }

    if (a.channels != b.channels) {
        std::fprintf(stderr, "channel count differs (%d vs %d)\n", a.channels, b.channels);
        return 1;
    }
    size_t L = std::min(a.frames(), b.frames());
    if (L == 0) { std::fprintf(stderr, "empty audio\n"); return 1; }
    if (a.frames() != b.frames())
        std::printf("note: length differs (%zu vs %zu), comparing %zu\n",
                    a.frames(), b.frames(), L);

    const int W = 2048;
    const int hop = W / 2;
    const double sr = (double)a.sampleRate;

    std::vector<double> binBark(W / 2 + 1);
    for (int k = 0; k <= W / 2; ++k) binBark[k] = hz_to_bark((double)k * sr / W);
    int nb = (int)binBark[W / 2] + 1;
    std::vector<int> bandStart(nb + 1, W / 2 + 1);
    bandStart[0] = 1;
    for (int k = 1; k <= W / 2; ++k) {
        int band = (int)binBark[k];
        if (band + 1 < (int)bandStart.size() && bandStart[band + 1] > k + 1)
            bandStart[band + 1] = std::min<int>(bandStart[band + 1], k + 1);
    }
    std::vector<int> bs(nb + 1);
    bs[0] = 1;
    int cur = 0;
    for (int k = 1; k <= W / 2; ++k) {
        int band = (int)binBark[k];
        if (band > cur) { for (int bb = cur + 1; bb <= band; ++bb) bs[bb] = k; cur = band; }
    }
    bs[nb] = W / 2 + 1;
    for (int bnd = 1; bnd <= nb; ++bnd) if (bs[bnd] < bs[bnd - 1]) bs[bnd] = bs[bnd - 1];

    std::vector<double> bandBark(nb), bandNbins(nb), athPow(nb);
    for (int bnd = 0; bnd < nb; ++bnd) {
        int s = bs[bnd], e = bs[bnd + 1];
        double fc = ((double)(s + e) * 0.5) * sr / W;
        bandBark[bnd] = hz_to_bark(fc);
        bandNbins[bnd] = std::max(1, e - s);
        constexpr double kSplOffset = 96.0 - 143.0;
        athPow[bnd] = std::pow(10.0, (ath_db(fc) - kSplOffset) / 10.0);
    }
    std::vector<std::vector<double>> spread(nb, std::vector<double>(nb, 0.0));
    for (int i = 0; i < nb; ++i)
        for (int j = 0; j < nb; ++j)
            spread[i][j] = std::pow(10.0, spread_db(bandBark[i] - bandBark[j]) / 10.0);

    std::vector<double> win(W);
    for (int n = 0; n < W; ++n) win[n] = 0.5 - 0.5 * std::cos(2.0 * M_PI * n / W);

    const double hop_ms = 1000.0 * hop / sr;
    const double fwd_decay = std::pow(10.0, -(hop_ms * 0.15) / 10.0);
    const double bwd_gain  = std::pow(10.0, -15.0 / 10.0);

    double sig_energy = 0.0, err_energy = 0.0, max_abs = 0.0;
    double seg_snr_sum = 0.0;
    long seg_count = 0;
    double nmr_max[2] = {-1e30, -1e30}, nmr_sum[2] = {0, 0};
    long nmr_count = 0, nmr_audible[2] = {0, 0};
    double worst_fc = 0, worst_t = 0, worst_S = 0, worst_N = 0, worst_mask = 0;
    int worst_ch = 0;

    std::vector<Complex> fs(W), fe(W);

    for (int c = 0; c < a.channels; ++c) {
        const std::vector<double>& xa = a.ch[c];
        const std::vector<double>& xb = b.ch[c];
        for (size_t i = 0; i < L; ++i) {
            double e = xa[i] - (i < xb.size() ? xb[i] : 0.0);
            sig_energy += xa[i] * xa[i];
            err_energy += e * e;
            double ae = std::fabs(e);
            if (ae > max_abs) max_abs = ae;
        }

        size_t nframes = (L >= (size_t)W) ? (L - W) / hop + 1 : 0;
        std::vector<std::vector<double>> S(nframes, std::vector<double>(nb)),
                                         Nerr(nframes, std::vector<double>(nb));
        for (size_t f = 0; f < nframes; ++f) {
            size_t start = f * hop;
            double se = 0.0, ne = 0.0;
            for (int n = 0; n < W; ++n) {
                double s = xa[start + n];
                double e = s - xb[start + n];
                fs[n] = Complex(s * win[n], 0.0);
                fe[n] = Complex(e * win[n], 0.0);
                se += s * s; ne += e * e;
            }
            if (se > 1e-9) { seg_snr_sum += 10.0 * std::log10(se / std::max(ne, 1e-12)); ++seg_count; }
            fft_inplace(fs, false);
            fft_inplace(fe, false);
            for (int bnd = 0; bnd < nb; ++bnd) {
                double s = 0.0, n = 0.0;
                for (int k = bs[bnd]; k < bs[bnd + 1]; ++k) { s += std::norm(fs[k]); n += std::norm(fe[k]); }
                S[f][bnd] = s; Nerr[f][bnd] = n;
            }
        }

        std::vector<double> post(nb, 0.0);
        std::vector<double> Eeff(nb), mask_str(nb), mask_tmp(nb);
        for (size_t f = 0; f < nframes; ++f) {
            for (int b = 0; b < nb; ++b) {
                double eff = S[f][b];
                eff = std::max(eff, post[b]);
                if (f + 1 < nframes) eff = std::max(eff, S[f + 1][b] * bwd_gain);
                Eeff[b] = eff;
            }
            for (int i = 0; i < nb; ++i) {
                double spStr = 0.0, spTmp = 0.0;
                for (int j = 0; j < nb; ++j) { spStr += S[f][j] * spread[i][j]; spTmp += Eeff[j] * spread[i][j]; }
                double off = std::pow(10.0, -9.0 / 10.0);
                mask_str[i] = std::max(spStr * off, athPow[i] * bandNbins[i]);
                mask_tmp[i] = std::max(spTmp * off, athPow[i] * bandNbins[i]);
            }
            for (int i = 0; i < nb; ++i) {
                if (S[f][i] < athPow[i] * bandNbins[i] && Nerr[f][i] < athPow[i] * bandNbins[i])
                    continue;
                double n_e = std::max(Nerr[f][i], 1e-12);
                double nmrS = 10.0 * std::log10(n_e / std::max(mask_str[i], 1e-12));
                double nmrT = 10.0 * std::log10(n_e / std::max(mask_tmp[i], 1e-12));
                ++nmr_count;
                nmr_sum[0] += nmrS; nmr_sum[1] += nmrT;
                if (nmrS > 0.0) ++nmr_audible[0];
                if (nmrT > 0.0) ++nmr_audible[1];
                if (nmrS > nmr_max[0]) nmr_max[0] = nmrS;
                if (nmrT > nmr_max[1]) {
                    nmr_max[1] = nmrT;
                    worst_fc = bandBark[i]; worst_t = (double)(f * hop) / sr;
                    worst_S = S[f][i]; worst_N = Nerr[f][i]; worst_mask = mask_tmp[i]; worst_ch = c;
                }
            }
            for (int b = 0; b < nb; ++b) post[b] = std::max(S[f][b], post[b] * fwd_decay);
        }
    }

    double snr = 10.0 * std::log10(sig_energy / std::max(err_energy, 1e-12));
    double seg_snr = seg_count ? seg_snr_sum / seg_count : 0.0;

    std::printf("PERC\n");
    std::printf("channels:           %d\n", a.channels);
    std::printf("samples/ch:         %zu  (%.1f s)\n", L, (double)L / sr);
    std::printf("max sample error:   %.1f  (of full-scale 32768)\n", max_abs);
    std::printf("overall SNR:        %.2f dB   (raw waveform; not perceptual)\n", snr);
    std::printf("segmental SNR:      %.2f dB\n", seg_snr);
    std::printf("Noise-to-Mask Ratio (independent model, 0 dB = at threshold)\n");
    std::printf("simultaneous-only : mean %+.2f dB   worst %+.2f dB   audible %.3f%%\n",
                nmr_count ? nmr_sum[0] / nmr_count : 0.0, nmr_max[0],
                nmr_count ? 100.0 * nmr_audible[0] / nmr_count : 0.0);
    std::printf("+ temporal masking: mean %+.2f dB   worst %+.2f dB   audible %.3f%%\n",
                nmr_count ? nmr_sum[1] / nmr_count : 0.0, nmr_max[1],
                nmr_count ? 100.0 * nmr_audible[1] / nmr_count : 0.0);
    std::printf("  worst (temporal) at: ch%d t=%.3fs bark=%.1f sigE=%.2e errE=%.2e maskE=%.2e\n",
                worst_ch, worst_t, worst_fc, worst_S, worst_N, worst_mask);
    double audible_pct = nmr_count ? 100.0 * nmr_audible[1] / nmr_count : 0.0;
    std::printf("verdict: %s\n",
        (nmr_max[1] <= 1.0)
            ? "TRANSPARENT (error below the masking threshold everywhere)"
            : (audible_pct < 0.5
                ? "ESSENTIALLY TRANSPARENT (>99.5% below threshold; rare marginal\n"
                : "audible differences predicted; lower the quality value"));
    return 0;
}
