#ifndef GETDAQ_REMOTEENDPOINTIDENTITY_P_H
#define GETDAQ_REMOTEENDPOINTIDENTITY_P_H

#include "remotereceivermanager.h"
#include <string>

namespace GetDAQ {
namespace RemoteReceiverManagerInternal {
RemoteReceiverOptions CheckedOptions(RemoteReceiverOptions options);
std::string Binding(const RemoteReceiverOptions &options);
}
}

#endif
