#include "matrix.h"

#include "mfmstream.h"
#include "matrix_codec_p.h"

#include <cerrno>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace {
struct SelectedCountReached {};

class OutputTransaction {
public:
    explicit OutputTransaction(const std::string &destination)
        : fDestination(destination), fTemporary(destination + ".tmp.XXXXXX"), fFd(-1)
    {
        if (destination.empty()) throw std::invalid_argument("output path is empty");
        struct stat existing;
        if (::lstat(destination.c_str(), &existing) == 0)
            throw std::runtime_error("matrix output path already exists: " + destination);
        if (errno != ENOENT) throw Error("cannot inspect matrix output path");
        std::vector<char> name(fTemporary.begin(), fTemporary.end());
        name.push_back('\0');
        fFd = ::mkstemp(name.data());
        if (fFd < 0) throw Error("cannot create temporary matrix output");
        fTemporary.assign(name.data());
        if (::fchmod(fFd, 0644) != 0) {
            const int error = errno;
            ::close(fFd); fFd = -1; ::unlink(fTemporary.c_str());
            throw Error("cannot set matrix output permissions", error);
        }
    }

    ~OutputTransaction()
    {
        if (fFd >= 0) ::close(fFd);
        if (!fTemporary.empty()) ::unlink(fTemporary.c_str());
    }

    void Write(const std::uint8_t *data, std::size_t bytes)
    {
        while (bytes) {
            const ssize_t written = ::write(fFd, data, bytes);
            if (written < 0 && errno == EINTR) continue;
            if (written <= 0) throw Error("cannot write matrix output");
            data += written;
            bytes -= std::size_t(written);
        }
    }

    void Publish()
    {
        if (::fsync(fFd) != 0) throw Error("cannot sync matrix output");
        const int closeError = ::close(fFd) == 0 ? 0 : errno;
        fFd = -1;
        if (closeError) throw Error("cannot close matrix output", closeError);
        if (::link(fTemporary.c_str(), fDestination.c_str()) != 0)
            throw Error("cannot publish matrix output without replacing an existing path");
        if (::unlink(fTemporary.c_str()) != 0) throw Error("cannot remove temporary matrix output");
        fTemporary.clear();
        const std::string parent = Parent(fDestination);
        const int directory = ::open(parent.c_str(), O_RDONLY);
        if (directory < 0) throw Error("cannot open matrix output directory for synchronization");
        const int syncError = ::fsync(directory) == 0 ? 0 : errno;
        ::close(directory);
        if (syncError) throw Error("cannot sync matrix output directory", syncError);
    }

private:
    static std::string Parent(const std::string &path)
    {
        const std::string::size_type slash = path.find_last_of('/');
        if (slash == std::string::npos) return ".";
        if (slash == 0) return "/";
        return path.substr(0, slash);
    }

    class Error : public std::runtime_error {
    public:
        Error(const std::string &what, int error = errno)
            : std::runtime_error(what + ": " + std::strerror(error)) {}
    };

    std::string fDestination;
    std::string fTemporary;
    int fFd;
};

}

MatrixResult ConvertGrawToMatrix(const MatrixOptions &options)
{
    if (options.input.empty()) throw std::invalid_argument("input path is empty");
    OutputTransaction output(options.output);
    MatrixResult result;
    std::vector<std::uint16_t> samples(MatrixInternal::SampleCount());
    std::vector<std::uint8_t> matrix;
    bool reachedCount = false;
    MfmStream stream(MatrixInternal::MaximumFrameBytes(), [&](const std::uint8_t *frame, const FrameInfo &info) {
        if (!info.coboFrame && info.type != 1 && info.type != 2) return;
        const std::uint64_t index = result.coboFramesSeen++;
        if (index < options.firstCoboFrame) return;
        if (!info.coboFrame)
            throw std::runtime_error("selected CoBo frame revision is not supported by the MFM source parser");
        if (result.outputBytes > options.maximumOutputBytes ||
            MatrixInternal::MatrixBytes() > options.maximumOutputBytes - result.outputBytes)
            throw std::runtime_error("matrix output exceeds configured maximum byte limit");
        MatrixInternal::DecodeMatrix(frame, info, samples);
        MatrixInternal::EncodeMatrix(samples, matrix);
        output.Write(matrix.data(), matrix.size());
        ++result.matricesWritten;
        result.outputBytes += matrix.size();
        if (options.maximumCoboFrames && result.matricesWritten == options.maximumCoboFrames)
            throw SelectedCountReached();
    });

    std::ifstream input(options.input.c_str(), std::ios::binary);
    if (!input) throw std::runtime_error("cannot open input file: " + options.input);
    std::vector<std::uint8_t> block(64u * 1024u);
    while (!reachedCount && input) {
        input.read(reinterpret_cast<char *>(block.data()), std::streamsize(block.size()));
        const std::streamsize count = input.gcount();
        if (count > 0) {
            try { stream.Feed(block.data(), std::size_t(count)); }
            catch (const SelectedCountReached &) { reachedCount = true; }
        }
    }
    if (input.bad()) throw std::runtime_error("error while reading input file: " + options.input);
    if (!reachedCount) stream.Finish();
    input.clear();
    input.close();
    if (input.fail()) throw std::runtime_error("cannot close input file: " + options.input);
    if (!result.matricesWritten) throw std::runtime_error("no complete CoBo matrix was selected");
    output.Publish();
    return result;
}
}
