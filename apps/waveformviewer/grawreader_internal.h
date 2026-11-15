#ifndef GETDAQ_GRAWREADER_INTERNAL_H
#define GETDAQ_GRAWREADER_INTERNAL_H

#include "grawreader.h"

namespace GetDAQ {
namespace GrawReaderInternal {

static const quint64 kMaximumFrameBytes = 1024ULL * 1024ULL * 1024ULL;
static const quint64 kMaximumCoboFrameBytes = 4ULL * 1024ULL * 1024ULL;
static const int kMaximumIndexedFrames = 2000000;
static const int kMaximumLayerDepth = 8;
static const quint64 kAgets = 4;
static const quint64 kChannels = 68;
static const quint64 kBuckets = 512;

} // namespace GrawReaderInternal
} // namespace GetDAQ

#endif
