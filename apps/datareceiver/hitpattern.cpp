#include "hitpattern.h"

#include "mfmheader.h"
#include "mfmstream.h"

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace GetDAQ {
namespace {
const std::size_t kPatternOffset = 31;
const std::size_t kPatternBytes = 9;
const std::size_t kPatternHeaderEnd = kPatternOffset + 4 * kPatternBytes;
const std::size_t kMaximumFrameBytes = 32u * 1024u * 1024u;

std::size_t HeaderBytes(const std::uint8_t *frame, const FrameInfo &info)
{
    if (!frame || (frame[0] & 0x40u) != 0)
        throw std::runtime_error("CoBo hit-pattern requires a standard MFM header");
    const unsigned exponent = unsigned(frame[0] & 15u);
    const std::uint64_t base = MfmHeaderInteger(frame + 8, 2, info.littleEndian);
    if (exponent >= 32 || base > (std::uint64_t(1) << 32) >> exponent)
        throw std::runtime_error("MFM header length exceeds supported range");
    return static_cast<std::size_t>(base << exponent);
}

bool IsHit(const std::uint8_t *frame, const FrameInfo &info, unsigned aget,
           unsigned channel)
{
    const std::size_t base = kPatternOffset + std::size_t(aget) * kPatternBytes;
    const unsigned byteInPattern = channel / 8;
    const std::size_t byteOffset = info.littleEndian
        ? base + byteInPattern
        : base + (kPatternBytes - 1 - byteInPattern);
    const unsigned bit = channel % 8;
    return (frame[byteOffset] & (std::uint8_t(1u) << bit)) != 0;
}
}

HitPatternCounts CountHitPatternFrames(std::istream &input, unsigned cobo,
                                       unsigned asad, unsigned aget,
                                       bool requireMatch)
{
    if (cobo > 255 || asad > 3 || aget > 3)
        throw std::invalid_argument("CoBo ID must be 0..255; AsAd and AGET IDs must be 0..3");

    HitPatternCounts result{};
    MfmStream stream(kMaximumFrameBytes,
        [&](const std::uint8_t *frame, const FrameInfo &info) {
            if (!info.coboFrame || info.cobo != cobo || info.asad != asad) return;
            if (info.type != 1 || info.revision > 5)
                throw std::runtime_error("Selected CoBo frame is not supported type-1 revision 0..5");
            const std::size_t headerBytes = HeaderBytes(frame, info);
            if (headerBytes < kPatternHeaderEnd || headerBytes > info.bytes)
                throw std::runtime_error("CoBo header does not contain four complete 72-bit hit patterns");

            ++result.frames;
            const std::size_t pattern = kPatternOffset + std::size_t(aget) * kPatternBytes;
            if (pattern + kPatternBytes > headerBytes)
                throw std::runtime_error("Selected AGET hit-pattern exceeds the MFM header");
            for (unsigned channel = 0; channel < kAgetPhysicalChannels; ++channel)
                if (IsHit(frame, info, aget, channel)) ++result.channelFrames[channel];
        });

    std::vector<std::uint8_t> buffer(64u * 1024u);
    while (input) {
        input.read(reinterpret_cast<char *>(buffer.data()),
                   static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) stream.Feed(buffer.data(), static_cast<std::size_t>(count));
    }
    if (input.bad()) throw std::runtime_error("MFM input read failed");
    stream.Finish();
    if (requireMatch && !result.frames) throw std::runtime_error("No matching type-1 CoBo frames were found");
    return result;
}

void MergeHitPatternCounts(HitPatternCounts *total, const HitPatternCounts &part)
{
    if (!total) throw std::invalid_argument("Hit-pattern total must not be null");
    if (part.frames > UINT64_MAX - total->frames)
        throw std::overflow_error("Hit-pattern frame counter overflow");
    for (std::size_t channel = 0; channel < total->channelFrames.size(); ++channel)
        if (part.channelFrames[channel] > UINT64_MAX - total->channelFrames[channel])
            throw std::overflow_error("Hit-pattern channel counter overflow");
    total->frames += part.frames;
    for (std::size_t channel = 0; channel < total->channelFrames.size(); ++channel)
        total->channelFrames[channel] += part.channelFrames[channel];
}

} // namespace GetDAQ
