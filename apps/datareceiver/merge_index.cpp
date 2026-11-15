#include "merge_internal.h"
#include "mfmheader.h"

#include <algorithm>
#include <limits.h>
#include <stdexcept>
#include <cstdlib>

namespace GetDAQ {
namespace MergeInternal {
namespace {
uint32_t ReadEvent(const uint8_t *p, bool little)
{
    return uint32_t(MfmHeaderInteger(p, 4, little));
}

std::string Canonical(const std::string &path)
{
    char resolved[PATH_MAX];
    if (!realpath(path.c_str(), resolved)) return path;
    return resolved;
}
}

void IndexMfmInputs(const std::vector<std::string> &inputs,
                    const std::string &output,
                    std::vector<std::unique_ptr<std::ifstream> > *streams,
                    std::vector<FrameRef> *frames)
{
    if (!streams || !frames) throw std::invalid_argument("MFM input index output is required");
    streams->clear();
    frames->clear();
    const std::string outputCanonical = Canonical(output);
    for (std::size_t fi = 0; fi < inputs.size(); ++fi) {
        if (Canonical(inputs[fi]) == outputCanonical) throw std::invalid_argument("Output path refers to an input file");
        streams->emplace_back(new std::ifstream(inputs[fi].c_str(), std::ios::binary));
        std::ifstream &in = *streams->back();
        if (!in) throw std::runtime_error("Cannot open input: " + inputs[fi]);
        for (;;) {
            const std::streamoff at = in.tellg();
            uint8_t prefix[28];
            in.read(reinterpret_cast<char *>(prefix), 8);
            if (in.gcount() == 0 && in.eof()) break;
            if (in.gcount() != 8) throw std::runtime_error("Truncated MFM primary header in " + inputs[fi]);
            const uint64_t frameBytes64 = MfmHeaderInteger(prefix + 1, 3, (prefix[0] & 0x80) != 0) * (uint64_t(1) << (prefix[0] & 15));
            if (frameBytes64 < 8 || frameBytes64 > 1024ull * 1024ull * 1024ull) throw std::runtime_error("Invalid or over-1-GiB MFM frame in " + inputs[fi]);
            const std::size_t frameBytes = static_cast<std::size_t>(frameBytes64);
            const std::size_t need = std::min<std::size_t>(frameBytes, sizeof(prefix));
            if (need > 8) {
                in.read(reinterpret_cast<char *>(prefix + 8), static_cast<std::streamsize>(need - 8));
                if (static_cast<std::size_t>(in.gcount()) != need - 8) throw std::runtime_error("Truncated MFM header in " + inputs[fi]);
            }
            const FrameInfo info = InspectMfmHeader(prefix, need, frameBytes);
            bool include = false;
            uint32_t event = 0;
            if (info.coboFrame) { include = true; event = info.event; }
            else if (info.type == 0x30) {
                if (need < 18) throw std::runtime_error("RIBF frame header is too short");
                include = true; event = ReadEvent(prefix + 14, info.littleEndian);
            }
            if (include) {
                if (frames->size() >= 20000000u) throw std::runtime_error("Input contains over 20 million indexable frames; refusing excessive index memory use");
                frames->push_back(FrameRef{event, static_cast<uint32_t>(fi), static_cast<uint64_t>(at), static_cast<uint32_t>(frameBytes)});
            }
            in.clear();
            in.seekg(at + static_cast<std::streamoff>(frameBytes), std::ios::beg);
            if (!in) throw std::runtime_error("MFM frame extends beyond end of input: " + inputs[fi]);
        }
    }
}

} // namespace MergeInternal
} // namespace GetDAQ
