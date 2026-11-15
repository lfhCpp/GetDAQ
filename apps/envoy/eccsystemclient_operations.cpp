#include "eccsystemclient.h"
#include "eccsystemclient_p.h"
#include <stdexcept>

using namespace EccSystemClientInternal;

void EccSystemClient::LatchControlFault()
{
    ++fControlFaultGeneration;
    fControlFault = true;
    fMatched = false;
}
void EccSystemClient::BeginScanLifecycleObservation(const QString &operationId)
{
    if (operationId.isEmpty()) throw std::invalid_argument("Scan lifecycle recovery requires an operation ID");
    if (fScanRecoveryOperationId == operationId) return;
    fScanRecoveryOperationId = operationId;
    fScanRecoveryFaultGeneration = fControlFaultGeneration;
    fScanRecoveryMayClearFault = !fControlFault;
    fScanRecoveryObservationFault = false;
}
bool EccSystemClient::CompleteScanLifecycleObservation(const QString &operationId)
{
    if (operationId.isEmpty() || fScanRecoveryOperationId != operationId ||
        !fStatusAvailable || !fStatusClock.isValid() ||
        fStatusClock.elapsed() > fOptions.statusMaxAgeMs || fFault ||
        fLatestStatus.controller.outcomeUnknown || fLatestStatus.controller.needsReconciliation ||
        fLatestStatus.controlNeedsReconciliation || fLatestStatus.dataNeedsReconciliation ||
        fLatestStatus.controller.operationId != operationId.toStdString())
        return false;
    if (fOptions.alarmStatusEnabled && (fAlarmBusy || !fAlarmAvailable)) return false;
    if (fScanRecoveryObservationFault) {
        if (!fScanRecoveryMayClearFault || fControlFaultGeneration != fScanRecoveryFaultGeneration) {
            fScanRecoveryOperationId.clear();
            fScanRecoveryFaultGeneration = 0;
            fScanRecoveryMayClearFault = false;
            fScanRecoveryObservationFault = false;
            return false;
        }
        fControlFault = false;
        fMatched = true;
    }
    const bool controlReady = !fControlFault;
    fScanRecoveryOperationId.clear();
    fScanRecoveryFaultGeneration = 0;
    fScanRecoveryMayClearFault = false;
    fScanRecoveryObservationFault = false;
    return controlReady;
}
bool EccSystemClient::SubmitRequest(const GetDAQ::EccControlRequest &request)
{
    const auto operationId=QString::fromStdString(request.operationId);
    try {
        const auto bytes = GetDAQ::EncodeEccControlRequest(request);
        const auto headers = Headers();
        QString error;
        if (!fWriter || !fWriter(bytes, &error)) {
            emit OperationSignal(operationId, "not-sent", {}, "Cannot persist ECC intent; no command sent."); return false;
        }
        fPendingId = operationId; // 持久意图之后，任何发送/回复失败都保留同一ID。
        if(request.command=="retire-data-plans")RetainPlanRetirement(request);
        fStatusAvailable = false;
        fSubmitBusy = true;
        fHttp->Request(Url("/v1/operations"), QByteArray::fromStdString(bytes), "application/json", fOptions.timeoutMs,
            [this, request](const QByteArray &body, const QString &failure) { fSubmitBusy=false; Receive(request, body, failure); }, headers);
    } catch (const std::exception &e) {
        fSubmitBusy = false;
        emit OperationSignal(operationId, Pending() ? "unknown" : "not-sent", {}, Failure(e)); return false;
    }
    return true;
}
bool EccSystemClient::Observe(const GetDAQ::EccControlRequest &request)
{
    if (fReadBusy || fSubmitBusy) return false;
    try {
        GetDAQ::ValidateEccControlRequest(request);
        if (request.snapshotSha256 != Utf8(fOptions.snapshotSha256) || request.clientId != Utf8(fOptions.clientId) || request.clientEpoch != Utf8(fOptions.clientEpoch))
            throw std::runtime_error("Saved ECC request differs from this client scope");
        const auto headers = Headers(); fReadBusy = true;
        fHttp->Request(Url("/v1/operations/" + QString::fromStdString(request.operationId)), {}, {}, fOptions.timeoutMs,
            [this, request](const QByteArray &body, const QString &failure) { fReadBusy = false; Receive(request, body, failure); }, headers);
    } catch (const std::exception &e) { emit OperationSignal(QString::fromStdString(request.operationId), "unknown", {}, Failure(e)); return false; }
    return true;
}
void EccSystemClient::Receive(const GetDAQ::EccControlRequest &request, const QByteArray &body, const QString &failure)
{
    QString error = failure, phase = "unknown";
    if (error.isEmpty()) try {
        const auto operation = GetDAQ::DecodeEccControlOperation(body.toStdString());
        if (GetDAQ::EncodeEccControlRequest(operation.request) != GetDAQ::EncodeEccControlRequest(request)) throw std::runtime_error("ECC operation response differs from persisted request");
        phase = QString::fromStdString(operation.phase);
        if (phase == "complete" || phase == "failed") {
            const bool closesPending=fPendingId==QString::fromStdString(request.operationId);
            const auto status = GetDAQ::DecodeEccControlControllerStatus(operation.resultJson);
            if(request.command=="retire-data-plans" && phase=="complete" &&
                ((status.state!=1 && status.state!=2) || status.transition || status.outcomeUnknown || status.error))
                throw std::runtime_error("Completed plan retirement must preserve known Idle/Described controller state");
            if(((request.command=="reconcile-description" || request.command=="release-stopped-alarm") || (request.command=="reconcile-preparation" || request.command=="reconcile-preparation-with-alarm") || ((request.command=="reconcile-acquisition" || request.command=="reconcile-acquisition-with-sources") || request.command=="reconcile-acquisition-with-alarm")) && phase=="complete" &&
               (status.state!=2 || status.operationId!=request.operationId || status.transition || status.outcomeUnknown || status.needsReconciliation))
                throw std::runtime_error("Description recovery result does not match the verified target");
            if(closesPending)fPendingId.clear();
            if (status.outcomeUnknown || status.needsReconciliation) LatchControlFault();
            else if(((request.command=="reconcile-description" || request.command=="release-stopped-alarm") || (request.command=="reconcile-preparation" || request.command=="reconcile-preparation-with-alarm") || ((request.command=="reconcile-acquisition" || request.command=="reconcile-acquisition-with-sources") || request.command=="reconcile-acquisition-with-alarm")) && phase=="complete" && closesPending) {
                if(status.state!=2 || status.operationId!=request.operationId || status.transition)
                    throw std::runtime_error("Description recovery result does not match the verified target");
                ++fRecoveryGeneration;fAlarmAvailable=false;
                fControlFault=false;fMatched=!fFault;fStatusAvailable=false;
                ++fControlFaultGeneration;
                fScanRecoveryOperationId.clear();fScanRecoveryFaultGeneration=0;
                fScanRecoveryMayClearFault=false;fScanRecoveryObservationFault=false;
            }
        }
        if (phase == "unknown") {
            if (fScanRecoveryOperationId == QString::fromStdString(request.operationId)) {
                fScanRecoveryObservationFault = true;
                fControlFault = true;
                fMatched = false;
            } else LatchControlFault();
        }
    } catch (const std::exception &e) { fFault = true; fMatched = false; error = Failure(e); }
    emit OperationSignal(QString::fromStdString(request.operationId), phase, error.isEmpty() ? body : QByteArray(), error);
}
