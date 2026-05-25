#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct WavData {
    std::vector<int16_t> samples;
    uint32_t sampleRate = 0;
};

bool readWav16(const std::string& path, WavData& out, std::string& error);

bool writeWav16(const std::string& path,
                const std::vector<int16_t>& samples,
                uint32_t sampleRate,
                std::string& error);
