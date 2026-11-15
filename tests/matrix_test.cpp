#include "../apps/datareceiver/matrix.h"

#include <dirent.h>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include <unistd.h>

namespace {
void Check(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

void Put16(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint16_t value)
{
    bytes[offset] = std::uint8_t(value >> 8);
    bytes[offset + 1] = std::uint8_t(value);
}

void Put32(std::vector<std::uint8_t> &bytes, std::size_t offset, std::uint32_t value)
{
    bytes[offset] = std::uint8_t(value >> 24);
    bytes[offset + 1] = std::uint8_t(value >> 16);
    bytes[offset + 2] = std::uint8_t(value >> 8);
    bytes[offset + 3] = std::uint8_t(value);
}

std::uint16_t Value(std::size_t aget, std::size_t channel, std::size_t bucket, std::uint16_t base)
{
    return std::uint16_t((base + aget * 700 + channel * 7 + bucket % 7) & 0xfff);
}

std::vector<std::uint8_t> Cobo(std::uint16_t type, std::uint8_t revision, std::uint32_t items,
                               std::uint16_t base)
{
    const unsigned exponent = revision == 0 ? 5 : revision < 5 ? 6 : 8;
    const std::size_t blockBytes = std::size_t(1) << exponent;
    const std::size_t headerUnits = revision == 5 ? 1 : 2;
    const std::size_t headerBytes = blockBytes * headerUnits;
    const std::size_t payloadBytes = type == 1 ? std::size_t(items) * 4 :
                                     type == 2 ? std::size_t(items) * 2 : 0;
    const std::size_t frameBytes = ((headerBytes + payloadBytes + blockBytes - 1) / blockBytes) * blockBytes;
    const std::size_t blockCount = frameBytes / blockBytes;
    std::vector<std::uint8_t> frame(frameBytes, 0);
    frame[0] = std::uint8_t(exponent);
    frame[1] = std::uint8_t(blockCount >> 16);
    frame[2] = std::uint8_t(blockCount >> 8);
    frame[3] = std::uint8_t(blockCount);
    frame[4] = 0;
    Put16(frame, 5, type);
    frame[7] = revision;
    Put16(frame, 8, std::uint16_t(headerUnits));
    Put16(frame, 10, type == 1 ? 4 : 2);
    Put32(frame, 12, items);
    frame[26] = 0;
    frame[27] = 0;
    if (type == 1) {
        std::size_t item = headerBytes;
        for (std::size_t aget = 0; aget < 4; ++aget) {
            for (std::size_t channel = 0; channel < 68; ++channel) {
                for (std::size_t sequence = 0; sequence < 512 && item < frame.size(); ++sequence) {
                    const std::size_t bucket = (sequence + 100) % 512;
                    const std::uint32_t word = (std::uint32_t(aget) << 30) |
                                               (std::uint32_t(channel) << 23) |
                                               (std::uint32_t(bucket) << 14) |
                                               Value(aget, channel, bucket, base);
                    Put32(frame, item, word);
                    item += 4;
                }
            }
        }
    }
    return frame;
}

void Write(const std::string &path, const std::vector<std::uint8_t> &bytes)
{
    std::ofstream output(path.c_str(), std::ios::binary);
    output.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
    if (!output) throw std::runtime_error("cannot write synthetic CoBo input");
}

std::vector<std::uint8_t> Read(const std::string &path)
{
    std::ifstream input(path.c_str(), std::ios::binary);
    return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
}

std::uint64_t DoubleBits(const std::vector<std::uint8_t> &bytes, std::size_t sample)
{
    std::uint64_t bits = 0;
    for (unsigned i = 0; i < 8; ++i) bits |= std::uint64_t(bytes[sample * 8 + i]) << (i * 8);
    return bits;
}

std::uint64_t ExpectedBits(std::uint16_t sample)
{
    const double value = sample;
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

void ExpectFailure(const GetDAQ::MatrixOptions &options, const char *message)
{
    bool failed = false;
    try { GetDAQ::ConvertGrawToMatrix(options); }
    catch (const std::exception &) { failed = true; }
    Check(failed, message);
    Check(::access(options.output.c_str(), F_OK) != 0, "failed conversion published an output");
}

void Cleanup(const std::string &directory)
{
    DIR *stream = ::opendir(directory.c_str());
    if (!stream) return;
    while (dirent *entry = ::readdir(stream)) {
        if (!std::strcmp(entry->d_name, ".") || !std::strcmp(entry->d_name, "..")) continue;
        ::unlink((directory + "/" + entry->d_name).c_str());
    }
    ::closedir(stream);
    ::rmdir(directory.c_str());
}
}

int main()
{
    const char *temporaryRoot = std::getenv("TMPDIR");
    if (temporaryRoot && *temporaryRoot && ::access(temporaryRoot, W_OK | X_OK) != 0) temporaryRoot = 0;
    const std::string pattern = std::string(temporaryRoot && *temporaryRoot ? temporaryRoot : "/tmp") + "/getdaq-matrix-test-XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end()); name.push_back('\0');
    if (!::mkdtemp(name.data())) { std::cerr << "cannot create matrix test directory\n"; return 1; }
    const std::string directory(name.data());
    try {
        const std::string input = directory + "/synthetic.graw";
        const std::string output = directory + "/all.gmat";
        const std::string selected = directory + "/selected.gmat";
        std::vector<std::uint8_t> first;
        const std::uint16_t bases[] = {3, 5, 7, 11, 15, 19};
        for (std::uint8_t revision = 0; revision <= 5; ++revision) {
            const std::vector<std::uint8_t> frame = Cobo(1, revision, 4 * 68 * 512, bases[revision]);
            first.insert(first.end(), frame.begin(), frame.end());
        }
        Write(input, first);

        GetDAQ::MatrixOptions options; options.input = input; options.output = output;
        GetDAQ::MatrixResult result = GetDAQ::ConvertGrawToMatrix(options);
        Check(result.coboFramesSeen == 6 && result.matricesWritten == 6, "unexpected matrix frame counts");
        const std::size_t matrixBytes = 4u * 68u * 512u * 8u;
        Check(result.outputBytes == 6u * matrixBytes, "wrong .gmat matrix byte layout");
        const std::vector<std::uint8_t> bytes = Read(output);
        Check(bytes.size() == result.outputBytes, "published output size differs from receipt");
        const std::size_t samples = 4u * 68u * 512u;
        Check(DoubleBits(bytes, 0) == ExpectedBits(Value(0, 0, 100, 3)), "first sample arrival order/value is wrong");
        Check(DoubleBits(bytes, 511) == ExpectedBits(Value(0, 0, 99, 3)), "per-channel arrival order was not preserved");
        Check(DoubleBits(bytes, 512) == ExpectedBits(Value(0, 1, 100, 3)), "channel-major matrix ordering is wrong");
        Check(DoubleBits(bytes, 5u * samples) == ExpectedBits(Value(0, 0, 100, 19)), "sixth matrix did not follow the first five contiguously");
        Check(DoubleBits(bytes, 5u * samples + 68u * 512u) == ExpectedBits(Value(1, 0, 100, 19)),
              "AGET-major matrix ordering is wrong");
        bool existingRejected = false;
        try { GetDAQ::ConvertGrawToMatrix(options); } catch (const std::exception &) { existingRejected = true; }
        Check(existingRejected && Read(output) == bytes, "existing output was replaced");

        options.output = selected; options.firstCoboFrame = 5; options.maximumCoboFrames = 1;
        result = GetDAQ::ConvertGrawToMatrix(options);
        Check(result.coboFramesSeen == 6 && result.matricesWritten == 1 && result.outputBytes == matrixBytes,
              "start/count selection did not convert one requested frame");
        const std::vector<std::uint8_t> selectedBytes = Read(selected);
        Check(selectedBytes.size() == matrixBytes && DoubleBits(selectedBytes, 0) == ExpectedBits(Value(0, 0, 100, 19)),
              "selected frame output is not the second input matrix");

        GetDAQ::MatrixOptions limited; limited.input = input; limited.output = directory + "/limited.gmat";
        limited.maximumOutputBytes = matrixBytes * 6 - 1;
        ExpectFailure(limited, "output byte cap was not enforced");

        const std::string incompleteInput = directory + "/incomplete.graw";
        std::vector<std::uint8_t> incomplete = Cobo(1, 5, 4 * 68 * 512 - 1, 0);
        Write(incompleteInput, incomplete);
        GetDAQ::MatrixOptions invalid; invalid.input = incompleteInput; invalid.output = directory + "/incomplete.gmat";
        ExpectFailure(invalid, "incomplete CoBo waveform was accepted");

        const std::string compactInput = directory + "/compact.graw";
        Write(compactInput, Cobo(2, 5, 1, 0));
        invalid.input = compactInput; invalid.output = directory + "/compact.gmat";
        ExpectFailure(invalid, "compact type-2 frame was accepted as a four-AGET matrix");

        const std::string newerInput = directory + "/newer-revision.graw";
        Write(newerInput, Cobo(1, 6, 4 * 68 * 512, 0));
        invalid.input = newerInput; invalid.output = directory + "/newer-revision.gmat";
        ExpectFailure(invalid, "unsupported CoBo revision was silently skipped");

        Cleanup(directory);
        std::cout << "PASS gmat_layout IEEE754_LE ordering start_count no_replace byte_cap incomplete_compact_rejection "
                     "unsupported_revision_rejection synthetic_raw_bytes=" << first.size() << " raw_removed=true\n";
        return 0;
    } catch (const std::exception &error) {
        Cleanup(directory);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
