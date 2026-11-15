#include "grawreader_internal.h"
#include "../datareceiver/mfmheader.h"
#include <QFile>
#include <stdexcept>

namespace GetDAQ {
namespace {
using namespace GrawReaderInternal;
struct ParsedFrame {
    GrawFrameRecord record;
    quint64 count = 0;
};

bool ReadAt(QFile &file, qint64 offset, char *destination, qint64 bytes)
{
    if (offset < 0 || bytes < 0 || !file.seek(offset)) return false;
    return file.read(destination, bytes) == bytes;
}

ParsedFrame ParseFrame(QFile &file, qint64 offset, qint64 enclosingEnd)
{
    char primary[8];
    if (offset < 0 || offset > enclosingEnd - 8 || !ReadAt(file, offset, primary, sizeof(primary)))
        throw std::runtime_error("truncated MFM primary header");
    const std::size_t frameBytes = MfmFrameBytes(reinterpret_cast<const std::uint8_t *>(primary), sizeof(primary));
    if (frameBytes < 16 || frameBytes > kMaximumFrameBytes ||
        frameBytes > quint64(enclosingEnd - offset))
        throw std::runtime_error("MFM frame length is invalid or exceeds the 1 GiB safety limit");
    char prefix[28] = {};
    const qint64 prefixBytes = qMin<qint64>(sizeof(prefix), qint64(frameBytes));
    if (!ReadAt(file, offset, prefix, prefixBytes)) throw std::runtime_error("truncated MFM frame header");
    const FrameInfo info = InspectMfmHeader(reinterpret_cast<const std::uint8_t *>(prefix),
                                            std::size_t(prefixBytes), frameBytes);
    ParsedFrame parsed;
    parsed.record.offset = offset;
    parsed.record.bytes = quint32(frameBytes);
    parsed.record.type = quint16(info.type);
    parsed.record.revision = quint8(info.revision);
    parsed.record.cobo = quint8(info.cobo);
    parsed.record.asad = quint8(info.asad);
    parsed.record.event = quint32(info.event);
    parsed.record.littleEndian = info.littleEndian;
    if (info.coboFrame && frameBytes > kMaximumCoboFrameBytes)
        throw std::runtime_error("CoBo frame exceeds the 4 MiB waveform safety limit");
    if ((static_cast<unsigned char>(prefix[0]) & 0x40u) != 0) return parsed;
    const std::uint64_t scale = std::uint64_t(1) << (static_cast<unsigned char>(prefix[0]) & 15u);
    parsed.record.headerBytes = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(prefix + 8), 2, info.littleEndian) * scale;
    parsed.record.itemBytes = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(prefix + 10), 2, info.littleEndian);
    parsed.count = MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(prefix + 12), 4, info.littleEndian);
    parsed.record.itemCount = parsed.count;
    return parsed;
}

void IndexNested(QFile &file, qint64 offset, qint64 enclosingEnd, int depth,
                 QVector<GrawFrameRecord> *frames, const std::atomic<bool> *cancel)
{
    if (cancel && cancel->load()) throw std::runtime_error("MFM frame index cancelled");
    if (depth > kMaximumLayerDepth) throw std::runtime_error("layered MFM nesting exceeds eight levels");
    const ParsedFrame parsed = ParseFrame(file, offset, enclosingEnd);
    const qint64 frameEnd = offset + parsed.record.bytes;
    if (parsed.record.type == 0xff01u && parsed.record.revision == 1) {
        if (parsed.record.itemBytes != 0)
            throw std::runtime_error("layered MFM frame has an invalid item size");
        qint64 child = offset + qint64(parsed.record.headerBytes);
        for (quint64 i = 0; i < parsed.count; ++i) {
            const ParsedFrame childFrame = ParseFrame(file, child, frameEnd);
            if (childFrame.record.bytes == 0) throw std::runtime_error("layered frame contains an invalid child");
            IndexNested(file, child, frameEnd, depth + 1, frames, cancel);
            child += childFrame.record.bytes;
        }
        char primary[1];
        if (!ReadAt(file, offset, primary, sizeof(primary))) throw std::runtime_error("cannot reread layered MFM header");
        const quint64 blockBytes = std::uint64_t(1) << (static_cast<unsigned char>(primary[0]) & 15u);
        if (child > frameEnd || quint64(frameEnd - child) >= blockBytes)
            throw std::runtime_error("layered frame payload size does not match its child count");
        return;
    }
    if (parsed.record.type == 1 && parsed.record.revision <= 5) {
        if (frames->size() >= kMaximumIndexedFrames)
            throw std::runtime_error("file contains more than two million CoBo frames; index memory limit reached");
        frames->append(parsed.record);
    } else if (parsed.record.type == 2 && parsed.record.revision == 5) {
        if (frames->size() >= kMaximumIndexedFrames)
            throw std::runtime_error("file contains more than two million CoBo frames; index memory limit reached");
        frames->append(parsed.record);
    }
}

}

GrawIndexResult GrawFrameReader::BuildIndex(const QString &path, const std::atomic<bool> *cancel)
{
    GrawIndexResult result;
    try {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(file.errorString().toStdString());
        result.inputBytes = file.size();
        if (result.inputBytes < 8) throw std::runtime_error("file is too small to contain an MFM frame");
        qint64 offset = 0;
        while (offset < result.inputBytes) {
            if (cancel && cancel->load()) throw std::runtime_error("MFM frame index cancelled");
            const ParsedFrame top = ParseFrame(file, offset, result.inputBytes);
            if (!top.record.bytes) throw std::runtime_error("MFM frame has zero length");
            IndexNested(file, offset, result.inputBytes, 0, &result.frames, cancel);
            offset += top.record.bytes;
        }
        if (offset != result.inputBytes) throw std::runtime_error("file ends in a partial MFM frame");
        if (result.frames.isEmpty()) throw std::runtime_error("no supported CoBo type-1 or compact type-2 frames were found");
    } catch (const std::exception &exception) {
        result.frames.clear();
        result.error = QString::fromLocal8Bit(exception.what());
    }
    return result;
}


}
