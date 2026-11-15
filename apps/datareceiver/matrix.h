#ifndef GETDAQ_MATRIX_H
#define GETDAQ_MATRIX_H
#include <cstdint>
#include <string>

namespace GetDAQ {
struct MatrixOptions {
    std::string input;
    std::string output;
    std::uint64_t firstCoboFrame = 0; // Skip this many recognized CoBo frames.
    std::uint64_t maximumCoboFrames = 0; // 0 means all remaining CoBo frames.
    std::uint64_t maximumOutputBytes = 20ull * 1024ull * 1024ull * 1024ull;
};
struct MatrixResult {
    std::uint64_t coboFramesSeen = 0;
    std::uint64_t matricesWritten = 0;
    std::uint64_t outputBytes = 0;
};

// Convert complete CoBo waveform frames into the .gmat layout consumed by
// the legacy CoBoFrameViewer: AGET, channel, bucket, IEEE-754 binary64 LE.
MatrixResult ConvertGrawToMatrix(const MatrixOptions &options);
}
#endif
