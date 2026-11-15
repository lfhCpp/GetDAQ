#ifndef GETDAQ_WAVEFORMVIEWER_GRAWREADER_DECODE_P_H
#define GETDAQ_WAVEFORMVIEWER_GRAWREADER_DECODE_P_H

#include "grawreader.h"
#include <QByteArray>

namespace GetDAQ {
namespace GrawReaderInternal {
bool DecodeType1(const QByteArray &bytes, const GrawFrameRecord &frame, int aget,
                 const QVector<int> &channels, QVector<GrawChannelSamples> *traces,
                 QString *error);
bool DecodeType2(const QByteArray &bytes, const GrawFrameRecord &frame, int aget,
                 const QVector<int> &channels, QVector<GrawChannelSamples> *traces,
                 QString *error);
}
}

#endif
