#include "nativeruncontrol.h"
#include "nativereceiverfinalizationpolicy.h"
#include "runrecorder.h"
#include "scanexecutionjournalstore.h"
#include "scanruntimeidentitycoordinator.h"
#include "scaneventcountbaselinecoordinator.h"
#include "scaneventcountobservation.h"
#include "scanexecutionsnapshot.h"
#include "scanexecutionpolicy.h"
#include "scanexecutionjournal.h"
#include "scanoperationrunner.h"
#include "scaneventcountmonitor.h"
#include "systemscanplan.h"
#include "configurationsha256.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#include <stdexcept>

NativeRunControl::ScanState::ScanState() = default;
NativeRunControl::ScanState::~ScanState() = default;
void NativeRunControl::ScanState::ReleaseResources() {
    identityCoordinator.reset();identityStore.reset();
    eventBaselineCoordinator.reset();eventBaselineStore.reset();
    runtimeSnapshot.reset();runtimeExpectation.reset();runtimeJournalPath.clear();
    operationRunner.reset();runStore.reset();eventMonitor.reset();identityResponseBody.clear();
    recoveryControlHoldOperationId.clear();
}


void NativeRunControl::StartScanPoint(const QString &journalPath)
{
    try {
        if (!fConnected || fShuttingDown || fBusy || fActive || !Available() ||
            !fScan.runtimeSnapshot || !fScan.runtimeExpectation || !fScan.eventMonitor ||
            fScan.thresholdManualOnly || QFileInfo(journalPath).absoluteFilePath()!=fScan.runtimeJournalPath)
            throw std::runtime_error("Scan Run requires this connected point's validated event policy, identity and baseline");
        if (!fConfig.nativeCatalog)
            throw std::runtime_error("Scan-point Run requires native_catalog so its receiver data can be completed and bound per point");
        if (!fHaveStatus || fSystem.controller.state!=4 || fSystem.controller.transition ||
            fSystem.operationActive || fSystem.controller.outcomeUnknown ||
            fSystem.controller.needsReconciliation || fSystem.dataNeedsReconciliation ||
            !fStatusClock.isValid() || fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
            !fIdentityMatched || !fIdentityClock.isValid() ||
            fIdentityClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
            !fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) ||
            !fObservations.ReceiverError().isEmpty() || fObservations.Receivers().instanceId!=fSystem.instanceId ||
            fObservations.Receivers().snapshotSha256!=fSystem.snapshotSha256 ||
            fObservations.Receivers().dataBindingSha256!=fScan.runtimeExpectation->dataBindingSha256)
            throw std::runtime_error("Fresh matching Native Ready, receiver and identity observations are required");
        fScan.operationRunner.reset();fScan.runStore.reset();
        fScan.runStore=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString());
        const GetDAQ::ScanExecutionJournal scan=fScan.runStore->Current();
        if (!scan.HasEventCountBaseline())
            throw std::runtime_error("Scan event baseline is not persisted");
        const GetDAQ::ScanEventCountBaseline &baseline=scan.EventCountBaseline();
        if (
            baseline.sourceId!=fScan.runtimeSnapshot->TargetCoBo() || baseline.instanceId!=fSystem.instanceId ||
            baseline.snapshotSha256!=fSystem.snapshotSha256 ||
            baseline.dataBindingSha256!=fObservations.Receivers().dataBindingSha256)
            throw std::runtime_error("Persisted event baseline differs from the current target or Native deployment");
        for (const auto &node:fSystem.nodes) for (const auto &receiver:node.receivers)
            if (node.needsReconciliation || node.state!="ready" || receiver.pid<=0 || receiver.reaped ||
                receiver.exitCode!=-1 || receiver.sealVerified)
                throw std::runtime_error("Scan Run requires every Native receiver in its verified Ready session");
        if (scan.State()!=GetDAQ::ScanPointState::Configured || scan.HasPendingOperation() ||
            scan.PendingOperationUnknown() || !scan.HasEventCountBaseline() ||
            scan.RuntimeIdentitySha256().empty() || fScan.identityResponseBody.isEmpty())
            throw std::runtime_error("Scan journal is not the exact Configured point with a bound event baseline");
        const auto identityResponse=GetDAQ::DecodeScanRuntimePointIdentityResponse(
            fScan.identityResponseBody.toStdString());
        if (identityResponse.expectation.instanceId!=fSystem.instanceId ||
            GetDAQ::EncodeScanRuntimeIdentityExpectation(identityResponse.expectation)!=
            GetDAQ::EncodeScanRuntimeIdentityExpectation(*fScan.runtimeExpectation))
            throw std::runtime_error("The successful scan identity revalidation belongs to a different point or deployment");

        QString runError;
        if (!RunRecorder::Reserve(fConfig,&runError) || !RunRecorder::ReadJournal(fConfig,&fJournal,&runError))
            throw std::runtime_error(runError.toStdString());
        fScan.operationRunner.reset(new GetDAQ::ScanOperationRunner(*fScan.runStore));
        fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-scan-run-start";
        GetDAQ::EccControlRequest request;
        request.operationId=fOperationId.toStdString();request.command="start";
        request.instanceId=fSystem.instanceId;request.snapshotSha256=fSystem.snapshotSha256;
        request.clientId=fConfig.nativeClient.clientId.toStdString();
        request.clientEpoch=fConfig.nativeClient.clientEpoch.toStdString();
        const std::string requestBytes=GetDAQ::EncodeEccControlRequest(request);
        fScan.operationRunner->PersistIntent(fOperationId.toStdString(),"run-start",requestBytes);
        fScan.runOperationId=fOperationId;fScan.runMonitoring=false;
        fScan.stopForSuccess=false;fScan.pointFinalizationPending=false;
        fActive=true;fDuration=0;fRunClock.invalidate();
        fJournal["native_identity"]=QString::fromUtf8(fIdentityBody);
        fJournal["receiver_ready"]=QString::fromUtf8(fStatusBody);
        if (!fConfigurationBody.isEmpty())fJournal["native_configuration"]=QString::fromUtf8(fConfigurationBody);
        if (!Journal("Native scan point Run reserved before Start"))
            throw std::runtime_error("Cannot save Native Run journal before scan start");
        fCommand="start";fTarget=5;fBusy=true;fPhase="Starting scan point on the complete Native ECC system";
        fDeadline.start(fConfig.stepTimeoutMs);Publish();
        emit ScanPointRunStatusSignal("Scan Run start intent persisted; waiting for the Native operation result and fresh state readback",true);
        if (!fClient->Submit("start",fOperationId))
            emit LogSignal("Scan Run start did not submit; its original scan and ECC intents must be inspected");
    } catch (const std::exception &error) {
        fScan.runMonitoring=false;
        const QString message=tr("Scan point was not started: %1").arg(QString::fromUtf8(error.what()));
        if (fActive) Fail(message);
        emit ScanPointRunStatusSignal(message,false);
        Publish();
    }
}
void NativeRunControl::ExecuteScanPointLifecycleOperation(const QString &journalPath,
                                                           const QString &command)
{
    try {
        if (!fConnected || fShuttingDown || fBusy || fActive || !Available() ||
            !fScan.runtimeSnapshot || !fScan.runtimeExpectation ||
            QFileInfo(journalPath).absoluteFilePath()!=fScan.runtimeJournalPath ||
            fScan.identityResponseBody.isEmpty())
            throw std::runtime_error("Scan lifecycle action requires a fresh bound identity for this point and an idle controller");
        const auto identity=GetDAQ::DecodeScanRuntimePointIdentityResponse(
            fScan.identityResponseBody.toStdString());
        if (identity.mode!="revalidate" || identity.expectation.instanceId!=fSystem.instanceId ||
            identity.expectation.snapshotSha256!=fSystem.snapshotSha256 ||
            GetDAQ::EncodeScanRuntimeIdentityExpectation(identity.expectation)!=
            GetDAQ::EncodeScanRuntimeIdentityExpectation(*fScan.runtimeExpectation) ||
            identity.observedControllerState!=fSystem.controller.state)
            throw std::runtime_error("A fresh successful identity revalidation for the current ECC state is required");
        const unsigned int expectedState=command=="describe" ? 1U :
            command=="prepare" ? 2U : command=="configure" ? 3U : 0U;
        const unsigned int resultState=command=="describe" ? 2U :
            command=="prepare" ? 3U : command=="configure" ? 4U : 0U;
        const GetDAQ::ScanPointState expectedJournalState=command=="describe" ?
            GetDAQ::ScanPointState::EccStarted : command=="prepare" ?
            GetDAQ::ScanPointState::Described : GetDAQ::ScanPointState::Prepared;
        if (!expectedState || !resultState || fSystem.controller.state!=expectedState ||
            fSystem.controller.transition || fSystem.operationActive ||
            fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation ||
            fSystem.dataNeedsReconciliation || !fStatusClock.isValid() ||
            fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
            !fIdentityMatched || !fIdentityClock.isValid() ||
            fIdentityClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
            !fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) ||
            !fObservations.ReceiverError().isEmpty() || fObservations.Receivers().instanceId!=fSystem.instanceId ||
            fObservations.Receivers().snapshotSha256!=fSystem.snapshotSha256 ||
            fObservations.Receivers().dataBindingSha256!=fScan.runtimeExpectation->dataBindingSha256 ||
            fClient->Busy() || fClient->Pending())
            throw std::runtime_error("Fresh matching ECC status, identity and receiver observations in the expected whole-system state are required");
        fScan.runStore=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString());
        const auto journal=fScan.runStore->Current();
        if (journal.State()!=expectedJournalState || journal.HasPendingOperation() ||
            journal.PendingOperationUnknown() || journal.RuntimeIdentitySha256().empty())
            throw std::runtime_error("Scan journal state does not permit this single lifecycle action");
        fScan.operationRunner.reset(new GetDAQ::ScanOperationRunner(*fScan.runStore));
        fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-scan-"+command;
        GetDAQ::EccControlRequest request;
        request.operationId=fOperationId.toStdString(); request.command=command.toStdString();
        request.instanceId=fSystem.instanceId; request.snapshotSha256=fSystem.snapshotSha256;
        request.clientId=fConfig.nativeClient.clientId.toStdString();
        request.clientEpoch=fConfig.nativeClient.clientEpoch.toStdString();
        const std::string requestBytes=GetDAQ::EncodeEccControlRequest(request);
        fScan.operationRunner->PersistIntent(fOperationId.toStdString(),command.toStdString(),requestBytes);
        fScan.runOperationId=fOperationId; fScan.lifecycleCommand=command;
        fScan.runMonitoring=false; fScan.awaitingControllerState=0;
        fCommand=command; fTarget=int(resultState); fBusy=true;
        fPhase="Submitting scan-point whole-system "+command;
        fDeadline.start(fConfig.stepTimeoutMs); Publish();
        if (!fClient->Submit(command,fOperationId)) {
            fScan.operationRunner->MarkUnknown(fOperationId.toStdString(),
                "Native client did not confirm submission; inspect the persisted scan intent before continuing");
            fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
            fScan.runOperationResponse.clear(); fScan.awaitingControllerState=0;
            emit ScanPointRunStatusSignal(tr("Whole-system %1 was not confirmed as submitted; inspect the persisted intent before continuing.").arg(command),false);
            Fail("Scan lifecycle operation outcome is unresolved; inspect the persisted operation before continuing");
        }
    } catch (const std::exception &error) {
        emit ScanPointRunStatusSignal(tr("Scan lifecycle action was not submitted: %1")
            .arg(QString::fromUtf8(error.what())),false);
    }
}
