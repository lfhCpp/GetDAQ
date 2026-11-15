#include "waveformframe.h"

#include <QFile>
#include <cmath>
#include <cstring>

namespace GetDAQ {
namespace {
const qint64 kSamplesPerMatrix = 4LL * 68LL * 512LL;
const qint64 kBytesPerMatrix = kSamplesPerMatrix * 8LL;
}

bool ReadGrawWaveformFrame(const QString &path, const GrawFrameRecord &frame, int aget,
                           const QVector<int> &channels, WaveformFrameData *result, QString *error)
{
    if (!result || !error) return false;
    WaveformFrameData decoded;
    QVector<GrawChannelSamples> samples;
    if (!GrawFrameReader::ReadChannels(path, frame, aget, channels, &samples, error)) {
        *error = "Cannot decode selected waveform: " + *error;
        return false;
    }
    for (const GrawChannelSamples &channel : samples) {
        WaveformPlotTrace trace;
        trace.channel = channel.channel;
        trace.samples.reserve(channel.samples.size());
        for (const GrawSample &sample : channel.samples)
            trace.samples.append(QPointF(sample.bucket, sample.value));
        if (channels.size() == 1) decoded.metricSamples = channel.samples;
        decoded.traces.append(trace);
    }
    *result = decoded;
    error->clear();
    return true;
}

bool ReadGmatWaveformFrame(const QString &path, int frame, int aget, const QVector<int> &channels,
                           WaveformFrameData *result, QString *error)
{
    if (!result || !error) return false;
    WaveformFrameData decoded;
    QFile input(path);
    if (!input.open(QIODevice::ReadOnly)) {
        *error = "Cannot open selected waveform file: " + input.errorString();
        return false;
    }
    bool integerAdcSamples = true;
    for (int channel : channels) {
        const qint64 offset = qint64(frame) * kBytesPerMatrix +
            (qint64(aget) * 68 + channel) * 512 * 8;
        if (!input.seek(offset)) {
            *error = QString("Cannot seek to selected waveform channel %1").arg(channel);
            return false;
        }
        const QByteArray bytes = input.read(512 * 8);
        if (bytes.size() != 512 * 8) {
            *error = QString("Selected waveform channel %1 is truncated").arg(channel);
            return false;
        }
        WaveformPlotTrace trace;
        trace.channel = channel;
        trace.samples.reserve(512);
        QVector<GrawSample> analysisSamples;
        if (channels.size() == 1) analysisSamples.reserve(512);
        for (int i = 0; i < 512; ++i) {
            quint64 bits = 0;
            for (int j = 0; j < 8; ++j)
                bits |= quint64(static_cast<unsigned char>(bytes.at(i * 8 + j))) << (j * 8);
            double value = 0.0;
            std::memcpy(&value, &bits, sizeof(bits));
            if (!std::isfinite(value)) {
                *error = QString("Selected waveform channel %1 contains a non-finite sample").arg(channel);
                return false;
            }
            trace.samples.append(QPointF(i, value));
            if (value < 0.0 || value > 4095.0 || std::floor(value) != value) {
                integerAdcSamples = false;
            } else if (channels.size() == 1) {
                GrawSample sample;
                sample.bucket = quint16(i);
                sample.value = quint16(value);
                analysisSamples.append(sample);
            }
        }
        if (channels.size() == 1) decoded.metricSamples = analysisSamples;
        decoded.traces.append(trace);
    }
    decoded.integerAdcSamples = integerAdcSamples;
    *result = decoded;
    error->clear();
    return true;
}

}
