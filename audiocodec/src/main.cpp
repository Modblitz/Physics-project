#include "analyser.hpp"
#include "compare.hpp"
#include "filterbank.hpp"
#include "neural.hpp"
#include "perc.hpp"
#include "pres.hpp"
#include "wav.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

int run_analyse(const char* path) {
    WavData wav;
    std::string err;
    if (!readWav16(path, wav, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    AudioFeatures f = analyse(wav.samples, wav.sampleRate);
    std::printf("file: %s\n", path);
    std::printf("sample rate: %u Hz\n", wav.sampleRate);
    std::printf("samples (mono): %zu\n", wav.samples.size());
    std::printf("duration: %.3f s\n", f.durationSeconds);
    std::printf("peak amplitude: %.6f\n", f.peakAmplitude);
    std::printf("rms amplitude: %.6f\n", f.rmsAmplitude);
    std::printf("crest factor: %.3f\n", f.crestFactor);
    std::printf("zero-crossing rate: %.6f\n", f.zeroCrossingRate);
    std::printf("lag-1 autocorrelation: %.6f\n", f.lag1Autocorr);
    std::printf("spectral centroid: %.2f Hz\n", f.spectralCentroidHz);
    std::printf("spectral flatness: %.6f\n", f.spectralFlatness);
    std::printf("spectral concentration: %.6f\n", f.spectralConcentration);
    return 0;
}

int run_lossless_encode(const char* in, const char* out) {
    WavData wav;
    std::string err;
    if (!readWav16(in, wav, err)) { std::fprintf(stderr, "error: %s\n", err.c_str()); return 1; }
    PresEncodeStats st;
    if (!encode_pres(out, wav.samples, wav.sampleRate, st, err)) {
        std::fprintf(stderr, "encode error: %s\n", err.c_str()); return 1;
    }
    std::printf("lossless encoded %s -> %s\n", in, out);
    std::printf("  raw PCM:   %llu bytes\n", (unsigned long long)st.raw_pcm_bytes);
    std::printf("  pres file: %llu bytes\n", (unsigned long long)st.file_size_bytes);
    std::printf("  ratio:     %.3f (%.2f%% of raw, %.2f%% smaller)\n",
                st.compression_ratio, st.compression_ratio * 100.0,
                (1.0 - st.compression_ratio) * 100.0);
    return 0;
}

int run_lossless_decode(const char* in, const char* out) {
    std::vector<int16_t> samples;
    uint32_t sr = 0;
    std::string err;
    if (!decode_pres(in, samples, sr, err)) { std::fprintf(stderr, "decode error: %s\n", err.c_str()); return 1; }
    if (!writeWav16(out, samples, sr, err)) { std::fprintf(stderr, "wav write error: %s\n", err.c_str()); return 1; }
    std::printf("lossless decoded %s -> %s (%zu samples @ %u Hz)\n", in, out, samples.size(), sr);
    return 0;
}

int run_lossy_encode(const char* in, const char* out, double q) {
    PercStats st;
    std::string err;
    if (!perc_encode(in, out, q, st, err)) { std::fprintf(stderr, "encode error: %s\n", err.c_str()); return 1; }
    std::printf("lossy(perc) encoded %s -> %s  (quality %.2f)\n", in, out, q);
    std::printf("  raw PCM:   %llu bytes\n", (unsigned long long)st.raw_pcm_bytes);
    std::printf("  perc file: %llu bytes\n", (unsigned long long)st.file_size_bytes);
    std::printf("  ratio:     %.4f  (%.2f%% of raw, %.2fx smaller)\n",
                st.compression_ratio, st.compression_ratio * 100.0,
                st.compression_ratio > 0 ? 1.0 / st.compression_ratio : 0.0);
    std::printf("  bitrate:   %.1f kbps\n", st.bitrate_kbps);
    std::printf("  kept coefs:%.2f%%   transient blocks:%.2f%%\n",
                st.nonzero_fraction * 100.0, st.short_frame_fraction * 100.0);
    return 0;
}

int run_lossy_decode(const char* in, const char* out) {
    std::string err;
    if (!perc_decode(in, out, err)) { std::fprintf(stderr, "decode error: %s\n", err.c_str()); return 1; }
    std::printf("lossy(perc) decoded %s -> %s\n", in, out);
    return 0;
}

int run_neural_encode(const char* in, const char* out, double step, const char* model) {
    NeuralStats st;
    std::string err;
    if (!neural_encode(in, out, model, step, st, err)) { std::fprintf(stderr, "encode error: %s\n", err.c_str()); return 1; }
    std::printf("neural encoded %s -> %s  (model %s, step %.3f)\n", in, out, model, step);
    std::printf("  raw PCM:    %llu bytes\n", (unsigned long long)st.raw_pcm_bytes);
    std::printf("  ncod file:  %llu bytes  (excludes shared %s)\n",
                (unsigned long long)st.file_size_bytes, model);
    std::printf("  ratio:      %.4f  (%.2f%% of raw, %.2fx smaller)\n",
                st.compression_ratio, st.compression_ratio * 100.0,
                st.compression_ratio > 0 ? 1.0 / st.compression_ratio : 0.0);
    std::printf("  bitrate:    %.1f kbps  (+ %llu-byte model, amortized)\n",
                st.bitrate_kbps, (unsigned long long)st.model_bytes);
    return 0;
}

int run_neural_decode(const char* in, const char* out, const char* model) {
    std::string err;
    if (!neural_decode(in, out, model, err)) { std::fprintf(stderr, "decode error: %s\n", err.c_str()); return 1; }
    std::printf("neural decoded %s -> %s\n", in, out);
    return 0;
}

int run_neural_train(const char* in, const char* model, int epochs) {
    std::string err;
    if (!neural_train(in, model, epochs, err)) { std::fprintf(stderr, "train error: %s\n", err.c_str()); return 1; }
    std::printf("trained neural model -> %s\n", model);
    return 0;
}

int run_selftest() {
    const int N = 1024, NS = 128, nShort = 8;
    Filterbank fb(N, NS, nShort);
    std::mt19937 rng(7);
    std::uniform_real_distribution<double> dist(-30000.0, 30000.0);
    const size_t Lsig = 60000;
    std::vector<double> sig(Lsig);
    for (auto& v : sig) v = dist(rng);
    size_t body = (size_t)N + Lsig, padded = ((body + N - 1) / N) * N, len = padded + N, frames = len / N - 1;
    std::vector<WinType> wt(frames, WIN_LONG);
    std::uniform_int_distribution<int> coin(0, 1);
    for (size_t fr = 0; fr < frames; ++fr) {
        bool prevOpen = fr > 0 && (wt[fr - 1] == WIN_START || wt[fr - 1] == WIN_SHORT);
        if (prevOpen) wt[fr] = coin(rng) ? WIN_SHORT : WIN_STOP;
        else wt[fr] = (fr + 1 < frames && coin(rng)) ? WIN_START : WIN_LONG;
    }
    if (frames && (wt[frames - 1] == WIN_START || wt[frames - 1] == WIN_SHORT)) wt[frames - 1] = WIN_STOP;
    std::vector<double> buf(len, 0.0), rec(len, 0.0);
    for (size_t i = 0; i < Lsig; ++i) buf[N + i] = sig[i];
    std::vector<double> coef(N), block(2 * N);
    for (size_t fr = 0; fr < frames; ++fr) {
        fb.forward(wt[fr], &buf[fr * N], coef.data());
        fb.inverse(wt[fr], coef.data(), block.data());
        for (int n = 0; n < 2 * N; ++n) rec[fr * N + n] += block[n];
    }
    double maxerr = 0.0;
    for (size_t i = 0; i < Lsig; ++i) maxerr = std::max(maxerr, std::fabs(rec[N + i] - sig[i]));
    std::printf("filterbank round-trip max error: %.3e -> %s\n", maxerr,
                maxerr < 1e-6 ? "PASS" : "FAIL");
    return maxerr < 1e-6 ? 0 : 1;
}

void usage(const char* p) {
    std::fprintf(stderr,
        "audiocodec: lossless + perceptual-lossy + neural-lossy audio codec\n"
        "usage:\n"
        "  %s analyse         <in.wav>\n"
        "  %s lossless-encode <in.wav>  <out.pres>\n"
        "  %s lossless-decode <in.pres> <out.wav>\n"
        "  %s lossy-encode    <in.wav>  <out.perc> [quality=1.0]\n"
        "  %s lossy-decode    <in.perc> <out.wav>\n"
        "  %s neural-train    <in.wav>  [model=neural.model] [epochs=20]\n"
        "  %s neural-encode   <in.wav>  <out.ncod> [step=6.0] [model=neural.model]\n"
        "  %s neural-decode   <in.ncod> <out.wav>  [model=neural.model]\n"
        "  %s compare         <orig.wav> <decoded.wav>\n"
        "  %s selftest\n",
        p, p, p, p, p, p, p, p, p, p);
}

}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }
    const char* cmd = argv[1];
    auto need = [&](int n) { if (argc < n) { usage(argv[0]); std::exit(1); } };

    if (!std::strcmp(cmd, "analyse")) { need(3); return run_analyse(argv[2]); }
    if (!std::strcmp(cmd, "lossless-encode")) { need(4); return run_lossless_encode(argv[2], argv[3]); }
    if (!std::strcmp(cmd, "lossless-decode")) { need(4); return run_lossless_decode(argv[2], argv[3]); }
    if (!std::strcmp(cmd, "lossy-encode")) { need(4); return run_lossy_encode(argv[2], argv[3], argc > 4 ? std::atof(argv[4]) : 1.0); }
    if (!std::strcmp(cmd, "lossy-decode")) { need(4); return run_lossy_decode(argv[2], argv[3]); }
    if (!std::strcmp(cmd, "neural-train")) { need(3); return run_neural_train(argv[2], argc > 3 ? argv[3] : "neural.model", argc > 4 ? std::atoi(argv[4]) : 20); }
    if (!std::strcmp(cmd, "neural-encode")) { need(4); return run_neural_encode(argv[2], argv[3], argc > 4 ? std::atof(argv[4]) : 6.0, argc > 5 ? argv[5] : "neural.model"); }
    if (!std::strcmp(cmd, "neural-decode")) { need(4); return run_neural_decode(argv[2], argv[3], argc > 4 ? argv[4] : "neural.model"); }
    if (!std::strcmp(cmd, "compare")) { need(4); return run_compare(argv[2], argv[3]); }
    if (!std::strcmp(cmd, "selftest")) { return run_selftest(); }
    usage(argv[0]);
    return 1;
}
