#ifndef GETDAQ_HARDWARESTATEPROTOCOL_H
#define GETDAQ_HARDWARESTATEPROTOCOL_H
#include <cstdint>
#include <map>
#include <string>
#include <vector>
namespace GetDAQ {
struct HardwareStateObservation {
    struct Node {std::string id,role,message;bool available=false;std::uint64_t observedAtUnixMs=0;std::map<std::string,unsigned int> values;};
    struct CoBoLifecycle {std::string id,state;};
    std::string instanceId,snapshotSha256,configurationSha256,artifactSha256,controllerOperationId;
    std::string coboLifecycleSource;
    unsigned int controllerState=1;
    bool busy=false,consistent=false,controllerOutcomeUnknown=false,controllerNeedsReconciliation=false,coboLifecycleIsHardwareReadback=false;
    std::vector<Node> nodes;
    std::vector<CoBoLifecycle> coboLifecycle;
};
HardwareStateObservation DecodeHardwareStateObservation(const std::string &bytes);
}
#endif
