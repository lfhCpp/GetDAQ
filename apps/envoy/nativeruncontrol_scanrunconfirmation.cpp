#include "nativeruncontrol.h"
#include "scanoperationrunner.h"
#include <QUuid>
#include <stdexcept>

void NativeRunControl::ConfirmScanRunControllerState(unsigned int expected)
{
    if (expected == 5U) {
        fScan.operationRunner->Complete(fScan.runOperationId.toStdString(),
                                       fScan.runOperationResponse.toStdString());
        fScan.runMonitoring = true;
        fScan.runClock.restart();
        fRunClock.restart();
        emit ScanPointRunStatusSignal(
            "Scan point Run confirmed; monitoring the selected receiver event count", true);
    } else if (fScan.stopForSuccess) {
        fScan.operationRunner->Complete(fScan.runOperationId.toStdString(),
                                       fScan.runOperationResponse.toStdString());
        fDuration = fRunClock.isValid() ? fRunClock.elapsed() / 1000.0 : fDuration;
        fRunClock.invalidate();
        fJournal["duration_seconds"] = fDuration;
        fJournal["scan_point_event_target_reached"] = true;
        if (!Journal("Scan point event target reached; Native Stop confirmed"))
            throw std::runtime_error("Cannot persist completed scan-point Run record");
        fScan.pointFinalizationPending = true;
        const std::string confirmedStopOperationId = fScan.runOperationId.toStdString();
        fScan.runOperationId = "gui-" + QUuid::createUuid().toString(QUuid::WithoutBraces) +
            "-scan-receiver-finish";
        fScan.operationRunner->PersistIntent(fScan.runOperationId.toStdString(), "receiver-finish",
            "receiver-finish after confirmed scan Run Stop operation " + confirmedStopOperationId);
        // Each scan point is an independent archived Run. After its event
        // target, continue through Breakup, receiver seal, optional catalog,
        // and Run-record finalization.
        fCommand = "finish-after-status";
        fOperationId.clear();
        fTarget = -1;
        fBusy = false;
        fPhase = "Scan point stopped; finishing receiver session and archive";
        emit ScanPointRunStatusSignal(
            tr("Event target reached and Native Stop confirmed. Finishing this point's receiver session and archive: %1")
                .arg(fScan.stopReason), true);
    } else {
        fScan.operationRunner->MarkUnknown(fScan.runOperationId.toStdString(),
            fScan.stopReason.isEmpty() ? "Scan point aborted; it cannot be marked successful" :
                                         fScan.stopReason.toStdString());
        emit ScanPointRunStatusSignal(
            tr("Native Stop confirmed, but this scan point was aborted and remains unresolved: %1")
                .arg(fScan.stopReason), false);
    }
    fScan.awaitingControllerState = 0;
    fScan.runOperationId.clear();
    fScan.runOperationResponse.clear();
    fScan.stopForSuccess = false;
}
