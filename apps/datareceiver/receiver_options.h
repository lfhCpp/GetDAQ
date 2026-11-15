#ifndef GETDAQ_RECEIVER_OPTIONS_H
#define GETDAQ_RECEIVER_OPTIONS_H

#include "receiver.h"
#include <iosfwd>
#include <string>
#include <vector>

namespace GetDAQ {
struct ReceiverCommandLine {
    ReceiverConfig config;
    bool showHelp = false;
};

// Arguments exclude argv[0]. Help returns immediately, matching the CLI's
// historical behavior even when later arguments would otherwise be invalid.
ReceiverCommandLine ParseReceiverOptions(const std::vector<std::string> &arguments);
void PrintReceiverUsage(std::ostream &output);
}

#endif
