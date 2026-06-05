#pragma once
#include <vector>

class Mdct {
public:
    explicit Mdct(int N);
    int n() const { return N_; }

    void forward(const double* in2N, double* outN) const;
    void inverse(const double* inN, double* out2N) const;

private:
    int N_;
    std::vector<double> cos_;
};
