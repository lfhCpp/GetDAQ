#include "rootstatistics.h"

#include <cmath>
#include <iostream>

namespace {
bool Near(double left, double right)
{
    return std::fabs(left - right) < 1.0e-9;
}
}

int main()
{
    if (!Near(GetDAQ::RootStatistics::EventTimeSeconds(100000000), 1.0)) {
        std::cerr << "event time conversion failed\n";
        return 1;
    }
    if (!Near(GetDAQ::RootStatistics::EventFrequencyHz(150000000, 100000000), 2.0)) {
        std::cerr << "event frequency conversion failed\n";
        return 1;
    }
    if (!Near(GetDAQ::RootStatistics::EventFrequencyHz(10, 10), 0.0)) {
        std::cerr << "zero-delta frequency compatibility failed\n";
        return 1;
    }
    if (!Near(GetDAQ::RootStatistics::EventPeriodMilliseconds(150000, 100000), 0.5)) {
        std::cerr << "event period conversion failed\n";
        return 1;
    }
    if (GetDAQ::RootStatistics::HistogramBin(0, -0.5, 511.5, 512) != 0 ||
        GetDAQ::RootStatistics::HistogramBin(511, -0.5, 511.5, 512) != 511 ||
        GetDAQ::RootStatistics::HistogramBin(272, -0.5, 272.5, 273) != 272 ||
        GetDAQ::RootStatistics::HistogramBin(511.5, -0.5, 511.5, 512) != -1 ||
        GetDAQ::RootStatistics::HistogramBin(0, 1, 1, 4) != -1) {
        std::cerr << "fixed histogram bin boundary failed\n";
        return 1;
    }
    return 0;
}
