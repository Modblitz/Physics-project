#include "pres.hpp"
#include "bitstream.hpp"
#include "predictor.hpp"
#include "rice.hpp"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

constexpr uint8_t kVersion     = 2;
constexpr size_t  kHeaderSize  = 24;
constexpr int     kBlockSize   = 1024;

void put_le16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
}
void put_le32(uint8_t* p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xff);
    p[1] = (uint8_t)((v >> 8) & 0xff);
    p[2] = (uint8_t)((v >> 16) & 0xff);
    p[3] = (uint8_t)((v >> 24) & 0xff);
}
void put_le64(uint8_t* p, uint64_t v) {
    for (int i = 0; i < 8; ++i) p[i] = (uint8_t)((v >> (8 * i)) & 0xff);
}
uint16_t get_le16(const uint8_t* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t get_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

}

bool encode_pres(const std::string& out_path,
                 const std::vector<int16_t>& samples,
                 uint32_t sample_rate,
                 PresEncodeStats& stats,
                 std::string& error) {
    const int K = predictor_window_size();
    if (samples.size() < (size_t)K) {
        error = "input has fewer samples than predictor window";
        return false;
    }

    const size_t total_samples = samples.size();
    const size_t residual_count = total_samples - (size_t)K;

    std::vector<uint8_t> bs_bytes;
    BitWriter bw(bs_bytes);

    std::vector<int32_t> block_buf(kBlockSize);
    std::vector<int32_t> alt_buf(kBlockSize);

    for (size_t off = 0; off < residual_count; off += (size_t)kBlockSize) {
        size_t bn = (off + (size_t)kBlockSize <= residual_count)
                        ? (size_t)kBlockSize
                        : (residual_count - off);

        int best_kind = 0;
        uint64_t best_sum = UINT64_MAX;

        for (int kind = 0; kind < kPredictorCount; ++kind) {
            uint64_t sum_abs = 0;
            std::vector<int32_t>& dst = (kind == 0) ? block_buf : alt_buf;
            bool gave_up = false;
            for (size_t i = 0; i < bn; ++i) {
                const size_t n = (size_t)K + off + i;
                int16_t pred = predict_with_kind(kind, samples.data(), (int)n);
                int32_t r = (int32_t)samples[n] - (int32_t)pred;
                dst[i] = r;
                sum_abs += (uint64_t)((r < 0) ? -(int64_t)r : (int64_t)r);
                if (sum_abs >= best_sum) {
                    gave_up = true;
                    break;
                }
            }
            if (!gave_up && sum_abs < best_sum) {
                best_sum = sum_abs;
                best_kind = kind;
                if (kind != 0) {
                    std::swap(block_buf, alt_buf);
                }
            }
        }

        stats.pred_counts[best_kind]++;

        int k = rice::choose_k(block_buf.data(), (int)bn);
        bw.write_bits((uint32_t)best_kind, kPredictorBits);
        bw.write_bits((uint32_t)k, rice::kKBits);
        for (size_t i = 0; i < bn; ++i) {
            rice::write_residual(bw, block_buf[i], k);
        }
    }
    uint64_t bit_len = bw.total_bits;
    bw.flush();

    uint8_t header[kHeaderSize] = {};
    std::memcpy(header, "PRES", 4);
    header[4] = kVersion;
    header[5] = 0;
    put_le16(header + 6, (uint16_t)K);
    put_le32(header + 8, sample_rate);
    put_le32(header + 12, (uint32_t)samples.size());
    put_le64(header + 16, bit_len);

    std::ofstream f(out_path, std::ios::binary);
    if (!f) { error = "cannot open output file"; return false; }
    f.write((const char*)header, kHeaderSize);
    f.write((const char*)samples.data(), (std::streamsize)(K * 2));
    if (!bs_bytes.empty()) {
        f.write((const char*)bs_bytes.data(),
                (std::streamsize)bs_bytes.size());
    }
    if (!f) { error = "write failed :("; return false; }

    stats.sample_count = samples.size();
    stats.residual_bit_len = bit_len;
    stats.file_size_bytes =
        (uint64_t)(kHeaderSize + K * 2 + bs_bytes.size());
    stats.raw_pcm_bytes = (uint64_t)samples.size() * 2u;
    stats.compression_ratio =
        (double)stats.file_size_bytes / (double)stats.raw_pcm_bytes;
    return true;
}

bool decode_pres(const std::string& in_path,
                 std::vector<int16_t>& samples,
                 uint32_t& sample_rate,
                 std::string& error) {
    std::ifstream f(in_path, std::ios::binary);
    if (!f) { error = "cannot open input file, check path"; return false; }

    uint8_t header[kHeaderSize];
    f.read((char*)header, kHeaderSize);
    if (f.gcount() != (std::streamsize)kHeaderSize) {
        error = "truncated header"; return false;
    }
    if (std::memcmp(header, "PRES", 4) != 0) {
        error = "not a PRES file"; return false;
    }
    if (header[4] != kVersion) {
        error = "unsupported version"; return false;
    }
    const int K_file = (int)get_le16(header + 6);
    sample_rate = get_le32(header + 8);
    const uint32_t sample_count = get_le32(header + 12);

    const int K = predictor_window_size();
    if (K != K_file) {
        error = "predictor window size in file does not match this build";
        return false;
    }
    if (sample_count < (uint32_t)K) {
        error = "sample_count smaller than window";
        return false;
    }

    samples.resize(sample_count);
    f.read((char*)samples.data(), (std::streamsize)(K * 2));
    if (f.gcount() != (std::streamsize)(K * 2)) {
        error = "truncated samples"; return false;
    }

    std::vector<uint8_t> rest((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());

    const size_t residual_count = sample_count - (uint32_t)K;
    BitReader br(rest.data(), rest.size());

    try {
        for (size_t off = 0; off < residual_count; off += (size_t)kBlockSize) {
            size_t bn = (off + (size_t)kBlockSize <= residual_count)
                            ? (size_t)kBlockSize
                            : (residual_count - off);
            int kind = (int)br.read_bits(kPredictorBits);
            int k = (int)br.read_bits(rice::kKBits);
            for (size_t i = 0; i < bn; ++i) {
                int32_t r = rice::read_residual(br, k);
                const size_t n = (size_t)K + off + i;
                int16_t pred = predict_with_kind(kind, samples.data(), (int)n);
                int32_t v = (int32_t)pred + r;
                if (v > 32767 || v < -32768) {
                    error = "reconstructed sample out of int16 range";
                    return false;
                }
                samples[n] = (int16_t)v;
            }
        }
    } catch (const std::exception& e) {
        error = std::string("decode failed: ") + e.what();
        return false;
    }
    return true;
}
