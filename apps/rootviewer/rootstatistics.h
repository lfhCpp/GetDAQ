#ifndef GETDAQ_ROOTSTATISTICS_H
#define GETDAQ_ROOTSTATISTICS_H

#include <cstddef>
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

namespace GetDAQ {
namespace RootStatistics {

enum class Measure {
    EventTime,
    EventFrequency,
    EventPeriod,
    Bucket,
    ChannelCount
};

struct Histogram {
    std::vector<unsigned long long> bins;
    double minimum = 0.0;
    double maximum = 1.0;
    std::uint64_t values = 0;
};

double EventTimeSeconds(std::uint64_t ticks);
double EventFrequencyHz(std::uint64_t currentTicks, std::uint64_t previousTicks);
double EventPeriodMilliseconds(std::uint64_t currentTicks, std::uint64_t previousTicks);
int HistogramBin(double value, double minimum, double maximum, std::size_t numberOfBins);
bool ScanFile(const std::string &path, Measure measure, std::atomic<bool> *cancelled,
              std::atomic<int> *progressPermille, Histogram *histogram, std::string *error);

}
}

#endif
