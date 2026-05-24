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

}

bool readWav16(const std::string& path, WavData& out, std::string& error) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { error = "cannot open file"; return false; }

    unsigned char hdr[12];
    f.read((char*)hdr, 12);
    if (f.gcount() != 12
        || std::memcmp(hdr, "RIFF", 4) != 0
        || std::memcmp(hdr + 8, "WAVE", 4) != 0) {
        error = "not a RIFF/WAVE file";
        return false;
    }

    uint16_t format = 0, channels = 0, bitsPerSample = 0;
    uint32_t sampleRate = 0;
    bool gotFmt = false;

    while (f) {
        unsigned char ch[8];
        f.read((char*)ch, 8);
        if (f.gcount() != 8) break;
        uint32_t size = le32(ch + 4);

        if (std::memcmp(ch, "fmt ", 4) == 0) {
            if (size < 16) { error = "fmt chunk too small"; return false; }
            std::vector<unsigned char> fmtBuf(size);
            f.read((char*)fmtBuf.data(), size);
            if ((uint32_t)f.gcount() != size) { error = "truncated fmt chunk"; return false; }
            format = le16(fmtBuf.data() + 0);
            channels = le16(fmtBuf.data() + 2);
            sampleRate = le32(fmtBuf.data() + 4);
            bitsPerSample = le16(fmtBuf.data() + 14);
            gotFmt = true;
        } else if (std::memcmp(ch, "data", 4) == 0) {
            if (!gotFmt) { error = "data chunk before fmt"; return false; }
            if (format != 1) { error = "only PCM (format 1) allowed"; return false; }
            if (bitsPerSample != 16) { error = "only 16-bit samples allowed"; return false; }
            if (channels == 0) { error = "zero channels"; return false; }

            size_t frameCount = size / ((size_t)channels * 2u);
            std::vector<int16_t> interleaved(frameCount * channels);
            f.read((char*)interleaved.data(),
                   (std::streamsize)(frameCount * channels * 2u));
            if ((size_t)f.gcount() != frameCount * channels * 2u) {
                error = "truncated data chunk"; return false;
            }

            out.samples.resize(frameCount);
            for (size_t i = 0; i < frameCount; ++i) {
                out.samples[i] = interleaved[i * channels];
            }
            out.sampleRate = sampleRate;
            return true;
        } else {
            f.seekg(size + (size & 1u), std::ios::cur);
        }
    }

    error = "no data chunk found";
    return false;
}
