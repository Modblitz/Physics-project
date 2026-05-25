#pragma once
#include <cstdint>

constexpr int kPredictorCount = 6;
constexpr int kPredictorBits  = 3;

enum PredictorKind {
    PredFixed0 = 0,
    PredFixed1 = 1,
    PredFixed2 = 2,
    PredFixed3 = 3,
    PredFixed4 = 4,
    PredMlpOrder2 = 5,
};

int predictor_window_size();

int16_t predict_with_kind(int kind, const int16_t* history, int history_len);
