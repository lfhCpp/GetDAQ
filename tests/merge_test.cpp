#include "../apps/datareceiver/merge.h"
#include "../apps/datareceiver/mfmheader.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

namespace {
void Check(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
void Write16(std::vector<uint8_t> &b, std::size_t at, uint16_t v) { b[at] = uint8_t(v >> 8); b[at + 1] = uint8_t(v); }
std::vector<uint8_t> Cobo(uint16_t type, uint32_t event, uint8_t marker)
{
    std::vector<uint8_t> b(28, 0);
    b[3] = 28; Write16(b, 5, type); b[7] = type == 2 ? 5 : 1;
    Write16(b, 8, 28); b[22] = uint8_t(event >> 24); b[23] = uint8_t(event >> 16);
    b[24] = uint8_t(event >> 8); b[25] = uint8_t(event); b[26] = 1; b[27] = 0;
    b[21] = marker;
    return b;
}
void Write(const std::string &path, const std::vector<uint8_t> &bytes)
{
    std::ofstream out(path.c_str(), std::ios::binary);
    out.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
    if (!out) throw std::runtime_error("cannot write test input");
}
void Append(std::vector<uint8_t> &to, const std::vector<uint8_t> &from) { to.insert(to.end(), from.begin(), from.end()); }
uint32_t U32(const uint8_t *p) { return uint32_t(GetDAQ::MfmHeaderInteger(p, 4, false)); }
}

int main()
{
    const char *temporaryRoot = std::getenv("TMPDIR");
    // Validation runners may export a private TMPDIR that was removed during
    // cleanup; fall back to the platform temporary directory in that case.
    if (temporaryRoot && *temporaryRoot && ::access(temporaryRoot, W_OK | X_OK) != 0)
        temporaryRoot = 0;
    const std::string pattern = std::string(temporaryRoot && *temporaryRoot ? temporaryRoot : "/tmp") + "/getdaq-merge-test-XXXXXX";
    std::vector<char> directory(pattern.begin(), pattern.end()); directory.push_back('\0');
    if (!mkdtemp(directory.data())) {
        std::cerr << "cannot create merge test directory under " << (temporaryRoot && *temporaryRoot ? temporaryRoot : "/tmp") << '\n';
        return 1;
    }
    const std::string dir(directory.data()), in1 = dir + "/a.graw", in2 = dir + "/b.graw", out = dir + "/merged.gmer";
    const std::string truncated = dir + "/truncated.graw", rejectedOutput = dir + "/rejected.gmer";
    try {
        const std::vector<uint8_t> a = Cobo(1, 5, 0xa5), b = Cobo(1, 2, 0xb2), c = Cobo(2, 5, 0xc5), ignored = Cobo(3, 1, 0x33);
        std::vector<uint8_t> file1; Append(file1, a); Append(file1, b);
        std::vector<uint8_t> file2; Append(file2, c); Append(file2, ignored);
        Write(in1, file1); Write(in2, file2);
        GetDAQ::MergeOptions options; options.output = out; options.maximumFileBytes = 0;
        const GetDAQ::MergeResult result = GetDAQ::MergeMfmFiles(std::vector<std::string>{in1, in2}, options);
        Check(result.inputFrames == 3 && result.mergedFrames == 3 && result.events == 2 && result.outputs.size() == 1, "unexpected merge counts");
        std::ifstream merged(out.c_str(), std::ios::binary);
        std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(merged)), std::istreambuf_iterator<char>());
        std::size_t pos = 0;
        Check(bytes.size() == 20 + 28 + 20 + 56, "unexpected merged output size");
        Check(GetDAQ::MfmFrameBytes(bytes.data(), bytes.size()) == 48, "first event length mismatch");
        Check(GetDAQ::InspectMfmHeader(bytes.data(), 20, 48).type == 0xff01, "output is not MergedByEventId format");
        Check(U32(bytes.data() + 12) == 1 && U32(bytes.data() + 16) == 2, "first event header mismatch");
        pos = 48;
        Check(GetDAQ::MfmFrameBytes(bytes.data() + pos, bytes.size() - pos) == 76, "second event length mismatch");
        Check(U32(bytes.data() + pos + 12) == 2 && U32(bytes.data() + pos + 16) == 5, "second event header mismatch");
        Check(bytes[pos + 20 + 21] == 0xa5 && bytes[pos + 48 + 21] == 0xc5, "same-event frame insertion order/content changed");
        bool rejected = false;
        try { GetDAQ::MergeMfmFiles(std::vector<std::string>{in1}, options); }
        catch (const std::exception &) { rejected = true; }
        Check(!rejected, "existing output prevented safe numbered-part selection");
        std::ifstream original(out.c_str(), std::ios::binary);
        std::vector<uint8_t> unchanged((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
        Check(unchanged == bytes, "existing output was overwritten");
        rejected = false;
        GetDAQ::MergeOptions aliasOptions; aliasOptions.output = in1;
        try { GetDAQ::MergeMfmFiles(std::vector<std::string>{in1}, aliasOptions); }
        catch (const std::invalid_argument &) { rejected = true; }
        Check(rejected, "input/output path alias was not rejected during indexing");
        const std::vector<uint8_t> shortHeader = {0, 0, 0, 0};
        Write(truncated, shortHeader);
        GetDAQ::MergeOptions badOptions; badOptions.output = rejectedOutput;
        rejected = false;
        try { GetDAQ::MergeMfmFiles(std::vector<std::string>{truncated}, badOptions); }
        catch (const std::runtime_error &) { rejected = true; }
        Check(rejected && ::access(rejectedOutput.c_str(), F_OK) != 0,
              "truncated input was not rejected before creating output");
        const std::string raceOutput = dir + "/race.gmer";
        const pid_t first = ::fork();
        Check(first >= 0, "cannot start first concurrent merge test process");
        if (first == 0) {
            try {
                GetDAQ::MergeOptions raceOptions; raceOptions.output = raceOutput; raceOptions.maximumFileBytes = 0;
                GetDAQ::MergeMfmFiles(std::vector<std::string>{in1, in2}, raceOptions);
                ::_exit(0);
            } catch (...) { ::_exit(1); }
        }
        const pid_t second = ::fork();
        if (second < 0) { ::waitpid(first, 0, 0); throw std::runtime_error("cannot start second concurrent merge test process"); }
        if (second == 0) {
            try {
                GetDAQ::MergeOptions raceOptions; raceOptions.output = raceOutput; raceOptions.maximumFileBytes = 0;
                GetDAQ::MergeMfmFiles(std::vector<std::string>{in1, in2}, raceOptions);
                ::_exit(0);
            } catch (...) { ::_exit(1); }
        }
        int firstStatus = 0, secondStatus = 0;
        Check(::waitpid(first, &firstStatus, 0) == first && ::waitpid(second, &secondStatus, 0) == second &&
              WIFEXITED(firstStatus) && WEXITSTATUS(firstStatus) == 0 &&
              WIFEXITED(secondStatus) && WEXITSTATUS(secondStatus) == 0,
              "concurrent merge process failed");
        const std::string racePart = dir + "/race.1.gmer";
        Check(::access(raceOutput.c_str(), F_OK) == 0 && ::access(racePart.c_str(), F_OK) == 0,
              "concurrent merges did not reserve two distinct output paths");
        std::ifstream raceFirst(raceOutput.c_str(), std::ios::binary);
        std::ifstream raceSecond(racePart.c_str(), std::ios::binary);
        const std::vector<uint8_t> raceFirstBytes((std::istreambuf_iterator<char>(raceFirst)), std::istreambuf_iterator<char>());
        const std::vector<uint8_t> raceSecondBytes((std::istreambuf_iterator<char>(raceSecond)), std::istreambuf_iterator<char>());
        Check(!raceFirstBytes.empty() && raceFirstBytes == raceSecondBytes,
              "concurrent output reservation overwrote or changed merged data");
        unlink((dir + "/merged.1.gmer").c_str());
        unlink(raceOutput.c_str()); unlink(racePart.c_str());
        unlink(in1.c_str()); unlink(in2.c_str()); unlink(out.c_str()); unlink(truncated.c_str()); rmdir(dir.c_str());
        std::cout << "PASS event_sort same_event_grouping input_order_preserved unsupported_type_skipped "
                  << "embedded_bytes_preserved mfm_outer_header existing_output_not_overwritten "
                  << "input_output_alias_rejected truncated_header_rejected_before_output "
                  << "concurrent_output_paths_exclusive "
                  << "temporary_inputs_removed synthetic_raw_bytes=112\n";
        return 0;
    } catch (const std::exception &error) {
        unlink((dir + "/merged.1.gmer").c_str()); unlink(in1.c_str()); unlink(in2.c_str()); unlink(out.c_str());
        unlink(truncated.c_str()); unlink(rejectedOutput.c_str());
        unlink((dir + "/race.gmer").c_str()); unlink((dir + "/race.1.gmer").c_str()); rmdir(dir.c_str());
        std::cerr << error.what() << '\n'; return 1;
    }
}
