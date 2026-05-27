#pragma once
#include <cstdint>
#include <vector>

/* creates a struct named AudioFeatures which contains info for the audio file
  kinda self explanatory
    */
struct AudioFeatures {
    double durationSeconds = 0.0;
    double peakAmplitude = 0.0;
    double rmsAmplitude = 0.0;
    double crestFactor = 0.0;
    double zeroCrossingRate = 0.0;
    double lag1Autocorr = 0.0;
    double spectralCentroidHz = 0.0;
    double spectralFlatness = 0.0;
    double spectralConcentration = 0.0;
};

AudioFeatures analyse(const std::vector<int16_t>& samples, uint32_t sampleRate);
