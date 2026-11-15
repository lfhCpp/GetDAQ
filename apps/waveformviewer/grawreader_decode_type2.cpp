#include "grawreader_decode_p.h"
#include "grawreader_internal.h"
#include <algorithm>

namespace GetDAQ {
namespace GrawReaderInternal {
namespace {
quint16 ReadBigEndian16(const char *p)
{
    const unsigned char *b = reinterpret_cast<const unsigned char *>(p);
    return quint16((quint16(b[0]) << 8) | quint16(b[1]));
}
}

bool DecodeType2(const QByteArray &bytes, const GrawFrameRecord &frame, int aget,
                 const QVector<int> &channels, QVector<GrawChannelSamples> *traces,
                 QString *error)
{
    if (frame.itemBytes != 2 || frame.itemCount != kAgets * kChannels * kBuckets ||
        frame.headerBytes > quint64(bytes.size()) || frame.itemCount > (frame.bytes - frame.headerBytes) / 2) {
        if (error) *error = QStringLiteral("Compact CoBo type-2 frame is incomplete or has an invalid layout");
        return false;
    }
    quint16 channelIndex[4] = {0, 0, 0, 0};
    quint16 bucketIndex[4] = {0, 0, 0, 0};
    quint16 counts[4] = {0, 0, 0, 0};
    bool found[4][68][512] = {};
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
    for (quint64 i = 0; i < frame.itemCount; ++i, item += 2) {
        const quint16 word = ReadBigEndian16(item);
        const unsigned itemAget = (word >> 14) & 3u;
        if (channelIndex[itemAget] >= kChannels) {
            channelIndex[itemAget] = 0;
            ++bucketIndex[itemAget];
        }
        if (bucketIndex[itemAget] >= kBuckets) {
            if (error) *error = QStringLiteral("Compact CoBo frame exceeds 512 buckets for one AGET");
            return false;
        }
        const int itemChannel = channelIndex[itemAget];
        const int bucket = bucketIndex[itemAget];
        if (found[itemAget][itemChannel][bucket]) {
            if (error) *error = QStringLiteral("Compact CoBo frame repeats a sample position");
            return false;
        }
        found[itemAget][itemChannel][bucket] = true;
        ++counts[itemAget];
        if (itemAget == unsigned(aget) && traceIndex[itemChannel] >= 0) {
            GrawSample sample; sample.bucket = quint16(bucket); sample.value = quint16(word & 0xfffu);
            (*traces)[traceIndex[itemChannel]].samples.append(sample);
        }
        ++channelIndex[itemAget];
    }
    if (counts[0] != kChannels * kBuckets || counts[1] != kChannels * kBuckets ||
        counts[2] != kChannels * kBuckets || counts[3] != kChannels * kBuckets) {
        if (error) *error = QStringLiteral("Compact CoBo frame does not contain a complete selected AGET readout");
        return false;
    }
    for (const GrawChannelSamples &trace : *traces) {
        if (trace.samples.size() != int(kBuckets)) {
            if (error) *error = QStringLiteral("Compact CoBo frame does not contain a complete selected channel");
            return false;
        }
    }
    return true;

}
}
}
