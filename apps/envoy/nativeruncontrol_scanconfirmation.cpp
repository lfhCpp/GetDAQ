#include "nativeruncontrol.h"
#include "scanoperationrunner.h"
#include <stdexcept>

void NativeRunControl::ConfirmScanOperationControllerState()
{
    if (!fScan.awaitingControllerState || fScan.runOperationId.isEmpty() ||
        !fScan.operationRunner || fScan.runOperationResponse.isEmpty()) return;
    const unsigned int expected=fScan.awaitingControllerState;
    if (!fHaveStatus || fSystem.controller.state!=expected || fSystem.controller.transition ||
        fSystem.operationActive || fSystem.controller.outcomeUnknown ||
        fSystem.controller.needsReconciliation || fSystem.dataNeedsReconciliation) {
        try { fScan.operationRunner->MarkUnknown(fScan.runOperationId.toStdString(),
            "Fresh Native status did not confirm the completed scan operation"); } catch (...) {}
        fScan.awaitingControllerState=0;fScan.runMonitoring=false;
        emit ScanPointRunStatusSignal("Native state readback differs from the scan operation result; inspect the pending scan intent",false);
        if (!fScan.lifecycleCommand.isEmpty()) {
            fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
            fScan.lifecycleRecovery=false;
            fScan.runOperationResponse.clear(); fOperationId.clear(); fCommand.clear();
            fTarget=-1; fBusy=false; fPhase="Native pending scan intent: inspection required";
            Publish();
        }
        return;
    }
    if (fScan.lifecycleRecovery &&
        fSystem.controller.operationId != fScan.runOperationId.toStdString()) {
        emit ScanPointRunStatusSignal("Fresh Native status does not identify the exact saved operation; the scan intent remains unresolved", false);
        fScan.awaitingControllerState = 0;
        fScan.runMonitoring = false;
        fBusy = false;
        fScan.lifecycleCommand.clear();
        fScan.lifecycleRecovery = false;
        fScan.runOperationResponse.clear();
        fScan.runOperationId.clear();
        fOperationId.clear();
        fCommand.clear();
        fTarget = -1;
        fPhase = "Native pending scan intent: inspection required";
        Publish();
        return;
    }
    try {
        if (!fScan.lifecycleCommand.isEmpty()) ConfirmScanLifecycleControllerState();
        else ConfirmScanRunControllerState(expected);
    } catch (const std::exception &error) {
        if (!fScan.lifecycleCommand.isEmpty()) {
            const QString command=fScan.lifecycleCommand;
            try { fScan.operationRunner->MarkUnknown(fScan.runOperationId.toStdString(),
                "Native state was observed but scan journal completion failed"); } catch (...) {}
            fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
            fScan.runOperationResponse.clear(); fOperationId.clear(); fCommand.clear();
            fTarget=-1; fBusy=false; fPhase="Native pending scan intent: inspection required";
            fScan.awaitingControllerState=0;
            emit ScanPointRunStatusSignal(tr("Whole-system %1 state was observed but the scan journal could not advance; inspect the pending intent: %2")
                .arg(command).arg(QString::fromUtf8(error.what())),false);
            Publish();
        } else {
            fScan.awaitingControllerState=0;fScan.runMonitoring=false;
        }
        Fail(tr("Native state readback succeeded but the scan journal could not advance: %1").arg(QString::fromUtf8(error.what())));
    }
}

bool NativeRunControl::HandleScanOperationResult(const QString &id,const QString &phase,const QByteArray &body,
    const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result) {
    if (!fScan.lifecycleCommand.isEmpty() && id==fScan.runOperationId) {
        const unsigned int expectedState=fScan.lifecycleCommand=="describe" ? 2U :
            fScan.lifecycleCommand=="prepare" ? 3U : 4U;
        if (phase!="complete" || operation.request.command!=fScan.lifecycleCommand.toStdString() ||
            result.state!=expectedState || result.transition || result.error ||
            result.outcomeUnknown || result.needsReconciliation ||
            result.operationId!=fScan.runOperationId.toStdString()) {
            if (fScan.operationRunner) try { fScan.operationRunner->MarkUnknown(id.toStdString(),
                "Native scan lifecycle result did not report the expected known whole-system state"); } catch (...) {}
            const QString unresolved=fScan.lifecycleCommand;
            fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
            fScan.lifecycleRecovery=false;
            fScan.runOperationResponse.clear(); fScan.awaitingControllerState=0;
            fBusy=false; fOperationId.clear(); fCommand.clear(); fTarget=-1;
            fPhase="Native pending scan intent: inspection required";
            emit ScanPointRunStatusSignal(tr("Whole-system %1 result is unresolved; inspect the persisted scan intent before continuing.").arg(unresolved),false);
            Publish(); QueryStatusAfterOperation(); return true;
        }
        fScan.runOperationResponse=body; fScan.awaitingControllerState=expectedState;
        fOperationId.clear(); fCommand.clear(); fTarget=-1; fBusy=false;
        fPhase="Confirming scan-point "+fScan.lifecycleCommand+" state";
        QueryStatusAfterOperation(); Publish(); return true;
    }
    if (!fScan.runOperationId.isEmpty() && id==fScan.runOperationId &&
        (operation.request.command=="start" || operation.request.command=="stop")) {
        const unsigned int expectedState=operation.request.command=="start" ? 5U :
            operation.request.command=="stop" ? 4U : 0U;
        if (phase!="complete" || !expectedState || result.state!=expectedState || result.transition ||
            result.error || result.outcomeUnknown || result.needsReconciliation) {
            if (fScan.operationRunner) try { fScan.operationRunner->MarkUnknown(id.toStdString(),
                "Native scan Run operation did not return the expected known controller state"); }
                catch (...) {}
            fScan.runMonitoring=false;fScan.awaitingControllerState=0;
            emit ScanPointRunStatusSignal("Scan Run outcome requires inspection; scan journal remains unresolved",false);
            Fail("Native scan Run result is unresolved; inspect the original operation before continuing");
            return true;
        } else {
            fScan.runOperationResponse=body;
            fScan.awaitingControllerState=expectedState;
            // The scan-point Run lifecycle has its own journal and completion
            // boundary. Do not route its Start/Stop through the ordinary
            // production Run finalizer (which continues into receiver
            // breakup, Finish/seal and catalog).
            fOperationId.clear();fCommand.clear();fTarget=-1;fBusy=false;
            fPhase=expectedState==5U ? "Confirming scan-point Run start" :
                "Confirming scan-point event-target Stop";
            QueryStatusAfterOperation();Publish();
            return true;
        }
    }
    return false;
}

