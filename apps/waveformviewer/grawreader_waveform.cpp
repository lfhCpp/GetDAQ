#include "grawreader_decode_p.h"
#include "grawreader_internal.h"
#include "../datareceiver/mfmheader.h"
#include <QFile>
#include <stdexcept>

namespace GetDAQ {
using namespace GrawReaderInternal;

bool GrawFrameReader::ReadChannels(const QByteArray &input, const GrawFrameRecord &frame,
                                   int aget, const QVector<int> &channels,
                                   QVector<GrawChannelSamples> *traces, QString *error)
{
    if (!traces) {
        if (error) *error = QStringLiteral("Invalid waveform selection");
        return false;
    }
    traces->clear();
    if (aget < 0 || aget >= 4 || channels.isEmpty()) {
        if (error) *error = QStringLiteral("Invalid waveform selection");
        return false;
    }
    bool requested[kChannels] = {};
    for (int channel : channels) {
        if (channel < 0 || channel >= int(kChannels) || requested[channel]) {
            if (error) *error = QStringLiteral("Invalid or duplicate waveform channel selection");
            return false;
        }
        requested[channel] = true;
    }
    if(frame.offset<0 || frame.bytes>kMaximumCoboFrameBytes || frame.offset>input.size()-frame.bytes){if(error)*error="Invalid memory frame bounds";return false;}
    const auto bytes=input.mid(frame.offset,frame.bytes);
    try {
        const FrameInfo info = InspectMfmHeader(reinterpret_cast<const std::uint8_t *>(bytes.constData()),
                                                std::size_t(bytes.size()), frame.bytes);
        const quint64 scale = std::uint64_t(1) << (static_cast<unsigned char>(bytes.at(0)) & 15u);
        const quint64 headerBytes = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(bytes.constData() + 8), 2, info.littleEndian) * scale;
        const quint64 itemBytes = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(bytes.constData() + 10), 2, info.littleEndian);
        const quint64 itemCount = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(bytes.constData() + 12), 4, info.littleEndian);
        if (!info.coboFrame || info.type != frame.type || info.revision != frame.revision ||
            info.event != frame.event || info.cobo != frame.cobo || info.asad != frame.asad ||
            headerBytes != frame.headerBytes || itemBytes != frame.itemBytes || itemCount != frame.itemCount)
            throw std::runtime_error("MFM frame header changed after indexing");
    } catch (const std::exception &exception) {
        if (error) *error = QString::fromLocal8Bit(exception.what());
        return false;
    }
    if (frame.type == 1) return GrawReaderInternal::DecodeType1(bytes, frame, aget, channels, traces, error);
    if (frame.type == 2 && frame.revision == 5) return GrawReaderInternal::DecodeType2(bytes, frame, aget, channels, traces, error);
    if (error) *error = QStringLiteral("Unsupported CoBo frame type or revision");
    return false;
}

bool GrawFrameReader::ReadChannels(const QString &path,const GrawFrameRecord &frame,int aget,const QVector<int> &channels,QVector<GrawChannelSamples> *traces,QString *error){
 QFile file(path);if(!file.open(QIODevice::ReadOnly)){if(error)*error=file.errorString();return false;}
 if(frame.offset<0 || frame.bytes>kMaximumCoboFrameBytes || frame.offset>file.size()-frame.bytes || !file.seek(frame.offset)){if(error)*error="Indexed frame no longer fits file";return false;}
 auto bytes=file.read(frame.bytes);auto local=frame;local.offset=0;return ReadChannels(bytes,local,aget,channels,traces,error);
}
bool GrawFrameReader::ReadChannel(const QString &path, const GrawFrameRecord &frame,
                                  int aget, int channel, QVector<GrawSample> *samples,
                                  QString *error)
{
    if (!samples) {
        if (error) *error = QStringLiteral("Invalid waveform selection");
        return false;
    }
    QVector<GrawChannelSamples> traces;
    if (!ReadChannels(path, frame, aget, QVector<int>() << channel, &traces, error)) return false;
    if (traces.size() != 1 || traces.first().channel != channel) {
        if (error) *error = QStringLiteral("Selected AGET/channel has no samples in this frame");
        return false;
    }
    *samples = traces.first().samples;
    return true;
}
}
