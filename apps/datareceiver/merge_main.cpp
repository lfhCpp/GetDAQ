#include "merge.h"

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
uint64_t Number(const char *value, const char *option)
{
    char *end = 0;
    errno = 0;
    const unsigned long long parsed = std::strtoull(value, &end, 10);
    if (errno || !end || *end) throw std::invalid_argument(std::string("Invalid value for ") + option);
    return static_cast<uint64_t>(parsed);
}
void Usage()
{
    std::cerr << "Usage: getdaq-merge -o OUTPUT.gmer [options] INPUT.graw [INPUT.graw ...]\n"
              << "  --min-event ID       Include event IDs at or above ID (default 0)\n"
              << "  --max-event ID       Include event IDs at or below ID (default 4294967295)\n"
              << "  --max-events COUNT   Stop after COUNT merged events (0 means all)\n"
              << "  --max-file-mib MIB   Rotate before an event exceeds this output size (0 means unlimited; default 1024)\n";
}
}

int main(int argc, char **argv)
{
    try {
        GetDAQ::MergeOptions options;
        std::vector<std::string> inputs;
        for (int i = 1; i < argc; ++i) {
            const std::string arg(argv[i]);
            if (arg == "--help" || arg == "-h") { Usage(); return 0; }
            if (arg == "-o" || arg == "--output" || arg == "--min-event" || arg == "--max-event" ||
                arg == "--max-events" || arg == "--max-file-mib") {
                if (i + 1 >= argc) throw std::invalid_argument("Missing value after " + arg);
                const char *value = argv[++i];
                if (arg == "-o" || arg == "--output") options.output = value;
                else if (arg == "--min-event" || arg == "--max-event") {
                    const uint64_t event = Number(value, arg.c_str());
                    if (event > 0xffffffffu) throw std::invalid_argument("Event IDs must fit uint32");
                    if (arg == "--min-event") options.minimumEvent = static_cast<uint32_t>(event);
                    else options.maximumEvent = static_cast<uint32_t>(event);
                }
                else if (arg == "--max-events") options.maximumEvents = Number(value, "--max-events");
                else {
                    const uint64_t mib = Number(value, "--max-file-mib");
                    if (mib > UINT64_MAX / (1024ull * 1024ull)) throw std::invalid_argument("--max-file-mib overflows bytes");
                    options.maximumFileBytes = mib * 1024ull * 1024ull;
                }
            } else if (!arg.empty() && arg[0] == '-') throw std::invalid_argument("Unknown option: " + arg);
            else inputs.push_back(arg);
        }
        const GetDAQ::MergeResult result = GetDAQ::MergeMfmFiles(inputs, options);
        std::cout << "Indexed " << result.inputFrames << " CoBo/RIBF frame(s); merged " << result.mergedFrames
                  << " frame(s) into " << result.events << " event(s) across " << result.outputs.size() << " file(s).\n";
        for (const std::string &name : result.outputs) std::cout << "  " << name << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "getdaq-merge: " << error.what() << '\n';
        Usage();
        return 1;
    }
}
