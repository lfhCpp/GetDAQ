#include "inspect_hitpattern.h"

#include "hitpattern.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>

namespace {
unsigned ParseId(const char *text, unsigned maximum, const char *name)
{
    if (!text || !*text) throw std::invalid_argument(std::string(name) + " must be decimal");
    errno = 0;
    char *end = 0;
    const unsigned long value = std::strtoul(text, &end, 10);
    if (errno || !end || *end || std::to_string(value) != text || value > maximum)
        throw std::invalid_argument(std::string(name) + " is outside its decimal range");
    return static_cast<unsigned>(value);
}

int CountHitPatterns(const char *path, const char *coboText,
                     const char *asadText, const char *agetText)
{
    try {
        const unsigned cobo = ParseId(coboText, 255, "CoBo ID");
        const unsigned asad = ParseId(asadText, 3, "AsAd ID");
        const unsigned aget = ParseId(agetText, 3, "AGET ID");
        std::ifstream input(path, std::ios::binary);
        if (!input) throw std::runtime_error("Cannot open input file");
        const GetDAQ::HitPatternCounts result = GetDAQ::CountHitPatternFrames(input, cobo, asad, aget);
        std::cout << "# kind=getdaq.cobo-hit-pattern-count\n"
                  << "# file=" << path << "\n"
                  << "# cobo=" << cobo << ",asad=" << asad << ",aget=" << aget << "\n"
                  << "# matching_frames=" << result.frames << "\n"
                  << "channel,hit_frames,frame_fraction\n";
        for (std::size_t channel = 0; channel < result.channelFrames.size(); ++channel) {
            const double fraction = static_cast<double>(result.channelFrames[channel]) /
                                    static_cast<double>(result.frames);
            std::cout << channel << ',' << result.channelFrames[channel] << ','
                      << std::fixed << std::setprecision(9) << fraction << '\n';
        }
        return 0;
    } catch (const std::exception &error) {
        std::cerr << path << ": cannot count CoBo hit-pattern headers: " << error.what() << '\n';
        return 1;
    }
}

int SummarizeHitPatternScan(int argc, char **argv)
{
    try {
        if (argc < 7 || (argc - 5) % 2 != 0)
            throw std::invalid_argument("Provide one or more THRESHOLD_CODE FILE pairs");
        const unsigned cobo = ParseId(argv[2], 255, "CoBo ID");
        const unsigned asad = ParseId(argv[3], 3, "AsAd ID");
        const unsigned aget = ParseId(argv[4], 3, "AGET ID");
        std::map<unsigned, GetDAQ::HitPatternCounts> points;
        unsigned previousCode = 0;
        bool havePrevious = false;
        for (int i = 5; i < argc; i += 2) {
            const unsigned code = ParseId(argv[i], 127, "threshold code");
            if (havePrevious && code < previousCode)
                throw std::invalid_argument("Threshold codes must be in ascending order; repeat a code for another file at that point");
            previousCode = code;
            havePrevious = true;
            std::ifstream input(argv[i + 1], std::ios::binary);
            if (!input) throw std::runtime_error(std::string("Cannot open input file: ") + argv[i + 1]);
            const GetDAQ::HitPatternCounts part = GetDAQ::CountHitPatternFrames(input, cobo, asad, aget);
            GetDAQ::MergeHitPatternCounts(&points[code], part);
        }
        if (points.empty()) throw std::invalid_argument("No threshold scan points were supplied");

        std::cout << "# kind=getdaq.threshold-scan-hit-pattern-counts\n"
                  << "# cobo=" << cobo << ",asad=" << asad << ",aget=" << aget << "\n"
                  << "# input_mapping=operator-supplied; configuration-to-file association was not verified\n"
                  << "# interpretation=header-bit counts only; no physical threshold conclusion\n"
                  << "threshold_code,channel,hit_frames,matching_frames,frame_fraction\n";
        for (const auto &point : points) {
            for (std::size_t channel = 0; channel < point.second.channelFrames.size(); ++channel) {
                const double fraction = static_cast<double>(point.second.channelFrames[channel]) /
                                        static_cast<double>(point.second.frames);
                std::cout << point.first << ',' << channel << ',' << point.second.channelFrames[channel]
                          << ',' << point.second.frames << ',' << std::fixed << std::setprecision(9)
                          << fraction << '\n';
            }
        }
        if (!std::cout) throw std::runtime_error("Cannot write hit-pattern scan summary");
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "Cannot summarize threshold scan hit-pattern counts: " << error.what() << '\n';
        return 1;
    }
}
}

int HandleHitPatternCommand(int argc, char **argv)
{
    if (argc == 6 && std::string(argv[1]) == "--hit-pattern")
        return CountHitPatterns(argv[2], argv[3], argv[4], argv[5]);
    if (argc >= 7 && std::string(argv[1]) == "--hit-pattern-scan")
        return SummarizeHitPatternScan(argc, argv);
    return -1;
}
