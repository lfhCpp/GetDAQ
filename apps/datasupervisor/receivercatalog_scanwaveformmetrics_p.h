#ifndef GETDAQ_RECEIVERCATALOG_SCANWAVEFORMMETRICS_P_H
#define GETDAQ_RECEIVERCATALOG_SCANWAVEFORMMETRICS_P_H

#include "mfmheader.h"

#include <cstdint>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {

struct BoundScanWaveformMetrics {
    std::uint64_t frames;
    std::uint64_t channelFrames;
    std::uint64_t missing;
    std::uint64_t invalid;
    std::uint64_t valid;
    double meanPeak;
    double meanPeaking;
    double meanFwhm;

    BoundScanWaveformMetrics();
};

class BoundScanWaveformAccumulator {
public:
    BoundScanWaveformAccumulator(unsigned int coboNumber,unsigned int asad,
                                 unsigned int aget,unsigned int channel);

    void ProcessFrame(const std::uint8_t *frame,const FrameInfo &info);
    const BoundScanWaveformMetrics &Metrics() const;

private:
    void ProcessFrameAtDepth(const std::uint8_t *frame,const FrameInfo &info,unsigned int depth);

    unsigned int fCoboNumber;
    unsigned int fAsad;
    unsigned int fAget;
    unsigned int fChannel;
    BoundScanWaveformMetrics fMetrics;
};

}
}

#endif
