#include "analyser.hpp"
#include "pres.hpp"
#include "wav.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

int run_analyse(const char* path) {
    WavData wav;
    std::string err;
    if (!readWav16(path, wav, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

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

int run_encode(const char* in_path, const char* out_path) {
    WavData wav;
    std::string err;
    if (!readWav16(in_path, wav, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }
    PresEncodeStats st;
    if (!encode_pres(out_path, wav.samples, wav.sampleRate, st, err)) {
        std::fprintf(stderr, "encode error: %s\n", err.c_str());
        return 1;
    }
    std::printf("encoded %s -> %s\n", in_path, out_path);
    std::printf("  samples:     %llu\n", (unsigned long long)st.sample_count);
    std::printf("  raw PCM:     %llu bytes\n",
                (unsigned long long)st.raw_pcm_bytes);
    std::printf("  pres file:   %llu bytes\n",
                (unsigned long long)st.file_size_bytes);
    std::printf("  ratio:       %.3f (%.2f%% of raw)\n",
                st.compression_ratio, st.compression_ratio * 100.0);
    std::printf("  savings:     %.2f%% smaller than raw\n",
                (1.0 - st.compression_ratio) * 100.0);
    return 0;
}

int run_decode(const char* in_path, const char* out_path) {
    std::vector<int16_t> samples;
    uint32_t sr = 0;
    std::string err;
    if (!decode_pres(in_path, samples, sr, err)) {
        std::fprintf(stderr, "decode error: %s\n", err.c_str());
        return 1;
    }
    if (!writeWav16(out_path, samples, sr, err)) {
        std::fprintf(stderr, "wav write error: %s\n", err.c_str());
        return 1;
    }
    std::printf("decoded %s -> %s (%zu samples @ %u Hz)\n",
                in_path, out_path, samples.size(), sr);
    return 0;
}

void usage(const char* prog) {
    std::fprintf(stderr,
        "usage:\n"
        "  %s analyse <in.wav>\n"
        "  %s encode  <in.wav>  <out.pres>\n"
        "  %s decode  <in.pres> <out.wav>\n",
        prog, prog, prog);
}

}

int main(int argc, char** argv) {
    if (argc < 2) { usage(argv[0]); return 1; }

    const char* cmd = argv[1];
    if (std::strcmp(cmd, "analyse") == 0) {
        if (argc != 3) { usage(argv[0]); return 1; }
        return run_analyse(argv[2]);
    } else if (std::strcmp(cmd, "encode") == 0) {
        if (argc != 4) { usage(argv[0]); return 1; }
        return run_encode(argv[2], argv[3]);
    } else if (std::strcmp(cmd, "decode") == 0) {
        if (argc != 4) { usage(argv[0]); return 1; }
        return run_decode(argv[2], argv[3]);
    } else {
        if (argc == 2) return run_analyse(argv[1]);
        usage(argv[0]);
        return 1;
    }
}
