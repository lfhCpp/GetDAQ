#ifndef GETDAQ_WAVEFORMVIEWER_GRAWREADER_H
#define GETDAQ_WAVEFORMVIEWER_GRAWREADER_H

#include <QVector>
#include <QByteArray>
#include <QString>

#include <array>
#include <atomic>
#include <cstdint>

namespace GetDAQ {
struct GrawFrameRecord {
    qint64 offset = 0;
    quint32 bytes = 0;
    quint64 headerBytes = 0;
    quint64 itemBytes = 0;
    quint64 itemCount = 0;
    quint16 type = 0;
    quint8 revision = 0;
    quint8 cobo = 0;
    quint8 asad = 0;
    quint32 event = 0;
    bool littleEndian = false;
};

struct GrawIndexResult {
    QVector<GrawFrameRecord> frames;
    qint64 inputBytes = 0;
    QString error;
};

struct GrawSample {
    quint16 bucket = 0;
    quint16 value = 0;
};

struct GrawChannelSamples {
    int channel = 0;
    QVector<GrawSample> samples;
};

class GrawFrameReader {
public:
    static GrawIndexResult BuildIndex(const QByteArray &bytes, const std::atomic<bool> *cancel = 0,int maximumFrames=2000000);
    static bool ReadChannels(const QByteArray &bytes,const GrawFrameRecord &frame,int aget,const QVector<int> &channels,QVector<GrawChannelSamples> *traces,QString *error);
    static GrawIndexResult BuildIndex(const QString &path, const std::atomic<bool> *cancel = 0);
    static bool ReadChannels(const QString &path, const GrawFrameRecord &frame,
                            int aget, const QVector<int> &channels,
                            QVector<GrawChannelSamples> *traces, QString *error);
    static bool ReadChannel(const QString &path, const GrawFrameRecord &frame,
                            int aget, int channel, QVector<GrawSample> *samples,
                            QString *error);
};
}

#endif
