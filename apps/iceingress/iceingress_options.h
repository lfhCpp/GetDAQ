#ifndef GETDAQ_ICEINGRESS_OPTIONS_H
#define GETDAQ_ICEINGRESS_OPTIONS_H

#include "iceingress.h"

#include <iosfwd>
#include <string>

namespace GetDAQ {
struct IceIngressOptions {
    IceIngressConfig config;
    std::string bind = "127.0.0.1";
    unsigned int port = 46105;
    unsigned int statusPort = 46115;
    unsigned int messageMiB = 1;
    bool help = false;
};

IceIngressOptions ParseIceIngressOptions(int argc, char **argv);
void PrintIceIngressUsage(std::ostream &output);
}

#endif
