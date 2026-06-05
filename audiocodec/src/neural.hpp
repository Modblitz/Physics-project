#pragma once
#include <cstdint>
#include <string>

struct NeuralStats {
    uint64_t raw_pcm_bytes = 0;
    uint64_t file_size_bytes = 0;
    uint64_t model_bytes = 0;
    double compression_ratio = 0.0;
    double bitrate_kbps = 0.0;
};

bool neural_train(const std::string& wav_in, const std::string& model_out,
                  int epochs, std::string& error);

bool neural_encode(const std::string& wav_in, const std::string& out,
                   const std::string& model, double step,
                   NeuralStats& stats, std::string& error);

bool neural_decode(const std::string& in, const std::string& wav_out,
                   const std::string& model, std::string& error);
