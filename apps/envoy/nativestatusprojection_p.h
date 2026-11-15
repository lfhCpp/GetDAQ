#ifndef GETDAQ_NATIVE_STATUS_PROJECTION_P_H
#define GETDAQ_NATIVE_STATUS_PROJECTION_P_H
#include "nativestatusprojection.h"
namespace NativeStatusProjectionInternal {
void ApplyReceiverFailureProjection(
    const RunConfig &config,
    QVector<NodeStatus> &rows,
    const GetDAQ::EccControlSystemStatus &system,
    bool haveSystemStatus,
    qint64 controllerAgeMs,
    const GetDAQ::EccControlReceiverFailureStatus &receiverFailure,
    bool receiverFailureFresh,
    const QString &receiverFailureError);
}
#endif
