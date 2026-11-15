#include "nativecobomaintenancepolicy.h"

namespace GetDAQ {
bool CanSubmitNativeCoBoMaintenance(const NativeCoBoMaintenanceContext &context,
                                   const std::string &instanceId,
                                   const std::string &snapshotSha256) {
    return context.available && !context.activeRun && context.controllerState==3U &&
        !context.controllerError && context.instanceId==instanceId &&
        context.snapshotSha256==snapshotSha256;
}

bool IsNativeCoBoMaintenanceCommand(const std::string &command) {
    return command=="reprepare-cobo" || command=="configure-cobo" ||
        command=="set-cobo-led" || command=="trigger-asad-pulser" ||
        command=="blink-cobo-leds" || command=="flip-cobo-led" ||
        command=="pulse-cobo-led";
}
}
