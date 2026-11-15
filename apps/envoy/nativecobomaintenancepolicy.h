#ifndef NATIVECOBOMAINTENANCEPOLICY_H
#define NATIVECOBOMAINTENANCEPOLICY_H

#include <string>

namespace GetDAQ {
struct NativeCoBoMaintenanceContext {
    bool available=false;
    bool activeRun=false;
    unsigned int controllerState=0;
    bool controllerError=false;
    std::string instanceId;
    std::string snapshotSha256;
};

bool CanSubmitNativeCoBoMaintenance(const NativeCoBoMaintenanceContext &context,
                                   const std::string &instanceId,
                                   const std::string &snapshotSha256);
bool IsNativeCoBoMaintenanceCommand(const std::string &command);
}

#endif
