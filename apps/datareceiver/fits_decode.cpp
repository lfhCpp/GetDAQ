#include "fits_writer_p.h"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
namespace FitsInternal {
namespace {
std::uint32_t Header32(const std::uint8_t *data, bool little) {
    return static_cast<std::uint32_t>(MfmHeaderInteger(data, 4, little));
}
} // namespace

EventData DecodeEvent(const std::uint8_t *frame, const FrameInfo &info) {
    if (!info.coboFrame || info.revision > 5 ||
        (info.type != 1 && !(info.type == 2 && info.revision == 5)))
        throw std::runtime_error("FITS conversion supports CoBo type-1 revisions 0 through 5 and compact type-2 revision 5 only");

    const unsigned exponent = frame[0] & 15u;
    const std::uint64_t scale = std::uint64_t(1) << exponent;
    const std::uint64_t headerBytes = MfmHeaderInteger(frame + 8, 2, info.littleEndian) * scale;
    const std::uint64_t itemBytes = MfmHeaderInteger(frame + 10, 2, info.littleEndian);
    const std::uint64_t itemCount = MfmHeaderInteger(frame + 12, 4, info.littleEndian);
    const unsigned expectedItemBytes = info.type == 1 ? 4u : 2u;
    if (headerBytes < 28 || headerBytes > info.bytes || itemBytes != expectedItemBytes ||
        itemCount > (info.bytes - headerBytes) / itemBytes)
        throw std::runtime_error("CoBo frame has inconsistent FITS sample bounds");
    const std::uint64_t payloadEnd = headerBytes + itemCount * itemBytes;
    if (((payloadEnd + scale - 1) / scale) * scale != info.bytes)
        throw std::runtime_error("CoBo frame has invalid MFM block padding");

    EventData event;
    event.info = info;
    const std::uint8_t *item = frame + headerBytes;
    if (info.type == 1) {
        for (std::uint64_t i = 0; i < itemCount; ++i, item += 4) {
            const std::uint32_t word = Header32(item, false);
            const unsigned aget = (word >> 30) & 3u;
            const unsigned channel = (word >> 23) & 0x7fu;
            const unsigned bucket = (word >> 14) & 0x1ffu;
            const unsigned value = word & 0xfffu;
            if (channel >= 68 || bucket >= 512)
                throw std::runtime_error("CoBo sample has out-of-range AGET/channel/bucket fields");
            ChannelSamples &samples = event.channels[std::make_pair(channel, aget)];
            if (samples.values.size() >= 4096)
                throw std::runtime_error("CoBo channel exceeds the safe FITS row limit");
            samples.buckets.push_back(static_cast<unsigned short>(bucket));
            samples.values.push_back(static_cast<unsigned short>(value));
            ++event.samples;
        }
    } else {
        const std::uint64_t expectedSamples = std::uint64_t(4) * 68 * 512;
        if (itemCount != expectedSamples)
            throw std::runtime_error("Compact CoBo type-2 frame must contain a complete 4 x 68 x 512 readout");
        std::array<unsigned, 4> channelIndex = {{0, 0, 0, 0}};
        std::array<unsigned, 4> bucketIndex = {{0, 0, 0, 0}};
        std::array<unsigned, 4> counts = {{0, 0, 0, 0}};
        for (std::uint64_t i = 0; i < itemCount; ++i, item += 2) {
            const unsigned short word = static_cast<unsigned short>((unsigned(item[0]) << 8) | item[1]);
            const unsigned aget = (word >> 14) & 3u;
            if (channelIndex[aget] >= 68) {
                channelIndex[aget] = 0;
                ++bucketIndex[aget];
            }
            if (bucketIndex[aget] >= 512)
                throw std::runtime_error("Compact CoBo type-2 frame exceeds 512 buckets for an AGET");
            const unsigned channel = channelIndex[aget];
            const unsigned bucket = bucketIndex[aget];
            const unsigned value = word & 0xfffu;
            ChannelSamples &samples = event.channels[std::make_pair(channel, aget)];
            samples.buckets.push_back(static_cast<unsigned short>(bucket));
            samples.values.push_back(static_cast<unsigned short>(value));
            ++channelIndex[aget];
            ++counts[aget];
            ++event.samples;
        }
        for (unsigned aget = 0; aget < 4; ++aget)
            if (counts[aget] != 68u * 512u)
                throw std::runtime_error("Compact CoBo type-2 frame has an incomplete AGET readout");
    }
    return event;
}

} // namespace FitsInternal
} // namespace GetDAQ
