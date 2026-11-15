#ifndef GETDAQ_FITS_H
#define GETDAQ_FITS_H
#include <cstdint>
#include <string>
namespace GetDAQ {
struct FitsOptions {
    std::string input;
    std::string outputDirectory;
    std::string baseName;
    std::uint64_t firstCoboFrame = 0;
    std::uint64_t maximumCoboFrames = 0;
    std::uint64_t maximumOutputBytes = 20ULL * 1024ULL * 1024ULL * 1024ULL;
};
struct FitsResult {
    std::uint64_t coboFramesSeen = 0;
    std::uint64_t eventsWritten = 0;
    std::uint64_t channelsWritten = 0;
    std::uint64_t outputBytes = 0;
};
FitsResult ConvertGrawToFits(const FitsOptions &options);
}
#endif
