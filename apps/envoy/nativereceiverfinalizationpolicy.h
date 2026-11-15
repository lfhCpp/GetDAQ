#ifndef NATIVERECEIVERFINALIZATIONPOLICY_H
#define NATIVERECEIVERFINALIZATIONPOLICY_H

#include "protocol.h"
#include "runconfig.h"
#include <QString>

namespace GetDAQ {
bool HasNormalNativeRetirementEvidence(const EccControlSystemStatus &status);
QString NativeReceiverFinalizationError(const EccControlSystemStatus &status,
                                        const RunConfig &config,
                                        bool requireCatalog);
}

#endif
