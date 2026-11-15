#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMANALYSIS_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMANALYSIS_H

#include "grawreader.h"

namespace GetDAQ {
struct WaveformAnalysisResult {
    bool valid = false;
    quint16 peakBucket = 0;
    quint16 peakValue = 0;
    quint16 peakingBuckets = 0;
    quint16 fwhmBuckets = 0;
    QString error;
};

// Discrete bucket-domain equivalent of the legacy CoBoFrameViewer calculation.
// Thresholds are fractions of (peak - baseline); no interpolation is performed.
WaveformAnalysisResult AnalyzeWaveform(const QVector<GrawSample> &samples,
                                       double baseline = 0.0,
                                       double peakingThreshold = 0.05,
                                       double fwhmThreshold = 0.5);
}

#endif
