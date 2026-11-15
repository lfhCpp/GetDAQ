#ifndef GETDAQ_MERGE_H
#define GETDAQ_MERGE_H

#include <cstdint>
#include <string>
#include <vector>

namespace GetDAQ {
struct MergeOptions {
    std::string output;
    uint32_t minimumEvent = 0;
    uint32_t maximumEvent = 0xffffffffu;
    uint64_t maximumEvents = 0;
    uint64_t maximumFileBytes = 1024ull * 1024ull * 1024ull;
};

struct MergeResult {
    uint64_t inputFrames = 0;
    uint64_t mergedFrames = 0;
    uint64_t events = 0;
    std::vector<std::string> outputs;
};

MergeResult MergeMfmFiles(const std::vector<std::string> &inputs, const MergeOptions &options);
}

#endif
