#ifndef GETDAQ_DATARECEIVER_CLI_OPTIONS_H
#define GETDAQ_DATARECEIVER_CLI_OPTIONS_H

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace detail {

inline std::uint64_t ParseUnsignedOption(const std::string &text, const char *name)
{
    if (text.empty() || text[0] == '-')
        throw std::invalid_argument(std::string("invalid ") + name);
    errno = 0;
    char *end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (errno || !end || *end || value > std::numeric_limits<std::uint64_t>::max())
        throw std::invalid_argument(std::string("invalid ") + name + ": " + text);
    return static_cast<std::uint64_t>(value);
}

inline std::string OptionValue(int &index, int argc, char **argv,
                               const std::string &argument)
{
    const std::string::size_type equal = argument.find('=');
    if (equal != std::string::npos) return argument.substr(equal + 1);
    if (index + 1 >= argc)
        throw std::invalid_argument("missing value for " + argument);
    return argv[++index];
}

}
}

#endif
