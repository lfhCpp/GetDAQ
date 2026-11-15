#include "rootstatistics.h"
#include "roottreereader.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
namespace RootStatistics {

namespace {
double MeasureEvent(Measure measure, const RootEvent &event, std::uint64_t previousTime)
{
    if (measure == Measure::EventTime) return EventTimeSeconds(event.eventTime);
    if (measure == Measure::EventFrequency) return EventFrequencyHz(event.eventTime, previousTime);
    return EventPeriodMilliseconds(event.eventTime, previousTime);
}

void UpdateProgress(std::atomic<int> *progressPermille, int pass,
                    std::int64_t entry, std::int64_t entries)
{
    if (!progressPermille || entries <= 0) return;
    const double completed = (double(pass) * entries + entry + 1) / (2.0 * entries);
    progressPermille->store(static_cast<int>(completed * 1000.0));
}
}

bool ScanFile(const std::string &path, Measure measure, std::atomic<bool> *cancelled,
              std::atomic<int> *progressPermille, Histogram *histogram, std::string *error)
{
    try {
        if (!histogram) throw std::invalid_argument("ROOT statistics histogram output is null");
        RootTreeReader reader;
        std::string readerError;
        if (!reader.Open(path, &readerError)) throw std::runtime_error(readerError);
        const std::int64_t entries = reader.Entries();
        if (entries <= 0 || entries > std::numeric_limits<int>::max())
            throw std::runtime_error("ROOT statistics require between 1 and INT_MAX entries");

        Histogram result;
        const bool fixedRange = measure == Measure::Bucket || measure == Measure::ChannelCount;
        const std::size_t numberOfBins = measure == Measure::Bucket ? 512 :
                                         measure == Measure::ChannelCount ? 273 : 1000;
        result.bins.assign(numberOfBins, 0);
        result.minimum = measure == Measure::Bucket || measure == Measure::ChannelCount ? -0.5 : 0.0;
        result.maximum = measure == Measure::Bucket ? 511.5 : measure == Measure::ChannelCount ? 272.5 : 0.0;
        bool hasValues = false;
        std::uint64_t previousTime = 0;
        RootEvent event;
        const auto observe = [&](double value) {
            if (fixedRange) {
                hasValues = true;
            } else if (!hasValues) {
                result.minimum = result.maximum = value;
                hasValues = true;
            } else {
                result.minimum = std::min(result.minimum, value);
                result.maximum = std::max(result.maximum, value);
            }
        };
        for (std::int64_t entry = 0; entry < entries; ++entry) {
            if (cancelled && cancelled->load()) return false;
            if (!reader.Read(entry, &event, &readerError)) throw std::runtime_error(readerError);
            if (measure == Measure::EventTime) observe(EventTimeSeconds(event.eventTime));
            else if (measure == Measure::EventFrequency || measure == Measure::EventPeriod) {
                if (entry > 0) observe(MeasureEvent(measure, event, previousTime));
            } else if (measure == Measure::Bucket) {
                for (int aget = 0; aget < 4; ++aget)
                    for (int channel = 0; channel < 68; ++channel)
                        for (const RootSample &sample : event.channels[aget][channel]) observe(sample.bucket);
            } else observe(event.channelCount);
            previousTime = event.eventTime;
            UpdateProgress(progressPermille, 0, entry, entries);
        }
        if (!hasValues) throw std::runtime_error("ROOT tree has no values for the selected statistic");
        if (!fixedRange) {
            if (result.maximum <= result.minimum) {
                result.minimum -= 0.5;
                result.maximum += 0.5;
            } else {
                const double margin = (result.maximum - result.minimum) * 0.05;
                result.minimum -= margin;
                result.maximum += margin;
            }
        }

        previousTime = 0;
        for (std::int64_t entry = 0; entry < entries; ++entry) {
            if (cancelled && cancelled->load()) return false;
            if (!reader.Read(entry, &event, &readerError)) throw std::runtime_error(readerError);
            const auto add = [&](double value) {
                const int bin = HistogramBin(value, result.minimum, result.maximum, result.bins.size());
                if (bin >= 0) {
                    ++result.bins[static_cast<std::size_t>(bin)];
                    ++result.values;
                }
            };
            if (measure == Measure::EventTime) add(EventTimeSeconds(event.eventTime));
            else if (measure == Measure::EventFrequency || measure == Measure::EventPeriod) {
                if (entry > 0) add(MeasureEvent(measure, event, previousTime));
            } else if (measure == Measure::Bucket) {
                for (int aget = 0; aget < 4; ++aget)
                    for (int channel = 0; channel < 68; ++channel)
                        for (const RootSample &sample : event.channels[aget][channel]) add(sample.bucket);
            } else add(event.channelCount);
            previousTime = event.eventTime;
            UpdateProgress(progressPermille, 1, entry, entries);
        }
        if (cancelled && cancelled->load()) return false;
        *histogram = std::move(result);
        if (progressPermille) progressPermille->store(1000);
        return true;
    } catch (const std::exception &exception) {
        if (error) *error = exception.what();
        return false;
    }
}


}
}
