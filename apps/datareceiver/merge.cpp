#include "merge.h"
#include "merge_internal.h"
#include "merge_output_p.h"

#include <algorithm>
#include <memory>
#include <stdexcept>

namespace GetDAQ {
using MergeInternal::FrameRef;

MergeResult MergeMfmFiles(const std::vector<std::string> &inputs, const MergeOptions &options)
{
    if (inputs.empty() || options.output.empty()) throw std::invalid_argument("At least one input and an output path are required");
    if (options.minimumEvent > options.maximumEvent) throw std::invalid_argument("Minimum event ID exceeds maximum event ID");
    std::vector<std::unique_ptr<std::ifstream> > streams;
    std::vector<FrameRef> refs;
    MergeResult result;
    MergeInternal::IndexMfmInputs(inputs, options.output, &streams, &refs);
    result.inputFrames = refs.size();
    if (refs.empty()) throw std::runtime_error("No supported CoBo/RIBF frames found (supported types: 0x01, 0x02, 0x30)");
    std::sort(refs.begin(), refs.end(), [](const FrameRef &a, const FrameRef &b) {
        if (a.event != b.event) return a.event < b.event;
        if (a.input != b.input) return a.input < b.input;
        return a.offset < b.offset;
    });

    MergeInternal::OutputTransaction output(options.output, options.maximumFileBytes, &result.outputs);
    uint64_t mergedEventCount = 0;
    for (std::size_t begin = 0; begin < refs.size();) {
        const uint32_t event = refs[begin].event;
        std::size_t end = begin + 1;
        while (end < refs.size() && refs[end].event == event) ++end;
        if (event >= options.minimumEvent && event <= options.maximumEvent &&
            (!options.maximumEvents || mergedEventCount < options.maximumEvents)) {
            output.WriteEvent(streams, refs, begin, end, event);
            result.mergedFrames += end - begin;
            ++result.events;
            ++mergedEventCount;
        }
        begin = end;
        if (options.maximumEvents && mergedEventCount >= options.maximumEvents) break;
    }
    output.Finish();
    output.Commit();
    return result;
}
}
