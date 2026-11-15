// Read-only reconciliation of the original pending scan lifecycle operation.
#include "nativeruncontrol.h"
#include "scanexecutionjournalstore.h"
#include "scanoperationrunner.h"
#include "scanruntimeidentitycoordinator.h"
#include "configurationsha256.h"
#include "eccprotocol.h"

#include <QFileInfo>
#include <stdexcept>

void NativeRunControl::ObservePendingScanLifecycleOperation(
    const QString &journalPath, const QString &operationId,
    const QString &expectedPlanSha256, const QByteArray &snapshotBytes)
{
    try {
        if (!fConnected || fShuttingDown || fBusy || fActive || !fClient || !fStore ||
            !fHaveStatus || fIdentityBody.isEmpty() || fClient->Busy() ||
            !fStatusClock.isValid() ||
            fStatusClock.elapsed() > fConfig.nativeClient.statusMaxAgeMs ||
            !fIdentityClock.isValid() ||
            fIdentityClock.elapsed() > fConfig.nativeClient.statusMaxAgeMs)
            throw std::runtime_error("Recovery requires a connected Native client, fresh fixed identity/status, and the currently bound scan plan");

        fScan.runStore.reset();
        fScan.operationRunner.reset();
        fScan.runStore = GetDAQ::ScanExecutionJournalStore::Open(
            journalPath.toUtf8().toStdString());
        fScan.runtimeJournalPath = QFileInfo(journalPath).absoluteFilePath();
        const GetDAQ::ScanExecutionJournal journal = fScan.runStore->Current();
        if (!journal.HasPendingOperation() || !journal.PendingOperationUnknown() ||
            journal.PendingOperationId() != operationId.toStdString() ||
            journal.SourcePlanSha256() != expectedPlanSha256.toStdString() ||
            GetDAQ::ConfigurationSha256(snapshotBytes.toStdString()) !=
                journal.SourceSnapshotSha256() ||
            (journal.PendingOperation() != "describe" &&
             journal.PendingOperation() != "prepare" &&
             journal.PendingOperation() != "configure") ||
            journal.RuntimeIdentitySha256().empty())
            throw std::runtime_error("Only the exact restart-quarantined Describe/Prepare/Configure intent can be observed automatically");

        GetDAQ::EccControlRequest request;
        if (!fStore->FindRequest(operationId.toStdString(), &request))
            throw std::runtime_error("The Native client has no saved request for this scan operation ID");
        if (request.operationId != journal.PendingOperationId() ||
            request.command != journal.PendingOperation() ||
            GetDAQ::ConfigurationSha256(GetDAQ::EncodeEccControlRequest(request)) !=
                journal.PendingRequestSha256() ||
            request.instanceId != fSystem.instanceId ||
            request.snapshotSha256 != fSystem.snapshotSha256 ||
            request.clientId != fConfig.nativeClient.clientId.toStdString() ||
            request.clientEpoch != fConfig.nativeClient.clientEpoch.toStdString())
            throw std::runtime_error("Saved Native request differs from the exact scan intent or current client scope");

        const GetDAQ::EccControlIdentity identity =
            GetDAQ::DecodeEccControlIdentity(fIdentityBody.toStdString());
        if (identity.serviceId != fConfig.nativeClient.serviceId.toStdString() ||
            identity.instanceId != request.instanceId ||
            identity.snapshotSha256 != request.snapshotSha256 ||
            identity.inputBindingSha256 != fConfig.nativeClient.inputBindingSha256.toStdString())
            throw std::runtime_error("Fresh Native identity response differs from the saved operation's fixed service scope");

        const unsigned int originState = request.command == "describe" ? 1U :
            request.command == "prepare" ? 2U : 3U;
        const unsigned int targetState = originState + 1U;
        if ((fSystem.controller.state != originState &&
             fSystem.controller.state != targetState) ||
            fSystem.controller.transition || fSystem.controller.outcomeUnknown ||
            fSystem.controller.needsReconciliation || fSystem.controlNeedsReconciliation ||
            fSystem.dataNeedsReconciliation ||
            (fSystem.operationActive &&
             fSystem.controller.operationId != request.operationId))
            throw std::runtime_error("Fresh Native controller status is outside the saved lifecycle operation's origin/target states");

        const auto pending = fStore->Pending();
        for (const auto &entry : pending)
            if (entry.operationId != request.operationId)
                throw std::runtime_error("Another Native request is unresolved; inspect it before scan recovery");

        fScan.operationRunner.reset(new GetDAQ::ScanOperationRunner(*fScan.runStore));
        fClient->BeginScanLifecycleObservation(operationId);
        fScan.runOperationId = operationId;
        fScan.lifecycleCommand = QString::fromStdString(request.command);
        fScan.lifecycleRecovery = true;
        fScan.runMonitoring = false;
        fScan.awaitingControllerState = 0;
        fScan.runOperationResponse.clear();
        fOperationId = operationId;
        fCommand = QString::fromStdString(request.command);
        fTarget = int(targetState);
        fBusy = true;
        fPhase = "Observing original scan lifecycle operation (read-only)";
        fDeadline.start(fConfig.stepTimeoutMs);
        Publish();
        if (!fClient->Observe(request)) {
            fDeadline.stop();
            fScan.lifecycleCommand.clear();
            fScan.lifecycleRecovery = false;
            fScan.runOperationId.clear();
            fOperationId.clear();
            fCommand.clear();
            fTarget = -1;
            fBusy = false;
            fPhase = "Native pending scan intent: inspection required";
            emit ScanPointRunStatusSignal("Original Native lifecycle result query could not start; the original command was not resent", false);
        }
    } catch (const std::exception &error) {
        fScan.lifecycleRecovery = false;
        fScan.lifecycleCommand.clear();
        fScan.runOperationId.clear();
        fOperationId.clear();
        fCommand.clear();
        fTarget = -1;
        fBusy = false;
        emit ScanPointRunStatusSignal(
            tr("Could not observe saved scan lifecycle operation: %1")
                .arg(QString::fromUtf8(error.what())), false);
        Publish();
    }
}
