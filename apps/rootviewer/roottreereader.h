#ifndef GETDAQ_ROOTTREEREADER_H
#define GETDAQ_ROOTTREEREADER_H

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace GetDAQ {
struct RootSample {
    std::uint16_t bucket = 0;
    std::uint16_t value = 0;
};

struct RootEvent {
    enum class Format { GDataFrame, T2KMatrix };
    Format format = Format::GDataFrame;
    std::uint32_t event = 0;
    std::uint64_t eventTime = 0;
    unsigned cobo = 0;
    unsigned asad = 0;
    unsigned channelCount = 0;
    unsigned sampleCount = 0;
    std::vector<RootSample> channels[4][68];
};

class RootTreeReader {
public:
    RootTreeReader();
    ~RootTreeReader();
    RootTreeReader(RootTreeReader &&other) noexcept;
    RootTreeReader &operator=(RootTreeReader &&other) noexcept;
    RootTreeReader(const RootTreeReader &) = delete;
    RootTreeReader &operator=(const RootTreeReader &) = delete;

    bool Open(const std::string &path, std::string *error);
    bool Read(std::int64_t entry, RootEvent *event, std::string *error);
    std::int64_t Entries() const;
    std::string Path() const;

private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}

#endif
