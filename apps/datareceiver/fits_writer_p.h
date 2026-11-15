#ifndef GETDAQ_FITS_WRITER_P_H
#define GETDAQ_FITS_WRITER_P_H

#include "mfmstream.h"
#include <cstddef>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace GetDAQ {
namespace FitsInternal {

struct ChannelSamples {
    std::vector<unsigned short> buckets;
    std::vector<unsigned short> values;
};

struct EventData {
    FrameInfo info;
    std::map<std::pair<unsigned, unsigned>, ChannelSamples> channels;
    std::size_t samples = 0;
};

EventData DecodeEvent(const std::uint8_t *frame, const FrameInfo &info);

// Owns FITS serialization for one staged conversion. The caller controls the
// staging directory and publishes it only after Finish() succeeds.
class Writer {
public:
    Writer(const std::string &stagingDirectory, const std::string &baseName);
    ~Writer();

    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    const std::string &IndexPath() const;
    std::size_t IndexRows() const;
    void WriteEvent(const std::string &path, const EventData &event);
    void Finish();

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};

} // namespace FitsInternal
} // namespace GetDAQ

#endif
