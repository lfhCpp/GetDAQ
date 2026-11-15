#include "rootconverter.h"
#include "cli_options.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Usage(std::ostream &out)
{
    out << "Usage: getdaq-root --input FILE.graw [--output FILE.root] [options]\n"
           "Convert supported CoBo events into the legacy CoBoFrameViewer ROOT object tree.\n"
           "ROOT is an optional offline-analysis dependency; this tool does not receive DAQ data.\n"
           "Options:\n"
           "  -i, --input FILE          MFM/CoBo raw input\n"
           "  -o, --output FILE         New ROOT output (default: input name with .root)\n"
           "      --config FILE         Embed configuration text in TTree UserInfo\n"
           "      --t2k                 Write legacy TT2K/T2KEvent layout\n"
           "  -s, --start N             Skip N MFM frames (default: 0)\n"
           "  -n, --count N             Process at most N MFM frames (default: all)\n"
           "      --max-output-bytes N  Output size cap (default: 21474836480, 20 GiB)\n"
           "  -h, --help                Show this help\n";
}

}

int main(int argc, char **argv)
{
    try {
        GetDAQ::RootOptions options;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "-h" || argument == "--help") {
                Usage(std::cout);
                return 0;
            }
            if (argument == "-i" || argument == "--input" ||
                argument.compare(0, 8, "--input=") == 0) {
                options.input = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "-o" || argument == "--output" ||
                       argument.compare(0, 9, "--output=") == 0) {
                options.output = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "--config" || argument.compare(0, 9, "--config=") == 0) {
                options.config = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "--t2k") {
                options.t2k = true;
            } else if (argument == "-s" || argument == "--start" ||
                       argument.compare(0, 8, "--start=") == 0) {
                options.firstFrame = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "start frame");
            } else if (argument == "-n" || argument == "--count" ||
                       argument.compare(0, 8, "--count=") == 0) {
                options.maximumFrames = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "frame count");
            } else if (argument == "--max-output-bytes" ||
                       argument.compare(0, 19, "--max-output-bytes=") == 0) {
                options.maximumOutputBytes = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "output byte limit");
            } else {
                throw std::invalid_argument("unknown option: " + argument);
            }
        }
        const GetDAQ::RootResult result = GetDAQ::ConvertGrawToRoot(options);
        std::cout << "Converted " << result.eventsWritten << " events from " << result.framesSeen
                  << " MFM frames (" << result.outputBytes << " bytes) into " << options.output << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "getdaq-root: " << error.what() << '\n';
        return 2;
    }
}
