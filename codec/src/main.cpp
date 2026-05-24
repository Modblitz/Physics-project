#include "analyser.hpp"
#include "wav.hpp"

#include <cstdio>
#include <string>

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <wav-file>\n", argv[0]);
        return 1;
    }

    WavData wav;
    std::string err;
    if (!readWav16(argv[1], wav, err)) {
        std::fprintf(stderr, "error: %s\n", err.c_str());
        return 1;
    }

    AudioFeatures f = analyse(wav.samples, wav.sampleRate);

    std::printf("file: %s\n", argv[1]);
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
