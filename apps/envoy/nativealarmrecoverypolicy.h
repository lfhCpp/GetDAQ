#ifndef NATIVEALARMRECOVERYPOLICY_H
#define NATIVEALARMRECOVERYPOLICY_H

#include "../../common/control/eccprotocol.h"

namespace GetDAQ {
enum class NativeAlarmRecoveryMode {
    AcknowledgeUnknownStop,
    ReleaseKnownStoppedRun
};

bool MatchesNativeAlarmRecovery(const EccControlAlarmStatus &alarm,
                                const EccControlSystemStatus &system,
                                NativeAlarmRecoveryMode mode);
}

#endif
