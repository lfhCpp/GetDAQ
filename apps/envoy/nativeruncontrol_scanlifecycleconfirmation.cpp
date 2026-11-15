#include "nativeruncontrol.h"
#include "scanoperationrunner.h"

void NativeRunControl::ConfirmScanLifecycleControllerState()
{
    bool scanControlHoldReleased = false;
    if (fScan.lifecycleRecovery) {
        fScan.operationRunner->CompleteObserved(fScan.runOperationId.toStdString(),
                                               fScan.runOperationResponse.toStdString());
        scanControlHoldReleased = fClient->CompleteScanLifecycleObservation(fScan.runOperationId);
        if (!scanControlHoldReleased && !fConfig.nativeClient.alarmStatusEnabled)
            emit LogSignal("The saved scan operation and ECC state match, but an independent Native control hold remains latched");
    } else {
        fScan.operationRunner->Complete(fScan.runOperationId.toStdString(),
                                       fScan.runOperationResponse.toStdString());
    }

    const QString completed = fScan.lifecycleCommand;
    const QString recoveredOperationId = fScan.runOperationId;
    const bool wasScanRecovery = fScan.lifecycleRecovery;
    fScan.lifecycleCommand.clear();
    fScan.awaitingControllerState = 0;
    fScan.lifecycleRecovery = false;
    fScan.runOperationId.clear();
    fScan.runOperationResponse.clear();
    fScan.operationRunner.reset();
    fScan.runStore.reset();
    fOperationId.clear();
    fCommand.clear();
    fTarget = -1;
    fBusy = false;
    fPhase = "Native monitoring";
    fScan.identityResponseBody.clear();
    if (wasScanRecovery && !scanControlHoldReleased && fConfig.nativeClient.alarmStatusEnabled)
        fScan.recoveryControlHoldOperationId = recoveredOperationId;
    emit ScanPointRunStatusSignal(
        tr("Whole-system %1 completed and was confirmed by fresh ECC state. Recheck runtime identity before the next point action.")
            .arg(completed), false);
    Publish();
    if (scanControlHoldReleased) fClient->QueryIdentity();
}
