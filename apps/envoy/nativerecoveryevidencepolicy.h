#ifndef NATIVERECOVERYEVIDENCEPOLICY_H
#define NATIVERECOVERYEVIDENCEPOLICY_H

#include "../../common/control/eccprotocol.h"
#include <QString>

namespace GetDAQ {
QString NativeRecoveryEvidenceError(const EccControlIdentity &identity,
                                    const EccControlSystemStatus &ready,
                                    const EccControlSystemStatus &current);
}

#endif
