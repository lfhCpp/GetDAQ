#include "waveformanalysis.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace GetDAQ {
namespace {
bool ValidThreshold(double value)
{
    return std::isfinite(value) && value >= 0.0 && value <= 1.0;
}
}

WaveformAnalysisResult AnalyzeWaveform(const QVector<GrawSample> &samples,
                                       double baseline,
                                       double peakingThreshold,
                                       double fwhmThreshold)
{
    WaveformAnalysisResult result;
    if (samples.isEmpty()) {
        result.error = QStringLiteral("No samples are available");
        return result;
    }
    if (!std::isfinite(baseline) || baseline < 0.0 || baseline > 4095.0 ||
        !ValidThreshold(peakingThreshold) || !ValidThreshold(fwhmThreshold)) {
        result.error = QStringLiteral("Baseline must be in 0..4095 and thresholds must be finite values in 0..1");
        return result;
    }

    std::vector<GrawSample> ordered;
    ordered.reserve(static_cast<std::size_t>(samples.size()));
    for (const GrawSample &sample : samples) {
        if (sample.bucket >= 512 || sample.value > 4095) {
            result.error = QStringLiteral("Sample bucket or ADC value is outside the CoBo waveform range");
            return result;
        }
        ordered.push_back(sample);
    }
    std::sort(ordered.begin(), ordered.end(), [](const GrawSample &left, const GrawSample &right) {
        return left.bucket < right.bucket;
    });
    for (std::size_t i = 1; i < ordered.size(); ++i) {
        if (ordered[i - 1].bucket == ordered[i].bucket) {
            result.error = QStringLiteral("Duplicate sample bucket cannot be analyzed");
            return result;
        }
    }

    const auto peak = std::max_element(ordered.begin(), ordered.end(), [](const GrawSample &left, const GrawSample &right) {
        return left.value < right.value;
    });
    const double excursion = double(peak->value) - baseline;
    if (!(excursion > 0.0)) {
        result.error = QStringLiteral("Waveform peak does not rise above the baseline");
        return result;
    }
    // The legacy algorithm binds its comparison to uint16_t; preserve its truncation of threshold levels.
    const quint16 peakingLevel = quint16(baseline + peakingThreshold * excursion);
    const quint16 halfMaximumLevel = quint16(baseline + fwhmThreshold * excursion);

    result.peakBucket = peak->bucket;
    result.peakValue = peak->value;
    bool foundPeaking = false;
    bool foundHalfMaximumLeft = false;
    for (auto sample = ordered.begin(); sample != peak; ++sample) {
        if (!foundPeaking && sample->value > peakingLevel) {
            const unsigned int width = unsigned(peak->bucket) - unsigned(sample->bucket);
            result.peakingBuckets = quint16(width);
            foundPeaking = true;
        }
        if (!foundHalfMaximumLeft && sample->value >= halfMaximumLevel)
            foundHalfMaximumLeft = true;
    }

    bool foundHalfMaximumRight = false;
    unsigned int fwhmWidth = 0;
    for (auto sample = peak + 1; sample != ordered.end(); ++sample) {
        if (sample->value < halfMaximumLevel) {
            if (foundHalfMaximumLeft) {
                fwhmWidth = unsigned(sample->bucket) - unsigned(peak->bucket);
                // Legacy width includes the distance from the left crossing to the first sample below half maximum.
                for (auto left = ordered.begin(); left != peak; ++left) {
                    if (left->value >= halfMaximumLevel) {
                        fwhmWidth = unsigned(sample->bucket) - unsigned(left->bucket);
                        break;
                    }
                }
                foundHalfMaximumRight = true;
            }
            break;
        }
    }
    if (!foundPeaking || !foundHalfMaximumLeft || !foundHalfMaximumRight) {
        result.error = QStringLiteral("Waveform does not cross the requested analysis thresholds on both sides");
        return result;
    }
    result.fwhmBuckets = quint16(fwhmWidth);
    result.valid = true;
    return result;
}
}
