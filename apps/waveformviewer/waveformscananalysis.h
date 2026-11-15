#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMSCANANALYSIS_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMSCANANALYSIS_H

#include "waveformanalysis.h"

#include <QVector>

namespace GetDAQ {
struct PeakingScanInput {
    int pointIndex = -1;
    QString fileName;
};

struct PeakingScanSummary {
    int pointIndex = -1;
    quint64 framesSeen = 0;
    quint64 channelPresent = 0;
    quint64 validWaveforms = 0;
    quint64 invalidWaveforms = 0;
    quint64 channelMissing = 0;
    double meanPeakAdc = 0.0;
    double meanPeakingBuckets = 0.0;
    double meanFwhmBuckets = 0.0;
};

// Analyze operator-mapped raw files for the legacy 16-point GET peaking-time
// index. This is read-only offline analysis; it does not verify run binding.
QVector<PeakingScanSummary> AnalyzePeakingScan(const QVector<PeakingScanInput> &inputs,
                                               int cobo, int asad, int aget, int channel,
                                               QString *error);

struct PulserResponseInput {
    int channel = -1;
    int pulserMillivolts = 0;
    QString fileName;
};

struct PulserResponseSummary {
    int channel = -1;
    int pulserMillivolts = 0;
    quint64 framesSeen = 0;
    quint64 channelPresent = 0;
    quint64 validWaveforms = 0;
    quint64 invalidWaveforms = 0;
    quint64 channelMissing = 0;
    double meanPeakAdc = 0.0;
    double meanPeakingBuckets = 0.0;
    double meanFwhmBuckets = 0.0;
};

// Summarize operator-mapped pulser amplitudes by channel. This reports
// waveform response only; it does not infer gain or a pass/fail criterion.
QVector<PulserResponseSummary> AnalyzePulserResponse(const QVector<PulserResponseInput> &inputs,
                                                     int cobo, int asad, int aget,
                                                     QString *error);
}

#endif
