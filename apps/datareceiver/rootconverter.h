#ifndef GETDAQ_ROOTCONVERTER_H
#define GETDAQ_ROOTCONVERTER_H
#include <cstdint>
#include <string>
namespace GetDAQ {
struct RootOptions {
    std::string input, output, config;
    std::uint64_t firstFrame = 0;
    std::uint64_t maximumFrames = 0;
    std::uint64_t maximumOutputBytes = 20ull * 1024ull * 1024ull * 1024ull;
    bool t2k = false;
};
struct RootResult {
    std::uint64_t framesSeen = 0, eventsWritten = 0, outputBytes = 0;
    std::string output;
};
RootResult ConvertGrawToRoot(const RootOptions &options);
}
#endif
