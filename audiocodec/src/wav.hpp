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

struct Audio {
    uint32_t sampleRate = 0;
    int channels = 0;
    std::vector<double> ch[2];
    size_t frames() const { return ch[0].size(); }
};

bool readWav(const std::string& path, Audio& out, std::string& error);

bool writeWav(const std::string& path, const Audio& in, std::string& error);
