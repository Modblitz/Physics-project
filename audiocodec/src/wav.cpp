#include "wav.hpp"

#include <cstring>
#include <fstream>

namespace {

uint16_t le16(const unsigned char* p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}
uint32_t le32(const unsigned char* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
void put_le16(std::ofstream& f, uint16_t v) {
    unsigned char b[2] = { (unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff) };
    f.write((const char*)b, 2);
}
void put_le32(std::ofstream& f, uint32_t v) {
    unsigned char b[4] = { (unsigned char)(v & 0xff), (unsigned char)((v >> 8) & 0xff),
                           (unsigned char)((v >> 16) & 0xff), (unsigned char)((v >> 24) & 0xff) };
    f.write((const char*)b, 4);
}
int16_t clip16(double v) {
    long r = (long)(v >= 0.0 ? v + 0.5 : v - 0.5);
    if (r >  32767) return  32767;
    if (r < -32768) return -32768;
    return (int16_t)r;
}

struct WavFmt {
    uint16_t format = 0, channels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
};

bool open_pcm16(std::ifstream& f, WavFmt& fmt, std::vector<int16_t>& inter,
                size_t& frameCount, std::string& error) {
    unsigned char hdr[12];
    f.read((char*)hdr, 12);
    if (f.gcount() != 12 || std::memcmp(hdr, "RIFF", 4) != 0
        || std::memcmp(hdr + 8, "WAVE", 4) != 0) {
        error = "not a RIFF/WAVE file"; return false;
    }
    bool gotFmt = false;
    while (f) {
        unsigned char ch[8];
        f.read((char*)ch, 8);
        if (f.gcount() != 8) break;
        uint32_t size = le32(ch + 4);
        if (std::memcmp(ch, "fmt ", 4) == 0) {
            if (size < 16) { error = "fmt chunk too small"; return false; }
            std::vector<unsigned char> b(size);
            f.read((char*)b.data(), size);
            if ((uint32_t)f.gcount() != size) { error = "truncated fmt"; return false; }
            fmt.format = le16(b.data());
            fmt.channels = le16(b.data() + 2);
            fmt.sampleRate = le32(b.data() + 4);
            fmt.bitsPerSample = le16(b.data() + 14);
            gotFmt = true;
        } else if (std::memcmp(ch, "data", 4) == 0) {
            if (!gotFmt) { error = "data chunk before fmt"; return false; }
            if (fmt.format != 1) { error = "only PCM (format 1) allowed"; return false; }
            if (fmt.bitsPerSample != 16) { error = "only 16-bit samples allowed"; return false; }
            if (fmt.channels == 0) { error = "zero channels"; return false; }
            // Many WAVs declare a data size larger than what is actually present
            // (truncated/streamed files). Clamp to the bytes really available.
            std::streampos dataStart = f.tellg();
            f.seekg(0, std::ios::end);
            std::streamoff avail = f.tellg() - dataStart;
            f.seekg(dataStart);
            if (avail < 0) avail = 0;
            if ((uint64_t)size > (uint64_t)avail) size = (uint32_t)avail;
            frameCount = size / ((size_t)fmt.channels * 2u);
            inter.resize(frameCount * fmt.channels);
            f.read((char*)inter.data(), (std::streamsize)(frameCount * fmt.channels * 2u));
            if ((size_t)f.gcount() != frameCount * fmt.channels * 2u) {
                error = "truncated data chunk"; return false;
            }
            return true;
        } else {
            f.seekg(size + (size & 1u), std::ios::cur);
        }
    }
    error = "no data chunk found";
    return false;
}

void write_pcm16(const std::string& path, const std::vector<int16_t>& inter,
                 int channels, uint32_t sampleRate, std::ofstream& f) {
    const uint32_t dataBytes = (uint32_t)(inter.size() * 2);
    f.write("RIFF", 4);
    put_le32(f, 36u + dataBytes);
    f.write("WAVE", 4);
    f.write("fmt ", 4);
    put_le32(f, 16u);
    put_le16(f, 1);
    put_le16(f, (uint16_t)channels);
    put_le32(f, sampleRate);
    put_le32(f, sampleRate * (uint32_t)channels * 2u);
    put_le16(f, (uint16_t)(channels * 2));
    put_le16(f, 16);
    f.write("data", 4);
    put_le32(f, dataBytes);
    if (!inter.empty()) f.write((const char*)inter.data(), (std::streamsize)dataBytes);
    (void)path;
}

}

bool readWav16(const std::string& path, WavData& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open file"; return false; }
    WavFmt fmt;
    std::vector<int16_t> inter;
    size_t frameCount = 0;
    if (!open_pcm16(f, fmt, inter, frameCount, error)) return false;
    out.sampleRate = fmt.sampleRate;
    out.samples.resize(frameCount);
    for (size_t i = 0; i < frameCount; ++i) out.samples[i] = inter[i * fmt.channels];
    return true;
}

bool writeWav16(const std::string& path, const std::vector<int16_t>& samples,
                uint32_t sampleRate, std::string& error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "cannot open output file"; return false; }
    write_pcm16(path, samples, 1, sampleRate, f);
    if (!f) { error = "write failed"; return false; }
    return true;
}

bool readWav(const std::string& path, Audio& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open file"; return false; }
    WavFmt fmt;
    std::vector<int16_t> inter;
    size_t frameCount = 0;
    if (!open_pcm16(f, fmt, inter, frameCount, error)) return false;
    out.sampleRate = fmt.sampleRate;
    out.channels = (fmt.channels >= 2) ? 2 : 1;
    out.ch[0].resize(frameCount);
    if (out.channels == 2) out.ch[1].resize(frameCount);
    for (size_t i = 0; i < frameCount; ++i) {
        out.ch[0][i] = (double)inter[i * fmt.channels + 0];
        if (out.channels == 2) out.ch[1][i] = (double)inter[i * fmt.channels + 1];
    }
    return true;
}

bool writeWav(const std::string& path, const Audio& in, std::string& error) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { error = "cannot open output file"; return false; }
    const int ch = in.channels;
    const size_t n = in.frames();
    std::vector<int16_t> inter(n * ch);
    for (size_t i = 0; i < n; ++i)
        for (int c = 0; c < ch; ++c)
            inter[i * ch + c] = clip16(in.ch[c][i]);
    write_pcm16(path, inter, ch, in.sampleRate, f);
    if (!f) { error = "write failed"; return false; }
    return true;
}
