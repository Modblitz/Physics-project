#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct PresEncodeStats {
    uint64_t sample_count = 0;
    uint64_t residual_bit_len = 0;
    uint64_t file_size_bytes = 0;
    uint64_t raw_pcm_bytes = 0;
    double   compression_ratio = 0.0;
    uint64_t pred_counts[6] = {0,0,0,0,0,0};
};

bool encode_pres(const std::string& out_path,
                 const std::vector<int16_t>& samples,
                 uint32_t sample_rate,
                 PresEncodeStats& stats,
                 std::string& error);

bool decode_pres(const std::string& in_path,
                 std::vector<int16_t>& samples,
                 uint32_t& sample_rate,
                 std::string& error);
