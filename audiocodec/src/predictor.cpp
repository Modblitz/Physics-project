#include "predictor.hpp"
#include "weights.hpp"

#include <cmath>
#include <cstdint>

namespace {

constexpr float kInvScale = 1.0f / 32768.0f;
constexpr float kScale    = 32768.0f;

int16_t clip_int16(int32_t v) {
    if (v >  32767) return  32767;
    if (v < -32768) return -32768;
    return (int16_t)v;
}

int32_t fixed_predict(int order, const int16_t* h, int n) {
    const int32_t s1 = (n >= 1) ? (int32_t)h[n - 1] : 0;
    const int32_t s2 = (n >= 2) ? (int32_t)h[n - 2] : 0;
    const int32_t s3 = (n >= 3) ? (int32_t)h[n - 3] : 0;
    const int32_t s4 = (n >= 4) ? (int32_t)h[n - 4] : 0;
    switch (order) {
        case 0: return 0;
        case 1: return s1;
        case 2: return 2 * s1 - s2;
        case 3: return 3 * s1 - 3 * s2 + s3;
        case 4: return 4 * s1 - 6 * s2 + 4 * s3 - s4;
    }
    return 0;
}

float mlp_correction(const int16_t* history, int n) {
    using namespace pres_weights;
    float x[K];
    const int start = n - K;
    for (int i = 0; i < K; ++i) x[i] = (float)history[start + i] * kInvScale;

    float h[H];
    for (int j = 0; j < H; ++j) {
        float s = b1[j];
        const float* w = W1[j];
        for (int i = 0; i < K; ++i) s += w[i] * x[i];
        h[j] = std::tanh(s);
    }
    float y = b2;
    for (int j = 0; j < H; ++j) y += W2[j] * h[j];
    return y;
}

}

int predictor_window_size() {
    return pres_weights::K;
}

int16_t predict_with_kind(int kind, const int16_t* history, int n) {
    if (kind <= 4) {
        return clip_int16(fixed_predict(kind, history, n));
    }
    const int32_t linear = fixed_predict(2, history, n);
    const float c = mlp_correction(history, n) * kScale;
    const int32_t correction = (c >= 0.0f)
        ? (int32_t)(c + 0.5f)
        : -(int32_t)(-c + 0.5f);
    return clip_int16(linear + correction);
}
