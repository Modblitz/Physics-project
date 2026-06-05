#pragma once
#include <cstdint>
#include <string>

struct PercStats {
    uint64_t raw_pcm_bytes = 0;
    uint64_t file_size_bytes = 0;
    uint64_t num_frames = 0;
    double compression_ratio = 0.0;
    double bitrate_kbps = 0.0;
    double nonzero_fraction = 0.0;
    double short_frame_fraction = 0.0;
};

bool perc_encode(const std::string& wav_in, const std::string& perc_out,
                 double quality, PercStats& stats, std::string& error);

bool perc_decode(const std::string& perc_in, const std::string& wav_out,
                 std::string& error);
