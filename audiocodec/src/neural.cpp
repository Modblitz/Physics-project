#include "neural.hpp"
#include "bitstream.hpp"
#include "golomb.hpp"
#include "wav.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <random>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

constexpr int F   = 512;
constexpr int HOP = F / 2;
constexpr int L   = 32;
constexpr int kGainScale = 8;

const int ENC_DIMS[] = { F, 256, 96, L };
const int DEC_DIMS[] = { L, 96, 256, F };

struct Layer {
    int in = 0, out = 0;
    bool act_tanh = true;
    std::vector<float> W, b;
    void init(int i, int o, bool t) { in = i; out = o; act_tanh = t; W.assign((size_t)o * i, 0); b.assign(o, 0); }
};

struct Net {
    std::vector<Layer> layers;
    int enc_count = 0;

    void build() {
        layers.clear();
        int ne = (int)(sizeof(ENC_DIMS) / sizeof(int)) - 1;
        for (int k = 0; k < ne; ++k) layers.push_back({}), layers.back().init(ENC_DIMS[k], ENC_DIMS[k + 1], k != ne - 1);
        enc_count = ne;
        int nd = (int)(sizeof(DEC_DIMS) / sizeof(int)) - 1;
        for (int k = 0; k < nd; ++k) layers.push_back({}), layers.back().init(DEC_DIMS[k], DEC_DIMS[k + 1], k != nd - 1);
    }
};

void layer_forward(const Layer& ly, const double* x, double* out) {
    for (int o = 0; o < ly.out; ++o) {
        double s = ly.b[o];
        const float* w = &ly.W[(size_t)o * ly.in];
        for (int i = 0; i < ly.in; ++i) s += w[i] * x[i];
        out[o] = ly.act_tanh ? std::tanh(s) : s;
    }
}

void encode_latent(const Net& net, const double* x, double* z) {
    std::vector<double> a(x, x + F), b;
    for (int k = 0; k < net.enc_count; ++k) {
        b.assign(net.layers[k].out, 0.0);
        layer_forward(net.layers[k], a.data(), b.data());
        a.swap(b);
    }
    for (int k = 0; k < L; ++k) z[k] = a[k];
}

void decode_latent(const Net& net, const double* z, double* y) {
    std::vector<double> a(z, z + L), b;
    for (size_t k = net.enc_count; k < net.layers.size(); ++k) {
        b.assign(net.layers[k].out, 0.0);
        layer_forward(net.layers[k], a.data(), b.data());
        a.swap(b);
    }
    for (int i = 0; i < F; ++i) y[i] = a[i];
}

bool save_net(const std::string& path, const Net& net, std::string& err) {
    std::ofstream f(path, std::ios::binary);
    if (!f) { err = "cannot open model for writing"; return false; }
    f.write("NMD2", 4);
    int32_t nl = (int32_t)net.layers.size(), ec = net.enc_count;
    f.write((const char*)&nl, 4);
    f.write((const char*)&ec, 4);
    for (const Layer& ly : net.layers) {
        int32_t hdr[3] = { ly.in, ly.out, ly.act_tanh ? 1 : 0 };
        f.write((const char*)hdr, sizeof(hdr));
        f.write((const char*)ly.W.data(), (std::streamsize)(ly.W.size() * sizeof(float)));
        f.write((const char*)ly.b.data(), (std::streamsize)(ly.b.size() * sizeof(float)));
    }
    if (!f) { err = "model write failed"; return false; }
    return true;
}

bool load_net(const std::string& path, Net& net, std::string& err) {
    std::ifstream f(path, std::ios::binary);
    if (!f) { err = "cannot open model '" + path + "' (run neural-train first)"; return false; }
    char magic[4];
    int32_t nl = 0, ec = 0;
    f.read(magic, 4);
    f.read((char*)&nl, 4);
    f.read((char*)&ec, 4);
    if (std::memcmp(magic, "NMD2", 4) != 0) { err = "not a neural model file"; return false; }
    net.layers.clear();
    net.enc_count = ec;
    for (int k = 0; k < nl; ++k) {
        int32_t hdr[3];
        f.read((char*)hdr, sizeof(hdr));
        Layer ly; ly.init(hdr[0], hdr[1], hdr[2] != 0);
        f.read((char*)ly.W.data(), (std::streamsize)(ly.W.size() * sizeof(float)));
        f.read((char*)ly.b.data(), (std::streamsize)(ly.b.size() * sizeof(float)));
        net.layers.push_back(std::move(ly));
    }
    if (!f) { err = "model read failed/truncated"; return false; }
    if (net.layers.empty() || net.layers.front().in != F || net.layers.back().out != F) {
        err = "model frame size mismatch this build"; return false;
    }
    return true;
}

uint64_t model_file_size(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f ? (uint64_t)f.tellg() : 0;
}

void sine_window(std::vector<double>& w) {
    w.resize(F);
    for (int n = 0; n < F; ++n) w[n] = std::sin(M_PI / F * (n + 0.5));
}

size_t build_buffer(const std::vector<double>& sig, std::vector<double>& buf) {
    size_t Ln = sig.size();
    size_t body = (size_t)HOP + Ln;
    size_t padded = ((body + HOP - 1) / HOP) * HOP;
    size_t len = padded + (size_t)HOP;
    buf.assign(len, 0.0);
    for (size_t i = 0; i < Ln; ++i) buf[(size_t)HOP + i] = sig[i];
    return len;
}

void mono_mix(const Audio& a, std::vector<double>& m) {
    size_t n = a.frames();
    m.resize(n);
    if (a.channels == 2) for (size_t i = 0; i < n; ++i) m[i] = 0.5 * (a.ch[0][i] + a.ch[1][i]);
    else m = a.ch[0];
}

void put32(std::vector<uint8_t>& v, uint32_t x) { for (int i = 0; i < 4; ++i) v.push_back((x >> (8 * i)) & 0xff); }
uint32_t get32(const uint8_t* p) { return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24); }

}

bool neural_train(const std::string& wav_in, const std::string& model_out,
                  int epochs, std::string& error) {
    Audio au;
    if (!readWav(wav_in, au, error)) return false;
    std::vector<double> mono;
    mono_mix(au, mono);

    std::vector<double> win; sine_window(win);
    std::vector<double> buf;
    size_t len = build_buffer(mono, buf);
    size_t frames = len / HOP - 1;

    std::vector<std::vector<float>> X;
    X.reserve(frames);
    for (size_t fr = 0; fr < frames; ++fr) {
        const double* in = &buf[fr * HOP];
        double e = 0.0;
        std::vector<float> x(F);
        for (int n = 0; n < F; ++n) { double v = in[n] * win[n]; x[n] = (float)v; e += v * v; }
        double g = std::sqrt(e / F);
        if (g < 1.0) continue;
        for (int n = 0; n < F; ++n) x[n] = (float)(x[n] / g);
        X.push_back(std::move(x));
    }
    if (X.size() < 64) { error = "not enough audio to train"; return false; }

    std::mt19937 rng(1234);
    const size_t kMaxSamples = 60000;
    if (X.size() > kMaxSamples) { std::shuffle(X.begin(), X.end(), rng); X.resize(kMaxSamples); }

    Net net; net.build();
    auto unif = [&](double s) { return (float)std::uniform_real_distribution<double>(-s, s)(rng); };
    for (Layer& ly : net.layers) { double s = std::sqrt(1.0 / ly.in); for (auto& w : ly.W) w = unif(s); }

    const int NL = (int)net.layers.size();
    std::vector<std::vector<float>> gW(NL), gb(NL), vW(NL), vb(NL);
    for (int k = 0; k < NL; ++k) {
        gW[k].assign(net.layers[k].W.size(), 0); gb[k].assign(net.layers[k].b.size(), 0);
        vW[k].assign(net.layers[k].W.size(), 0); vb[k].assign(net.layers[k].b.size(), 0);
    }
    std::vector<std::vector<double>> act(NL + 1);
    act[0].assign(F, 0);
    for (int k = 0; k < NL; ++k) act[k + 1].assign(net.layers[k].out, 0);
    std::vector<std::vector<double>> delta(NL);
    for (int k = 0; k < NL; ++k) delta[k].assign(net.layers[k].out, 0);

    const int BATCH = 32;
    double lr = 0.004, mom = 0.9;
    const double clip = 2.0;
    std::vector<size_t> idx(X.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = i;

    for (int ep = 0; ep < epochs; ++ep) {
        std::shuffle(idx.begin(), idx.end(), rng);
        if (ep == epochs / 2) lr *= 0.3;
        if (ep == (epochs * 4) / 5) lr *= 0.3;
        double total = 0.0;
        for (size_t b0 = 0; b0 < idx.size(); b0 += BATCH) {
            for (int k = 0; k < NL; ++k) { std::fill(gW[k].begin(), gW[k].end(), 0.f); std::fill(gb[k].begin(), gb[k].end(), 0.f); }
            size_t bn = std::min<size_t>(BATCH, idx.size() - b0);
            for (size_t bi = 0; bi < bn; ++bi) {
                const float* xf = X[idx[b0 + bi]].data();
                for (int i = 0; i < F; ++i) act[0][i] = xf[i];
                for (int k = 0; k < NL; ++k) layer_forward(net.layers[k], act[k].data(), act[k + 1].data());

                const std::vector<double>& y = act[NL];
                for (int i = 0; i < F; ++i) { double d = y[i] - act[0][i]; delta[NL - 1][i] = d; total += d * d; }
                for (int k = NL - 1; k >= 0; --k) {
                    const Layer& ly = net.layers[k];
                    if (ly.act_tanh)
                        for (int o = 0; o < ly.out; ++o) { double a = act[k + 1][o]; delta[k][o] *= (1.0 - a * a); }
                    const double* ain = act[k].data();
                    float* gw = gW[k].data();
                    float* gbk = gb[k].data();
                    for (int o = 0; o < ly.out; ++o) {
                        double d = delta[k][o];
                        gbk[o] += (float)d;
                        float* row = &gw[(size_t)o * ly.in];
                        for (int i = 0; i < ly.in; ++i) row[i] += (float)(d * ain[i]);
                    }
                    if (k > 0) {
                        std::vector<double>& dprev = delta[k - 1];
                        std::fill(dprev.begin(), dprev.end(), 0.0);
                        for (int o = 0; o < ly.out; ++o) {
                            double d = delta[k][o];
                            const float* row = &ly.W[(size_t)o * ly.in];
                            for (int i = 0; i < ly.in; ++i) dprev[i] += row[i] * d;
                        }
                    }
                }
            }
            double inv = 1.0 / (double)bn;
            auto clamp = [&](double x) { return x > clip ? clip : (x < -clip ? -clip : x); };
            for (int k = 0; k < NL; ++k) {
                Layer& ly = net.layers[k];
                for (size_t i = 0; i < ly.W.size(); ++i) { vW[k][i] = (float)(mom * vW[k][i] - lr * clamp(gW[k][i] * inv)); ly.W[i] += vW[k][i]; }
                for (size_t i = 0; i < ly.b.size(); ++i) { vb[k][i] = (float)(mom * vb[k][i] - lr * clamp(gb[k][i] * inv)); ly.b[i] += vb[k][i]; }
            }
        }
        double mse = total / (double)(X.size() * F);
        std::printf("[neural-train] epoch %2d/%d  mse=%.6f  lr=%.4f\n", ep + 1, epochs, mse, lr);
    }
    return save_net(model_out, net, error);
}

bool neural_encode(const std::string& wav_in, const std::string& out,
                   const std::string& model, double step,
                   NeuralStats& stats, std::string& error) {
    Net net;
    if (!load_net(model, net, error)) return false;
    Audio au;
    if (!readWav(wav_in, au, error)) return false;
    const size_t Ln = au.frames();
    const int channels = au.channels;

    std::vector<double> streams[2];
    if (channels == 2) {
        streams[0].resize(Ln); streams[1].resize(Ln);
        for (size_t i = 0; i < Ln; ++i) {
            streams[0][i] = 0.5 * (au.ch[0][i] + au.ch[1][i]);
            streams[1][i] = 0.5 * (au.ch[0][i] - au.ch[1][i]);
        }
    } else {
        streams[0] = au.ch[0];
    }

    std::vector<double> win; sine_window(win);
    std::vector<uint8_t> bs;
    BitWriter bw(bs);
    std::vector<double> z(L);
    uint32_t num_frames = 0;

    for (int c = 0; c < channels; ++c) {
        std::vector<double> buf;
        size_t len = build_buffer(streams[c], buf);
        size_t frames = len / HOP - 1;
        if (c == 0) num_frames = (uint32_t)frames;
        int32_t prev_g = 0;
        for (size_t fr = 0; fr < frames; ++fr) {
            const double* in = &buf[fr * HOP];
            double x[F], e = 0.0;
            for (int n = 0; n < F; ++n) { double v = in[n] * win[n]; x[n] = v; e += v * v; }
            double g = std::sqrt(e / F);
            int32_t gq = (g < 1e-6) ? -100000 : (int32_t)std::lround(kGainScale * std::log2(g));
            gol::write_int(bw, gq - prev_g, 4);
            prev_g = gq;
            if (gq == -100000) continue;
            double gdq = std::exp2((double)gq / kGainScale);
            for (int n = 0; n < F; ++n) x[n] /= gdq;
            encode_latent(net, x, z.data());
            for (int k = 0; k < L; ++k) gol::write_int(bw, (int32_t)std::lround(z[k] / step), 2);
        }
    }
    bw.flush();

    std::vector<uint8_t> file;
    file.insert(file.end(), {'N','C','O','D'});
    file.push_back((uint8_t)channels); file.push_back(0);
    put32(file, au.sampleRate);
    put32(file, (uint32_t)Ln);
    put32(file, num_frames);
    float sf = (float)step; uint32_t sfu; std::memcpy(&sfu, &sf, 4); put32(file, sfu);
    file.insert(file.end(), bs.begin(), bs.end());

    std::ofstream f(out, std::ios::binary);
    if (!f) { error = "cannot open output file"; return false; }
    f.write((const char*)file.data(), (std::streamsize)file.size());
    if (!f) { error = "write failed"; return false; }

    stats.raw_pcm_bytes = (uint64_t)Ln * channels * 2u;
    stats.file_size_bytes = file.size();
    stats.model_bytes = model_file_size(model);
    stats.compression_ratio = (double)stats.file_size_bytes / (double)stats.raw_pcm_bytes;
    double seconds = (double)Ln / (double)au.sampleRate;
    stats.bitrate_kbps = seconds > 0 ? stats.file_size_bytes * 8.0 / seconds / 1000.0 : 0.0;
    return true;
}

bool neural_decode(const std::string& in, const std::string& wav_out,
                   const std::string& model, std::string& error) {
    Net net;
    if (!load_net(model, net, error)) return false;
    std::ifstream f(in, std::ios::binary);
    if (!f) { error = "cannot open input file"; return false; }
    std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (data.size() < 22 || std::memcmp(data.data(), "NCOD", 4) != 0) { error = "not an NCOD file"; return false; }
    const int channels = data[4];
    const uint32_t sr = get32(&data[6]);
    const uint32_t Ln = get32(&data[10]);
    const uint32_t num_frames = get32(&data[14]);
    float step; uint32_t sfu = get32(&data[18]); std::memcpy(&step, &sfu, 4);

    BitReader br(data.data() + 22, data.size() - 22);
    std::vector<double> win; sine_window(win);
    Audio out; out.sampleRate = sr; out.channels = channels;
    std::vector<std::vector<double>> streams(channels);
    std::vector<double> y(F), z(L);

    for (int c = 0; c < channels; ++c) {
        size_t len = (size_t)(num_frames + 1) * HOP;
        std::vector<double>& buf = streams[c];
        buf.assign(len, 0.0);
        std::vector<double> norm(len, 0.0);
        int32_t prev_g = 0;
        for (uint32_t fr = 0; fr < num_frames; ++fr) {
            int32_t gq = prev_g + gol::read_int(br, 4);
            prev_g = gq;
            if (gq == -100000) continue;
            double gdq = std::exp2((double)gq / kGainScale);
            for (int k = 0; k < L; ++k) z[k] = (double)gol::read_int(br, 2) * step;
            decode_latent(net, z.data(), y.data());
            double* dst = &buf[(size_t)fr * HOP];
            double* nst = &norm[(size_t)fr * HOP];
            for (int n = 0; n < F; ++n) { dst[n] += y[n] * gdq * win[n]; nst[n] += win[n] * win[n]; }
        }
        for (size_t i = 0; i < len; ++i) if (norm[i] > 1e-9) buf[i] /= norm[i];
    }

    for (int c = 0; c < channels; ++c) out.ch[c].resize(Ln);
    if (channels == 2) {
        for (uint32_t i = 0; i < Ln; ++i) {
            double mid = streams[0][(size_t)HOP + i], sid = streams[1][(size_t)HOP + i];
            out.ch[0][i] = mid + sid; out.ch[1][i] = mid - sid;
        }
    } else {
        for (uint32_t i = 0; i < Ln; ++i) out.ch[0][i] = streams[0][(size_t)HOP + i];
    }
    return writeWav(wav_out, out, error);
}
