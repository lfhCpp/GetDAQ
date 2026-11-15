#include "matrix_codec_p.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <stdexcept>

namespace GetDAQ {
namespace MatrixInternal {
namespace {
const std::size_t kAgets = 4;
const std::size_t kChannels = 68;
const std::size_t kBuckets = 512;
const std::size_t kSamplesPerMatrix = kAgets * kChannels * kBuckets;
const std::size_t kBytesPerMatrix = kSamplesPerMatrix * sizeof(double);
const std::size_t kMaximumFrameBytes = 4u * 1024u * 1024u;
}

std::size_t SampleCount() { return kSamplesPerMatrix; }
std::size_t MatrixBytes() { return kBytesPerMatrix; }
std::size_t MaximumFrameBytes() { return kMaximumFrameBytes; }

void DecodeMatrix(const std::uint8_t *frame, const FrameInfo &info,
                  std::vector<std::uint16_t> &samples)
{
    if (info.type != 1 || info.revision > 5)
        throw std::runtime_error("only CoBo type 1 revisions 0 through 5 can form a complete four-AGET matrix");
    const unsigned exponent = frame[0] & 15u;
    const std::uint64_t scale = std::uint64_t(1) << exponent;
    const std::uint64_t headerBytes = MfmHeaderInteger(frame + 8, 2, info.littleEndian) * scale;
    const std::uint64_t itemBytes = MfmHeaderInteger(frame + 10, 2, info.littleEndian);
    const std::uint64_t itemCount = MfmHeaderInteger(frame + 12, 4, info.littleEndian);
    if (headerBytes < 28 || headerBytes > info.bytes || itemBytes != 4 ||
        itemCount > (info.bytes - headerBytes) / itemBytes)
        throw std::runtime_error("CoBo type 1 frame has unsupported or inconsistent item layout");
    const std::uint64_t payloadEnd = headerBytes + itemCount * itemBytes;
    const std::uint64_t paddedFrameBytes = ((payloadEnd + scale - 1) / scale) * scale;
    if (paddedFrameBytes != info.bytes)
        throw std::runtime_error("CoBo type 1 frame has invalid MFM block padding");

    std::fill(samples.begin(), samples.end(), 0);
    std::vector<std::uint8_t> present(kSamplesPerMatrix, 0);
    std::vector<std::size_t> channelCounts(kAgets * kChannels, 0);
    const std::uint8_t *item = frame + headerBytes;
    for (std::uint64_t i = 0; i < itemCount; ++i, item += 4) {
        const std::uint32_t word = std::uint32_t(MfmHeaderInteger(item, 4, false));
        const std::size_t aget = (word >> 30) & 3u;
        const std::size_t channel = (word >> 23) & 0x7fu;
        const std::size_t bucket = (word >> 14) & 0x1ffu;
        const std::uint16_t value = std::uint16_t(word & 0xfffu);
        if (channel >= kChannels || bucket >= kBuckets)
            throw std::runtime_error("CoBo sample has an out-of-range channel or bucket index");
        const std::size_t index = (aget * kChannels + channel) * kBuckets + bucket;
        if (present[index]) throw std::runtime_error("CoBo frame contains a duplicate AGET/channel/bucket sample");
        present[index] = 1;
        const std::size_t channelIndex = aget * kChannels + channel;
        const std::size_t arrivalIndex = channelCounts[channelIndex]++;
        if (arrivalIndex >= kBuckets) throw std::runtime_error("CoBo channel contains too many samples");
        // The legacy converter wrote Channel::data() in item-arrival order.
        // The viewer then labels those values as bucket 0..511 when reading .gmat.
        samples[channelIndex * kBuckets + arrivalIndex] = value;
    }
    if (itemCount != kSamplesPerMatrix ||
        std::find(present.begin(), present.end(), std::uint8_t(0)) != present.end() ||
        std::find_if(channelCounts.begin(), channelCounts.end(), [](std::size_t count) { return count != kBuckets; }) != channelCounts.end())
        throw std::runtime_error("CoBo frame is incomplete: every AGET/channel must contain 512 samples");
}

void EncodeMatrix(const std::vector<std::uint16_t> &samples, std::vector<std::uint8_t> &bytes)
{
    if (sizeof(double) != 8 || !std::numeric_limits<double>::is_iec559)
        throw std::runtime_error("host does not provide IEEE-754 binary64 required by the legacy .gmat format");
    bytes.resize(kBytesPerMatrix);
    for (std::size_t i = 0; i < samples.size(); ++i) {
        const double value = static_cast<double>(samples[i]);
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        for (unsigned byte = 0; byte < 8; ++byte)
            bytes[i * 8 + byte] = std::uint8_t(bits >> (byte * 8));
    }
}

} // namespace MatrixInternal
} // namespace GetDAQ
