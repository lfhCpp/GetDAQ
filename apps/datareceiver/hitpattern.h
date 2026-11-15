#ifndef GETDAQ_HITPATTERN_H
#define GETDAQ_HITPATTERN_H

#include <array>
#include <cstdint>
#include <istream>

namespace GetDAQ {

static const std::size_t kAgetHitPatternBits = 72;
static const std::size_t kAgetPhysicalChannels = 68;

struct HitPatternCounts {
    std::uint64_t frames = 0;
    std::array<std::uint64_t, kAgetPhysicalChannels> channelFrames;
};

// Stream a saved MFM file and count asserted header hit-pattern bits for one
// CoBo/AsAd/AGET. No waveform samples are decoded or retained.
HitPatternCounts CountHitPatternFrames(std::istream &input, unsigned cobo,
                                       unsigned asad, unsigned aget,
                                       bool requireMatch = true);

// Combine independent files that belong to the same explicitly selected scan
// point. Counters are checked for uint64 overflow.
void MergeHitPatternCounts(HitPatternCounts *total, const HitPatternCounts &part);

} // namespace GetDAQ

#endif
