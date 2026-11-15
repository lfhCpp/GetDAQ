#ifndef GETDAQ_DATARECEIVER_MERGE_INTERNAL_H
#define GETDAQ_DATARECEIVER_MERGE_INTERNAL_H

#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

namespace GetDAQ {
namespace MergeInternal {

struct FrameRef {
    uint32_t event;
    uint32_t input;
    uint64_t offset;
    uint32_t bytes;
};

void IndexMfmInputs(const std::vector<std::string> &inputs,
                    const std::string &output,
                    std::vector<std::unique_ptr<std::ifstream> > *streams,
                    std::vector<FrameRef> *frames);

} // namespace MergeInternal
} // namespace GetDAQ

#endif
