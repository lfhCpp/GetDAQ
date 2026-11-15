#ifndef GETDAQ_NATIVE_STATUS_PROJECTION_H
#define GETDAQ_NATIVE_STATUS_PROJECTION_H

#include "runconfig.h"
#include "../../common/control/eccprotocol.h"

// Projects the authoritative whole-system ECC snapshot into per-configured
// CoBo base rows. Throws when receiver ownership conflicts with the snapshot.
QVector<NodeStatus> BuildNativeSystemStatusRows(
    const RunConfig &config,
    const GetDAQ::EccControlSystemStatus &system,
    qint64 observationAgeMs,
    bool runActive);

// Build the GUI's Native observations from independently refreshed control,
// receiver, source, and receiver-failure evidence. This function is read-only.
QVector<NodeStatus> ProjectNativeStatus(
    const RunConfig &config,
    const QVector<NodeStatus> &statusRows,
    const GetDAQ::EccControlSystemStatus &system,
    bool haveSystemStatus,
    qint64 controllerAgeMs,
    const GetDAQ::EccControlReceiverObservation &receivers,
    bool receiverFresh,
    const QString &receiverError,
    const GetDAQ::EccControlSourceObservation &sources,
    bool sourceFresh,
    const QString &sourceError,
    const GetDAQ::EccControlReceiverFailureStatus &receiverFailure,
    bool receiverFailureFresh,
    const QString &receiverFailureError);

#endif
