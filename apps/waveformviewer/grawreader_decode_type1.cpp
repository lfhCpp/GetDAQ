#include "grawreader_decode_p.h"
#include "grawreader_internal.h"
#include <algorithm>

namespace GetDAQ {
namespace GrawReaderInternal {
namespace {
quint32 ReadBigEndian32(const char *p)
{
    const unsigned char *b = reinterpret_cast<const unsigned char *>(p);
    return (quint32(b[0]) << 24) | (quint32(b[1]) << 16) | (quint32(b[2]) << 8) | quint32(b[3]);
}
}

bool DecodeType1(const QByteArray &bytes, const GrawFrameRecord &frame, int aget,
                 const QVector<int> &channels, QVector<GrawChannelSamples> *traces,
                 QString *error)
{
    if (frame.itemBytes != 4 || frame.headerBytes < 28 || frame.headerBytes > frame.bytes ||
        frame.itemCount > (frame.bytes - frame.headerBytes) / 4) {
        if (error) *error = QStringLiteral("CoBo type-1 item layout is inconsistent");
        return false;
    }
    bool found[kChannels][kBuckets] = {};
    int traceIndex[kChannels];
    std::fill(traceIndex, traceIndex + kChannels, -1);
    traces->clear();
    for (int channel : channels) {
        GrawChannelSamples trace;
        trace.channel = channel;
        traceIndex[channel] = traces->size();
        traces->append(trace);
    }
    const char *item = bytes.constData() + frame.headerBytes;
    for (quint64 i = 0; i < frame.itemCount; ++i, item += 4) {
        const quint32 word = ReadBigEndian32(item);
        const int itemAget = int((word >> 30) & 3u);
        const int itemChannel = int((word >> 23) & 0x7fu);
        const int bucket = int((word >> 14) & 0x1ffu);
        if (itemChannel >= int(kChannels)) {
            if (error) *error = QStringLiteral("CoBo sample contains an invalid channel index");
            return false;
        }
        if (itemAget != aget || traceIndex[itemChannel] < 0) continue;
        if (found[itemChannel][bucket]) {
            if (error) *error = QStringLiteral("Selected waveform contains a duplicate bucket");
            return false;
        }
        found[itemChannel][bucket] = true;
        GrawSample sample; sample.bucket = quint16(bucket); sample.value = quint16(word & 0xfffu);
        (*traces)[traceIndex[itemChannel]].samples.append(sample);
    }
    for (int i = traces->size() - 1; i >= 0; --i) {
        if (traces->at(i).samples.isEmpty()) traces->remove(i);
    }
    if (traces->isEmpty()) {
        if (error) *error = QStringLiteral("No selected channels have samples in this frame");
        return false;
    }
    return true;
}

}
}
