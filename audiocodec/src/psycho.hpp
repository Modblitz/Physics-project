#pragma once
#include <vector>

class Psycho {
public:
    Psycho(int N, double sampleRate);

    int numBands() const { return numBands_; }
    int bandStart(int b) const { return bandStart_[b]; }
    int bandEnd(int b) const   { return bandStart_[b + 1]; }
    int bandOfBin(int k) const { return binBand_[k]; }

    void thresholds(const double* X, double quality, double maskCapDb,
                    std::vector<double>& thr) const;

private:
    int N_;
    int numBands_;
    std::vector<int> bandStart_;
    std::vector<int> binBand_;
    std::vector<double> bandBark_;
    std::vector<double> athPower_;
    std::vector<std::vector<double>> spread_;
    std::vector<double> spreadNorm_;
};
