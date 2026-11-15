#include "rootstatistics.h"
#include <cmath>
#include <limits>

namespace GetDAQ {
namespace RootStatistics {

double EventTimeSeconds(std::uint64_t ticks)
{
    return static_cast<double>(ticks) / 1.0e8;
}

double EventFrequencyHz(std::uint64_t currentTicks, std::uint64_t previousTicks)
{
    const std::uint64_t delta = currentTicks - previousTicks;
    return delta ? 1.0e8 / static_cast<double>(delta) : 0.0;
}

double EventPeriodMilliseconds(std::uint64_t currentTicks, std::uint64_t previousTicks)
{
    const std::uint64_t delta = currentTicks - previousTicks;
    return static_cast<double>(delta) / 1.0e5;
}

int HistogramBin(double value, double minimum, double maximum, std::size_t numberOfBins)
{
    if (!std::isfinite(value) || !std::isfinite(minimum) || !std::isfinite(maximum) ||
        maximum <= minimum || numberOfBins == 0 ||
        numberOfBins > static_cast<std::size_t>(std::numeric_limits<int>::max()) ||
        value < minimum || value >= maximum)
        return -1;
    const double scaled = (value - minimum) / (maximum - minimum) * numberOfBins;
    const int bin = static_cast<int>(scaled);
    return bin >= 0 && static_cast<std::size_t>(bin) < numberOfBins ? bin : -1;
}


}
}
