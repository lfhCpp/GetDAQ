#include "nativealarmrecoverypolicy.h"

namespace GetDAQ {
bool MatchesNativeAlarmRecovery(const EccControlAlarmStatus &alarm,
                                const EccControlSystemStatus &system,
                                NativeAlarmRecoveryMode mode) {
    if (!alarm.enabled || !alarm.latched ||
        alarm.instanceId!=system.instanceId ||
        alarm.snapshotSha256!=system.snapshotSha256)
        return false;

    if (mode==NativeAlarmRecoveryMode::AcknowledgeUnknownStop)
        return alarm.phase=="complete" && !alarm.operationId.empty() &&
            alarm.controllerOutcomeUnknown &&
            alarm.controllerOperationId==system.controller.operationId;

    if (alarm.controllerOutcomeUnknown ||
        (alarm.phase!="complete" && alarm.phase!="held-no-run"))
        return false;
    return alarm.operationId.empty()
        ? alarm.controllerOperationId==system.controller.operationId
        : alarm.operationId==system.controller.operationId;
}
}
