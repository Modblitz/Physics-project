#pragma once
#include <cstddef>
#include <cstdint>
#include <vector>

struct BitWriter {
    std::vector<uint8_t>& out;
    uint32_t buf = 0;
    int nbits = 0;
    uint64_t total_bits = 0;

    explicit BitWriter(std::vector<uint8_t>& o) : out(o) {}

    void write_bit(uint32_t b) {
        buf |= (b & 1u) << nbits;
        ++nbits;
        ++total_bits;
        if (nbits == 8) {
            out.push_back((uint8_t)buf);
            buf = 0;
            nbits = 0;
        }
    }
    void write_bits(uint32_t v, int n) {
        for (int i = 0; i < n; ++i) write_bit((v >> i) & 1u);
    }
    void flush() {
        if (nbits > 0) {
            out.push_back((uint8_t)buf);
            buf = 0;
            nbits = 0;
        }
    }
};

struct BitReader {
    const uint8_t* data;
    size_t byte_len;
    uint64_t bit_pos = 0;

    BitReader(const uint8_t* d, size_t bl) : data(d), byte_len(bl) {}

    uint32_t read_bit() {
        size_t byte_idx = (size_t)(bit_pos >> 3);
        int bit_idx = (int)(bit_pos & 7u);
        if (byte_idx >= byte_len) return 0;
        ++bit_pos;
        return (data[byte_idx] >> bit_idx) & 1u;
    }
    uint32_t read_bits(int n) {
        uint32_t v = 0;
        for (int i = 0; i < n; ++i) v |= read_bit() << i;
        return v;
    }
};
