#include "matrix.h"
#include "cli_options.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
void Usage(std::ostream &out)
{
    out << "Usage: getdaq-matrix --input FILE --output FILE [options]\n"
           "Convert complete CoBo type-1 waveform frames to the legacy CoBoFrameViewer .gmat layout.\n"
           "Options:\n"
           "  -i, --input FILE          MFM/CoBo raw input\n"
           "  -o, --output FILE         New output path; existing paths are never replaced\n"
           "  -s, --start N             Skip N recognized CoBo frames (default: 0)\n"
           "  -n, --count N             Convert at most N frames (default: all)\n"
           "      --max-output-bytes N  Output cap (default: 21474836480, 20 GiB)\n"
           "  -h, --help                Show this help\n";
}

}

int main(int argc, char **argv)
{
    try {
        GetDAQ::MatrixOptions options;
        for (int i = 1; i < argc; ++i) {
            const std::string argument(argv[i]);
            if (argument == "-h" || argument == "--help") { Usage(std::cout); return 0; }
            if (argument == "-i" || argument == "--input" || argument.compare(0, 8, "--input=") == 0)
                options.input = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            else if (argument == "-o" || argument == "--output" || argument.compare(0, 9, "--output=") == 0)
                options.output = GetDAQ::detail::OptionValue(i, argc, argv, argument);
            else if (argument == "-s" || argument == "--start" || argument.compare(0, 8, "--start=") == 0)
                options.firstCoboFrame = GetDAQ::detail::ParseUnsignedOption(GetDAQ::detail::OptionValue(i, argc, argv, argument), "start frame");
            else if (argument == "-n" || argument == "--count" || argument.compare(0, 8, "--count=") == 0)
                options.maximumCoboFrames = GetDAQ::detail::ParseUnsignedOption(GetDAQ::detail::OptionValue(i, argc, argv, argument), "frame count");
            else if (argument == "--max-output-bytes" || argument.compare(0, 19, "--max-output-bytes=") == 0)
                options.maximumOutputBytes = GetDAQ::detail::ParseUnsignedOption(GetDAQ::detail::OptionValue(i, argc, argv, argument), "output byte limit");
            else throw std::invalid_argument("unknown option: " + argument);
        }
        const GetDAQ::MatrixResult result = GetDAQ::ConvertGrawToMatrix(options);
        std::cout << "Converted " << result.matricesWritten << " matrices ("
                  << result.outputBytes << " bytes) to " << options.output << '\n';
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "getdaq-matrix: " << error.what() << '\n';
        return 2;
    }
}
