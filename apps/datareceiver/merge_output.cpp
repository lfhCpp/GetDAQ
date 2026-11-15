#include "merge_output_p.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
namespace MergeInternal {
namespace {
class OutputFile {
public:
    explicit OutputFile(int descriptor) : fDescriptor(descriptor) {}
    ~OutputFile() { if (fDescriptor >= 0) ::close(fDescriptor); }

    void Write(const void *data, std::size_t bytes, const char *failure)
    {
        const char *next = static_cast<const char *>(data);
        while (bytes) {
            const ssize_t written = ::write(fDescriptor, next, bytes);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw std::runtime_error(failure);
            next += written;
            bytes -= static_cast<std::size_t>(written);
        }
    }

    void Finish()
    {
        if (fDescriptor < 0) return;
        if (::fsync(fDescriptor) != 0) throw std::runtime_error("Output flush failed");
        const int descriptor = fDescriptor;
        fDescriptor = -1;
        if (::close(descriptor) != 0) throw std::runtime_error("Output close failed");
    }

private:
    int fDescriptor;
};

std::string PartName(const std::string &base, unsigned part)
{
    if (!part) return base;
    const std::string::size_type slash = base.find_last_of("/\\");
    const std::string::size_type dot = base.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash))
        return base + "." + std::to_string(part);
    return base.substr(0, dot) + "." + std::to_string(part) + base.substr(dot);
}

uint64_t Add(uint64_t left, uint64_t right)
{
    if (right > UINT64_MAX - left) throw std::runtime_error("Merged event size overflows 64 bits");
    return left + right;
}

uint64_t Align(uint64_t value, uint64_t alignment)
{
    if (value > std::numeric_limits<uint64_t>::max() - (alignment - 1))
        throw std::runtime_error("Merged event size overflows 64 bits");
    return ((value + alignment - 1) / alignment) * alignment;
}

uint8_t BlockExponent(uint64_t bytes)
{
    uint8_t exponent = 0;
    while ((bytes >> exponent) > 0xffffffu) {
        if (++exponent > 15) throw std::runtime_error("Merged event exceeds MFM maximum block size");
    }
    return exponent;
}

void Put16(uint8_t *p, uint16_t value) { p[0] = uint8_t(value >> 8); p[1] = uint8_t(value); }
void Put24(uint8_t *p, uint32_t value) { p[0] = uint8_t(value >> 16); p[1] = uint8_t(value >> 8); p[2] = uint8_t(value); }
void Put32(uint8_t *p, uint32_t value)
{
    p[0] = uint8_t(value >> 24); p[1] = uint8_t(value >> 16);
    p[2] = uint8_t(value >> 8); p[3] = uint8_t(value);
}

void CopyExact(std::ifstream &in, OutputFile &out, uint64_t offset, uint64_t bytes)
{
    in.clear();
    in.seekg(static_cast<std::streamoff>(offset), std::ios::beg);
    if (!in) throw std::runtime_error("Cannot seek input frame");
    char buffer[1024 * 1024];
    while (bytes) {
        const std::size_t amount = static_cast<std::size_t>(std::min<uint64_t>(bytes, sizeof(buffer)));
        in.read(buffer, static_cast<std::streamsize>(amount));
        if (static_cast<std::size_t>(in.gcount()) != amount)
            throw std::runtime_error("Input changed or became truncated during merge");
        out.Write(buffer, amount, "Output write failed");
        bytes -= amount;
    }
}

std::unique_ptr<OutputFile> CreateOutput(const std::string &base, unsigned &part,
                                         std::string *createdPath)
{
    while (part < 1000000) {
        const std::string candidate = PartName(base, part++);
        const int descriptor = ::open(candidate.c_str(), O_WRONLY | O_CREAT | O_EXCL |
                                      O_NOFOLLOW | O_CLOEXEC, 0666);
        if (descriptor >= 0) {
            std::unique_ptr<OutputFile> output;
            try {
                output.reset(new OutputFile(descriptor));
                *createdPath = candidate;
            } catch (...) {
                if (output) output.reset(); else ::close(descriptor);
                std::remove(candidate.c_str());
                throw;
            }
            return output;
        }
        if (errno == EEXIST) continue;
        throw std::runtime_error("Cannot create output: " + candidate);
    }
    throw std::runtime_error("Too many output parts");
}
} // namespace

struct OutputTransaction::Impl {
    Impl(const std::string &path, uint64_t limit, std::vector<std::string> *created)
        : basePath(path), maximumFileBytes(limit), paths(created) {}
    ~Impl()
    {
        output.reset();
        if (!committed) for (const std::string &path : *paths) std::remove(path.c_str());
    }

    std::string basePath;
    uint64_t maximumFileBytes;
    std::vector<std::string> *paths;
    std::unique_ptr<OutputFile> output;
    uint64_t outputBytes = 0;
    unsigned part = 0;
    bool committed = false;
};

OutputTransaction::OutputTransaction(const std::string &basePath, uint64_t maximumFileBytes,
                                     std::vector<std::string> *createdPaths)
    : fImpl()
{
    if (!createdPaths) throw std::invalid_argument("Merge output path list is required");
    fImpl.reset(new Impl(basePath, maximumFileBytes, createdPaths));
}
OutputTransaction::~OutputTransaction() = default;

void OutputTransaction::WriteEvent(const std::vector<std::unique_ptr<std::ifstream> > &streams,
                                   const std::vector<FrameRef> &frames,
                                   std::size_t begin, std::size_t end, uint32_t event)
{
    if (begin >= end || end > frames.size()) throw std::invalid_argument("Invalid merged event frame range");
    uint64_t innerBytes = 0;
    for (std::size_t i = begin; i < end; ++i) innerBytes = Add(innerBytes, frames[i].bytes);
    const uint64_t headerUnpadded = 20;
    const uint8_t exponent = BlockExponent(Add(headerUnpadded, innerBytes));
    const uint64_t block = uint64_t(1) << exponent;
    const uint64_t headerBytes = Align(headerUnpadded, block);
    const uint64_t totalBytes = Align(Add(headerBytes, innerBytes), block);

    if (fImpl->maximumFileBytes && fImpl->output && fImpl->outputBytes &&
        (totalBytes > UINT64_MAX - fImpl->outputBytes ||
         fImpl->outputBytes + totalBytes > fImpl->maximumFileBytes)) {
        fImpl->output->Finish(); fImpl->output.reset(); fImpl->outputBytes = 0;
    }
    if (!fImpl->output) {
        std::string name;
        fImpl->output = CreateOutput(fImpl->basePath, fImpl->part, &name);
        try { fImpl->paths->push_back(name); }
        catch (...) { fImpl->output.reset(); std::remove(name.c_str()); throw; }
    }

    uint8_t header[20] = {};
    header[0] = exponent;
    Put24(header + 1, static_cast<uint32_t>(totalBytes / block));
    header[4] = 0; Put16(header + 5, 0xff01); header[7] = 1;
    Put16(header + 8, static_cast<uint16_t>(headerBytes / block));
    Put16(header + 10, 0); Put32(header + 12, static_cast<uint32_t>(end - begin)); Put32(header + 16, event);
    fImpl->output->Write(header, sizeof(header), "Output header write failed");
    if (headerBytes > sizeof(header)) {
        std::vector<char> padding(static_cast<std::size_t>(headerBytes - sizeof(header)), 0);
        fImpl->output->Write(padding.data(), padding.size(), "Output padding write failed");
    }
    for (std::size_t i = begin; i < end; ++i) {
        if (frames[i].input >= streams.size() || !streams[frames[i].input])
            throw std::runtime_error("Merged frame refers to a missing input stream");
        CopyExact(*streams[frames[i].input], *fImpl->output, frames[i].offset, frames[i].bytes);
    }
    const uint64_t used = Add(headerBytes, innerBytes);
    if (totalBytes > used) {
        std::vector<char> padding(static_cast<std::size_t>(totalBytes - used), 0);
        fImpl->output->Write(padding.data(), padding.size(), "Output padding write failed");
    }
    fImpl->outputBytes = Add(fImpl->outputBytes, totalBytes);
}

void OutputTransaction::Finish()
{
    if (fImpl->output) { fImpl->output->Finish(); fImpl->output.reset(); }
}

void OutputTransaction::Commit()
{
    if (fImpl->output) throw std::logic_error("Merge output must be finished before commit");
    fImpl->committed = true;
}

} // namespace MergeInternal
} // namespace GetDAQ
