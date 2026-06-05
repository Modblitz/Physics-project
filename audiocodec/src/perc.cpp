#include "perc.hpp"
#include "bitstream.hpp"
#include "filterbank.hpp"
#include "golomb.hpp"
#include "psycho.hpp"
#include "wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <vector>

namespace {

constexpr uint8_t kVersion = 2;
constexpr int kN = 1024;
constexpr int kNS = 128;
constexpr int kNShort = 8;
constexpr int kSfScale = 8;
constexpr size_t kHeaderSize = 32;
constexpr int kMbBits = 11;

constexpr double kTransRatio = 4.0;

void put_le16(uint8_t* p, uint16_t v) { p[0]=v; p[1]=v>>8; }
void put_le32(uint8_t* p, uint32_t v) { p[0]=v; p[1]=v>>8; p[2]=v>>16; p[3]=v>>24; }
uint16_t get_le16(const uint8_t* p) { return p[0] | (p[1]<<8); }
uint32_t get_le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8)
         | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

size_t build_buffer(const std::vector<double>& sig, int N, std::vector<double>& buf) {
    size_t L = sig.size();
    size_t body = (size_t)N + L;
    size_t padded = ((body + N - 1) / N) * N;
    size_t len = padded + (size_t)N;
    buf.assign(len, 0.0);
    for (size_t i = 0; i < L; ++i) buf[(size_t)N + i] = sig[i];
    return len;
}

bool is_transient(const double* in, int N) {
    const int S = 16;
    const int seglen = (2 * N) / S;
    double e[16];
    double esum = 0.0;
    for (int s = 0; s < S; ++s) {
        double v2 = 0.0;
        for (int j = 0; j < seglen; ++j) { double v = in[s * seglen + j]; v2 += v * v; }
        e[s] = v2; esum += v2;
    }
    if (esum < 16.0) return false;
    double emean = esum / S;
    double run = e[0] + 1.0;
    for (int s = 0; s < S; ++s) {
        if (e[s] > kTransRatio * emean) return true;
        if (s > 0 && e[s] > kTransRatio * (run / s)) return true;
        run += e[s];
    }
    return false;
}

double temporal_safety(const double* in, int N, double floor) {
    const int S = 8;
    const int seglen = (2 * N) / S;
    double es = 0.0, emin = 1e300;
    for (int s = 0; s < S; ++s) {
        double v2 = 0.0;
        for (int j = 0; j < seglen; ++j) { double v = in[s * seglen + j]; v2 += v * v; }
        es += v2;
        if (v2 < emin) emin = v2;
    }
    double emean = es / S;
    if (emean <= 0.0) return 1.0;
    double s = emin / emean;
    if (s < floor) s = floor;
    if (s > 1.0) s = 1.0;
    return s;
}

struct EncParams {
    double quality = 1.0;
    double maskCapDb = 9.0;
    double deadzone = 0.30;
};

uint64_t encode_group(BitWriter& bw, const double* coef, const Psycho& psy,
                      const EncParams& ep, std::vector<int32_t>& q) {
    const int B = psy.numBands();
    std::vector<double> thr(B), delta(B);
    psy.thresholds(coef, ep.quality, ep.maskCapDb, thr);

    int max_band = 0, M = 0;
    for (int b = 0; b < B; ++b) {
        double d = std::sqrt(12.0 * std::max(thr[b], 1e-12));
        int sf = (int)std::lround(kSfScale * std::log2(d));
        double dq = std::exp2((double)sf / kSfScale);
        delta[b] = dq;
        int s = psy.bandStart(b), e = psy.bandEnd(b);
        for (int k = s; k < e; ++k) {
            double r = coef[k] / dq;
            double a = std::fabs(r);
            int32_t qi = (a < 0.5 + ep.deadzone)
                             ? 0
                             : (int32_t)((r < 0 ? -1 : 1) * std::lround(a - ep.deadzone));
            q[k] = qi;
            if (qi != 0) { ++M; max_band = b + 1; }
        }
    }

    bw.write_bits((uint32_t)max_band, kMbBits);
    if (max_band > 0) {
        std::vector<uint32_t> dz(max_band);
        int32_t prev = 0;
        for (int b = 0; b < max_band; ++b) {
            int32_t sf = (int32_t)std::lround(kSfScale * std::log2(delta[b]));
            dz[b] = gol::zigzag_encode(sf - prev);
            prev = sf;
        }
        int k_sf = gol::choose_k_uint(dz.data(), max_band);
        bw.write_bits((uint32_t)k_sf, 5);
        for (int b = 0; b < max_band; ++b) gol::write_uint(bw, dz[b], k_sf);
    }

    bw.write_bits((uint32_t)M, kMbBits);
    if (M > 0) {
        int limit = psy.bandStart(max_band);
        std::vector<uint32_t> gaps, vals;
        gaps.reserve(M); vals.reserve(M);
        int prev = -1;
        for (int k = 0; k < limit; ++k) {
            if (q[k] != 0) {
                gaps.push_back((uint32_t)(k - prev - 1));
                vals.push_back(gol::zigzag_encode(q[k]));
                prev = k;
            }
        }
        int k_gap = gol::choose_k_uint(gaps.data(), M);
        int k_val = gol::choose_k_uint(vals.data(), M);
        bw.write_bits((uint32_t)k_gap, 5);
        bw.write_bits((uint32_t)k_val, 5);
        for (int i = 0; i < M; ++i) {
            gol::write_uint(bw, gaps[i], k_gap);
            gol::write_uint(bw, vals[i], k_val);
        }
    }
    return (uint64_t)M;
}

void decode_group(BitReader& br, double* coef, const Psycho& psy) {
    const int B = psy.numBands();
    for (int k = 0; k < psy.bandEnd(B - 1); ++k) coef[k] = 0.0;
    std::vector<double> delta(B, 0.0);

    int max_band = (int)br.read_bits(kMbBits);
    if (max_band > B) throw std::runtime_error("max_band out of range");
    if (max_band > 0) {
        int k_sf = (int)br.read_bits(5);
        int32_t prev = 0;
        for (int b = 0; b < max_band; ++b) {
            int32_t sf = prev + gol::zigzag_decode(gol::read_uint(br, k_sf));
            prev = sf;
            delta[b] = std::exp2((double)sf / kSfScale);
        }
    }
    int M = (int)br.read_bits(kMbBits);
    if (M > 0) {
        int k_gap = (int)br.read_bits(5);
        int k_val = (int)br.read_bits(5);
        int limit = psy.bandStart(max_band);
        int k = -1;
        for (int i = 0; i < M; ++i) {
            uint32_t gap = gol::read_uint(br, k_gap);
            int32_t val = gol::zigzag_decode(gol::read_uint(br, k_val));
            k += (int)gap + 1;
            if (k < 0 || k >= limit) throw std::runtime_error("coef index out of range");
            coef[k] = (double)val * delta[psy.bandOfBin(k)];
        }
    }
}

std::vector<WinType> assign_windows(const std::vector<double>& buf, int N,
                                    size_t frames) {
    std::vector<char> shortf(frames, 0);
    bool no_short = std::getenv("PERC_NOSHORT") != nullptr;
    for (size_t fr = 0; fr < frames; ++fr)
        shortf[fr] = (!no_short && is_transient(&buf[fr * N], N)) ? 1 : 0;
    for (size_t fr = 1; fr + 1 < frames; ++fr)
        if (shortf[fr - 1] && shortf[fr + 1]) shortf[fr] = 1;
    if (frames) shortf[0] = 0;
    if (frames) shortf[frames - 1] = 0;

    std::vector<WinType> wt(frames, WIN_LONG);
    for (size_t fr = 0; fr < frames; ++fr) {
        if (!shortf[fr]) continue;
        size_t a = fr, b = fr;
        while (b + 1 < frames && shortf[b + 1]) ++b;
        for (size_t i = a; i <= b; ++i) wt[i] = WIN_SHORT;
        if (a > 0 && wt[a - 1] == WIN_LONG) wt[a - 1] = WIN_START;
        if (b + 1 < frames && wt[b + 1] == WIN_LONG) wt[b + 1] = WIN_STOP;
        fr = b;
    }
    return wt;
}

}

bool perc_encode(const std::string& wav_in, const std::string& perc_out,
                 double quality, PercStats& stats, std::string& error) {
    Audio au;
    if (!readWav(wav_in, au, error)) return false;

    const int N = kN;
    const int channels = au.channels;
    const size_t L = au.frames();
    if (L == 0) { error = "empty audio"; return false; }

    std::vector<double> streams[2];
    if (channels == 2) {
        streams[0].resize(L); streams[1].resize(L);
        for (size_t i = 0; i < L; ++i) {
            streams[0][i] = 0.5 * (au.ch[0][i] + au.ch[1][i]);
            streams[1][i] = 0.5 * (au.ch[0][i] - au.ch[1][i]);
        }
    } else {
        streams[0] = au.ch[0];
    }

    Filterbank fb(kN, kNS, kNShort);
    Psycho psyLong(kN, au.sampleRate);
    Psycho psyShort(kNS, au.sampleRate);

    EncParams ep;
    ep.quality = quality;
    const double ts_floor = 0.04;

    std::vector<uint8_t> bs_bytes;
    BitWriter bw(bs_bytes);

    std::vector<double> coef(N);
    std::vector<int32_t> q(N);
    uint64_t num_frames = 0, total_nonzero = 0, short_frames = 0;

    for (int c = 0; c < channels; ++c) {
        std::vector<double> buf;
        size_t len = build_buffer(streams[c], N, buf);
        size_t frames = len / N - 1;
        if (c == 0) num_frames = frames;

        std::vector<WinType> wt = assign_windows(buf, N, frames);

        for (size_t fr = 0; fr < frames; ++fr) {
            const double* in = &buf[fr * N];
            WinType t = wt[fr];
            fb.forward(t, in, coef.data());
            bw.write_bits((uint32_t)t, 2);

            if (t == WIN_SHORT) {
                ++short_frames;
                for (int b = 0; b < kNShort; ++b)
                    total_nonzero += encode_group(bw, coef.data() + (size_t)b * kNS,
                                                   psyShort, ep, q);
            } else {
                EncParams epf = ep;
                epf.quality = ep.quality * temporal_safety(in, N, ts_floor);
                total_nonzero += encode_group(bw, coef.data(), psyLong, epf, q);
            }
        }
    }
    bw.flush();

    uint8_t header[kHeaderSize] = {};
    std::memcpy(header, "PERC", 4);
    header[4] = kVersion;
    header[5] = (uint8_t)channels;
    put_le16(header + 6, (uint16_t)N);
    put_le32(header + 8, au.sampleRate);
    put_le32(header + 12, (uint32_t)L);
    put_le32(header + 16, (uint32_t)num_frames);
    put_le16(header + 20, (uint16_t)psyLong.numBands());
    put_le16(header + 22, (uint16_t)kSfScale);
    float qf = (float)quality;
    std::memcpy(header + 24, &qf, 4);
    put_le16(header + 28, (uint16_t)kNS);
    header[30] = (uint8_t)kNShort;

    std::ofstream f(perc_out, std::ios::binary);
    if (!f) { error = "cannot open output file"; return false; }
    f.write((const char*)header, kHeaderSize);
    if (!bs_bytes.empty())
        f.write((const char*)bs_bytes.data(), (std::streamsize)bs_bytes.size());
    if (!f) { error = "write failed"; return false; }

    stats.raw_pcm_bytes = (uint64_t)L * (uint64_t)channels * 2u;
    stats.file_size_bytes = kHeaderSize + bs_bytes.size();
    stats.num_frames = num_frames;
    stats.compression_ratio = (double)stats.file_size_bytes / (double)stats.raw_pcm_bytes;
    double seconds = (double)L / (double)au.sampleRate;
    stats.bitrate_kbps = seconds > 0 ? (stats.file_size_bytes * 8.0 / seconds / 1000.0) : 0.0;
    uint64_t denom = num_frames * (uint64_t)channels * (uint64_t)N;
    stats.nonzero_fraction = denom ? (double)total_nonzero / (double)denom : 0.0;
    uint64_t fc = num_frames * (uint64_t)channels;
    stats.short_frame_fraction = fc ? (double)short_frames / (double)fc : 0.0;
    return true;
}

bool perc_decode(const std::string& perc_in, const std::string& wav_out,
                 std::string& error) {
    std::ifstream f(perc_in, std::ios::binary);
    if (!f) { error = "cannot open input file"; return false; }

    uint8_t header[kHeaderSize];
    f.read((char*)header, kHeaderSize);
    if (f.gcount() != (std::streamsize)kHeaderSize) { error = "truncated header"; return false; }
    if (std::memcmp(header, "PERC", 4) != 0) { error = "not a PERC file"; return false; }
    if (header[4] != kVersion) { error = "unsupported version"; return false; }

    const int channels = header[5];
    const int N = get_le16(header + 6);
    const uint32_t sampleRate = get_le32(header + 8);
    const uint32_t L = get_le32(header + 12);
    const uint32_t num_frames = get_le32(header + 16);
    const int NS = get_le16(header + 28);
    const int nShort = header[30];
    if (N != kN || NS != kNS || nShort != kNShort) { error = "stream/build mismatch"; return false; }

    std::vector<uint8_t> rest((std::istreambuf_iterator<char>(f)),
                              std::istreambuf_iterator<char>());
    BitReader br(rest.data(), rest.size());

    Filterbank fb(kN, kNS, kNShort);
    Psycho psyLong(kN, sampleRate);
    Psycho psyShort(kNS, sampleRate);

    Audio out;
    out.sampleRate = sampleRate;
    out.channels = channels;
    std::vector<std::vector<double>> streams(channels);
    std::vector<double> coef(N), block(2 * N);

    try {
        for (int c = 0; c < channels; ++c) {
            size_t len = (size_t)(num_frames + 1) * N;
            std::vector<double>& buf = streams[c];
            buf.assign(len, 0.0);

            for (uint32_t fr = 0; fr < num_frames; ++fr) {
                WinType t = (WinType)br.read_bits(2);
                if (t == WIN_SHORT) {
                    for (int b = 0; b < kNShort; ++b)
                        decode_group(br, coef.data() + (size_t)b * kNS, psyShort);
                } else {
                    decode_group(br, coef.data(), psyLong);
                }
                fb.inverse(t, coef.data(), block.data());
                double* dst = &buf[(size_t)fr * N];
                for (int n = 0; n < 2 * N; ++n) dst[n] += block[n];
            }
        }
    } catch (const std::exception& e) {
        error = std::string("decode failed: ") + e.what();
        return false;
    }

    for (int c = 0; c < channels; ++c) out.ch[c].resize(L);
    if (channels == 2) {
        for (uint32_t i = 0; i < L; ++i) {
            double m = streams[0][(size_t)N + i];
            double s = streams[1][(size_t)N + i];
            out.ch[0][i] = m + s;
            out.ch[1][i] = m - s;
        }
    } else {
        for (uint32_t i = 0; i < L; ++i) out.ch[0][i] = streams[0][(size_t)N + i];
    }

    if (!writeWav(wav_out, out, error)) return false;
    return true;
}
