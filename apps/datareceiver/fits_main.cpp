#include "fits.h"
#include "cli_options.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Usage(std::ostream &out)
{
    out << "Usage: getdaq-fits --input FILE.graw --output-dir NEW_DIRECTORY [options]\n"
           "Convert supported CoBo type-1 frames to per-event FITS channel tables and an index.\n"
           "The output directory must not exist; publication is atomic.\n"
           "Options:\n"
           "  -i, --input FILE          MFM/CoBo raw input\n"
           "  -o, --output-dir DIR      New output directory; existing paths are never replaced\n"
           "      --basename NAME       FITS file name prefix (default: input stem)\n"
           "  -s, --start N             Skip N recognized CoBo frames (default: 0)\n"
           "  -n, --count N             Convert at most N frames (default: all)\n"
           "      --max-output-bytes N  Output size cap (default: 21474836480, 20 GiB)\n"
           "  -h, --help                Show this help\n";
}

}

int main(int argc, char **argv)
{
    try {
        GetDAQ::FitsOptions options;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "-h" || argument == "--help") {
                Usage(std::cout);
                return 0;
            }
            if (argument == "-i" || argument == "--input" ||
                argument.compare(0, 8, "--input=") == 0) {
                options.input = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "-o" || argument == "--output-dir" ||
                       argument.compare(0, 13, "--output-dir=") == 0) {
                options.outputDirectory = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "--basename" || argument.compare(0, 11, "--basename=") == 0) {
                options.baseName = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            } else if (argument == "-s" || argument == "--start" ||
                       argument.compare(0, 8, "--start=") == 0) {
                options.firstCoboFrame = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "start frame");
            } else if (argument == "-n" || argument == "--count" ||
                       argument.compare(0, 8, "--count=") == 0) {
                options.maximumCoboFrames = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "frame count");
            } else if (argument == "--max-output-bytes" ||
                       argument.compare(0, 19, "--max-output-bytes=") == 0) {
                options.maximumOutputBytes = GetDAQ::detail::ParseUnsignedOption(
                    GetDAQ::detail::OptionValue(i, argc, argv, argument), "output byte limit");
            } else {
                throw std::invalid_argument("unknown option: " + argument);
            }
        }
        const GetDAQ::FitsResult result = GetDAQ::ConvertGrawToFits(options);
        std::cout << "Converted " << result.eventsWritten << " events, " << result.channelsWritten
                  << " channel tables (" << result.outputBytes << " bytes) into "
                  << options.outputDirectory << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "getdaq-fits: " << error.what() << '\n';
        return 2;
    }
}
