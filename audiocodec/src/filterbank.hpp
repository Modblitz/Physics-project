#pragma once
#include "mdct.hpp"
#include <vector>

enum WinType { WIN_LONG = 0, WIN_START = 1, WIN_SHORT = 2, WIN_STOP = 3 };

class Filterbank {
public:
    Filterbank(int N, int NS, int nShort);

    int lines() const { return N_; }
    int shortLines() const { return NS_; }
    int numShort() const { return nShort_; }

    void forward(WinType t, const double* in2N, double* coefN) const;
    void inverse(WinType t, const double* coefN, double* out2N) const;

private:
    int N_, NS_, nShort_, base_;
    Mdct mdctLong_, mdctShort_;
    std::vector<double> wLong_, wShort_, wStart_, wStop_;

    const std::vector<double>& longWin(WinType t) const;
};
