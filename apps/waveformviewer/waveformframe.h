#ifndef GETDAQ_WAVEFORMVIEWER_WAVEFORMFRAME_H
#define GETDAQ_WAVEFORMVIEWER_WAVEFORMFRAME_H

#include "grawreader.h"
#include "waveformcanvas.h"

namespace GetDAQ {

struct WaveformFrameData {
    QVector<WaveformPlotTrace> traces;
    QVector<GrawSample> metricSamples;
    bool integerAdcSamples = true;
};

bool ReadGrawWaveformFrame(const QString &path, const GrawFrameRecord &frame, int aget,
                           const QVector<int> &channels, WaveformFrameData *result, QString *error);
bool ReadGmatWaveformFrame(const QString &path, int frame, int aget, const QVector<int> &channels,
                           WaveformFrameData *result, QString *error);

}

#endif
