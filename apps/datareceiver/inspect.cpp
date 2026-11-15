#include "mfmstream.h"
#include "inspect_hitpattern.h"
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

// 离线流式检查，不改原文件，不读取整份大文件，也不依赖 ROOT / Qt。
int main(int argc, char **argv)
{
    const int hitPatternResult = HandleHitPatternCommand(argc, argv);
    if (hitPatternResult >= 0) return hitPatternResult;
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "Usage: getdaq-inspect FILE.graw [FILE.graw ...]\n"
                     "       getdaq-inspect --hit-pattern FILE.graw COBO_ID ASAD_ID AGET_ID\n"
                     "       getdaq-inspect --hit-pattern-scan COBO_ID ASAD_ID AGET_ID THRESHOLD_CODE FILE.graw [THRESHOLD_CODE FILE.graw ...]\n"
                     "Count selected type-1 CoBo header hit bits for physical channels 0..67.\n"
                     "The scan form groups explicitly mapped files by ascending code; repeat a code for multiple files.\n"
                     "These are offline header-bit counts only, not threshold interpretation or DAQ control.\n";
        return 0;
    }
    if (argc < 2) { std::cerr << "Usage: getdaq-inspect FILE.graw [FILE.graw ...]\n"; return 2; }
    int result = 0;
    for (int i = 1; i < argc; ++i) {
        uint64_t bytes = 0, frames = 0, resets = 0;
        std::map<std::string, uint64_t> sources;
        std::map<std::string, uint32_t> events;
        try {
            std::ifstream input(argv[i], std::ios::binary);
            if (!input) throw std::runtime_error("Cannot open input file");
            GetDAQ::MfmStream stream(32 * 1024 * 1024, [&](const uint8_t *, const GetDAQ::FrameInfo &info) {
                const std::string key = info.coboFrame ? "CoBo" + std::to_string(info.cobo) + "/AsAd" + std::to_string(info.asad)
                    : "type" + std::to_string(info.type) + "/source" + std::to_string(info.source);
                if (!sources.count(key) && sources.size() >= 4096) throw std::runtime_error("Too many source identifiers");
                ++sources[key]; ++frames; bytes += info.bytes;
                if (info.hasEvent) {
                    if (events.count(key) && info.event <= events[key]) ++resets;
                    events[key] = info.event;
                }
            });
            char buffer[65536];
            while (input) { input.read(buffer, sizeof(buffer)); stream.Feed(reinterpret_cast<const uint8_t *>(buffer), std::size_t(input.gcount())); }
            if (input.bad()) throw std::runtime_error("Input read failed");
            stream.Finish();
            std::cout << argv[i] << ": VALID_STRUCTURE frames=" << frames << " bytes=" << bytes << " non_increasing_events=" << resets << '\n';
            for (const auto &source : sources) std::cout << "  " << source.first << " frames=" << source.second << '\n';
        } catch (const std::exception &e) {
            std::cerr << argv[i] << ": INVALID after_complete_bytes=" << bytes << " frames=" << frames << ": " << e.what() << '\n'; result = 1;
        }
    }
    return result;
}
