#ifndef GETDAQ_DATARECEIVER_MERGE_OUTPUT_P_H
#define GETDAQ_DATARECEIVER_MERGE_OUTPUT_P_H

#include "merge_internal.h"
#include <memory>
#include <string>
#include <vector>

namespace GetDAQ {
namespace MergeInternal {

// Owns atomic-no-replace output creation, MFM event encoding, file rotation,
// durability and rollback. The caller remains responsible for event ordering
// and selection.
class OutputTransaction {
public:
    OutputTransaction(const std::string &basePath, uint64_t maximumFileBytes,
                      std::vector<std::string> *createdPaths);
    ~OutputTransaction();
    OutputTransaction(const OutputTransaction &) = delete;
    OutputTransaction &operator=(const OutputTransaction &) = delete;

    void WriteEvent(const std::vector<std::unique_ptr<std::ifstream> > &streams,
                    const std::vector<FrameRef> &frames,
                    std::size_t begin, std::size_t end, uint32_t event);
    void Finish();
    void Commit();

private:
    struct Impl;
    std::unique_ptr<Impl> fImpl;
};

} // namespace MergeInternal
} // namespace GetDAQ

#endif
