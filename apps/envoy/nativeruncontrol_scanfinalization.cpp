#include "nativeruncontrol.h"
#include "nativereceiverfinalizationpolicy.h"
#include "scanexecutionjournalstore.h"
#include "scanexecutionjournal.h"
#include "scanoperationrunner.h"
#include "scaneventcountobservation.h"
#include "configurationsha256.h"
#include <QUuid>
#include <stdexcept>

void NativeRunControl::FinishScanRunEventQuery(const QString &senderId,
                                               const QByteArray &responseBytes,
                                               const QString &error)
{
    if (!fScan.runMonitoring || !fScan.eventMonitor || !fScan.runtimeSnapshot) return;
    if (!error.isEmpty()) { SubmitScanStop(false,"Receiver event observation failed: "+error); return; }
    try {
        const std::string targetCoBo=fScan.runtimeSnapshot->TargetCoBo();
        const bool statusFresh=fHaveStatus && fStatusClock.isValid() &&
            fStatusClock.elapsed()<=fConfig.nativeClient.statusMaxAgeMs;
        const bool receiversFresh=fObservations.ReceiverError().isEmpty() && fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs);
        const GetDAQ::ScanEventCountBaseline observation=GetDAQ::BindScanEventCountObservation(
            senderId.toStdString(),responseBytes.toStdString(),statusFresh,fSystem,receiversFresh,
            fObservations.Receivers(),*fScan.runtimeExpectation,targetCoBo);
        const auto decision=fScan.eventMonitor->Observe(observation,
            static_cast<std::uint64_t>(fScan.runClock.elapsed()));
        if (decision.decision==GetDAQ::ScanEventCountDecision::StopAtEventCount)
            SubmitScanStop(true,QString::fromStdString(decision.reason));
        else if (decision.decision==GetDAQ::ScanEventCountDecision::StopAtMaximumDuration ||
                 decision.decision==GetDAQ::ScanEventCountDecision::InspectRequired)
            SubmitScanStop(false,QString::fromStdString(decision.reason));
        else
            emit ScanPointRunStatusSignal(tr("Scan point running: %1 / %2 events")
                .arg(qulonglong(decision.eventsSinceBaseline)).arg(fScan.eventsPerPoint),true);
        if (decision.decision==GetDAQ::ScanEventCountDecision::Continue) {
            // Bracket the next event sample with fresh whole-system and
            // receiver-process observations. The next timer tick will query
            // events only after both responses have completed.
            fClient->QueryStatus();
            if (fConfig.nativeClient.receiverStatusEnabled) fClient->QueryReceivers();
        }
    } catch (const std::exception &failure) {
        SubmitScanStop(false,QString::fromUtf8(failure.what()));
    }
}
void NativeRunControl::SubmitScanStop(bool successfulPoint,const QString &reason)
{
    if (!fScan.runMonitoring || fBusy || !fClient || !fScan.operationRunner) return;
    fScan.runMonitoring=false;
    fScan.stopForSuccess=successfulPoint;
    fScan.stopReason=reason;
    const QString operationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-scan-run-stop";
    GetDAQ::EccControlRequest request;request.operationId=operationId.toStdString();request.command="stop";
    request.instanceId=fSystem.instanceId;request.snapshotSha256=fSystem.snapshotSha256;
    request.clientId=fConfig.nativeClient.clientId.toStdString();request.clientEpoch=fConfig.nativeClient.clientEpoch.toStdString();
    try {
        fScan.operationRunner->PersistIntent(operationId.toStdString(),"run-stop",
            GetDAQ::EncodeEccControlRequest(request));
    } catch (const std::exception &error) {
        Fail(QString::fromUtf8(error.what()));return;
    }
    fScan.runOperationId=operationId;fOperationId=operationId;fCommand="stop";fTarget=4;
    fBusy=true;fPhase=successfulPoint ? "Scan event target reached; stopping complete Native ECC system"
        : "Scan point abort/inspection: stopping Native system and retaining incomplete point";
    fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if (!fClient->Submit("stop",operationId))
        emit ScanPointRunStatusSignal(tr("Stop intent saved; the original operation is unresolved and must be inspected: %1").arg(reason),false);
}
void NativeRunControl::RecordScanFinalizationUnknown(const QString &reason) {
    if (!fScan.pointFinalizationPending || !fScan.operationRunner || !fScan.runStore) return;
    try {
        const GetDAQ::ScanExecutionJournal scan=fScan.runStore->Current();
        if (scan.HasPendingOperation() && !scan.PendingOperationUnknown() &&
            (scan.PendingOperation()=="receiver-finish" || scan.PendingOperation()=="catalog-verify"))
            fScan.operationRunner->MarkUnknown(scan.PendingOperationId(),reason.toStdString());
    } catch (...) {}
}
void NativeRunControl::RecordUnresolvedScanOperation(const QString &id,const QString &error) {
    RecordScanFinalizationUnknown(error.isEmpty() ?
        "Native scan receiver/catalog finalization outcome is unresolved" : error);
    if (id==fScan.runOperationId && fScan.operationRunner) {
        try { fScan.operationRunner->MarkUnknown(id.toStdString(),
            error.isEmpty() ? "Native scan operation outcome is unresolved" : error.toStdString()); }
        catch (...) {}
        fScan.runMonitoring=false;fScan.awaitingControllerState=0;
        emit ScanPointRunStatusSignal("Scan operation outcome is unresolved; inspect the persisted operation before continuing",false);
        if (!fScan.lifecycleCommand.isEmpty()) {
            fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
            fScan.lifecycleRecovery=false;
            fScan.runOperationResponse.clear(); fOperationId.clear(); fCommand.clear();
            fTarget=-1;
        }
    }
}
void NativeRunControl::HandleScanOperationDeadline() {
    RecordScanFinalizationUnknown("Native scan receiver/catalog finalization deadline expired");
    if (!fScan.lifecycleCommand.isEmpty() && fScan.operationRunner && !fScan.runOperationId.isEmpty()) {
        try { fScan.operationRunner->MarkUnknown(fScan.runOperationId.toStdString(),
            "Native scan lifecycle deadline expired before a confirmed result"); } catch (...) {}
        const QString command=fScan.lifecycleCommand;
        fScan.lifecycleCommand.clear(); fScan.runOperationId.clear();
        fScan.lifecycleRecovery=false;
        fScan.runOperationResponse.clear(); fScan.awaitingControllerState=0;
        emit ScanPointRunStatusSignal(tr("Whole-system %1 timed out; inspect the persisted intent before continuing.").arg(command),false);
    }
}

bool NativeRunControl::ConfirmScanFinalizationFromStatus(bool catalogued) {
    if (!fScan.pointFinalizationPending || !fScan.operationRunner || !fScan.runStore) return true;
    const GetDAQ::ScanExecutionJournal scan=fScan.runStore->Current();
    if(scan.State()==GetDAQ::ScanPointState::RunStopped &&
       scan.PendingOperation()=="receiver-finish" && !scan.PendingOperationUnknown()) {
        const QString evidenceError=GetDAQ::NativeReceiverFinalizationError(fSystem,fConfig,false);
        if(!evidenceError.isEmpty()) { Fail("Scan receiver-finish evidence is incomplete: "+evidenceError); return false; }
        fScan.operationRunner->Complete(scan.PendingOperationId(),fStatusBody.toStdString());
        fScan.runOperationId.clear();
    }
    const GetDAQ::ScanExecutionJournal afterReceiver=fScan.runStore->Current();
    if(afterReceiver.State()==GetDAQ::ScanPointState::ReceiverFinished &&
       !afterReceiver.HasPendingOperation()) {
        if(!catalogued) {
            const QString id="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-scan-catalog-verify";
            GetDAQ::EccControlRequest request;
            request.operationId=id.toStdString();request.command="catalog";
            request.instanceId=fSystem.instanceId;request.snapshotSha256=fSystem.snapshotSha256;
            request.clientId=fConfig.nativeClient.clientId.toStdString();
            request.clientEpoch=fConfig.nativeClient.clientEpoch.toStdString();
            request.catalogExperiment=fConfig.experiment.toStdString();
            request.catalogRunNumber=unsigned(fConfig.runNumber);
            fScan.operationRunner->PersistIntent(id.toStdString(),"catalog-verify",
                GetDAQ::EncodeEccControlRequest(request));
            fScan.runOperationId=id;
            fCommand.clear(); Send("catalog",3); return false;
        }
        Fail("A catalog receipt appeared before this scan point recorded its catalog intent; inspect the run and journal"); return false;
    }
    if(afterReceiver.State()==GetDAQ::ScanPointState::ReceiverFinished &&
       afterReceiver.PendingOperation()=="catalog-verify" && !afterReceiver.PendingOperationUnknown()) {
        if(!catalogued) {
            fScan.operationRunner->MarkUnknown(afterReceiver.PendingOperationId(),
                "Fresh ECC status did not expose the catalog receipt after the saved catalog operation");
            Fail("Scan catalog result is unresolved; the saved catalog intent was not replayed"); return false;
        }
        const QString evidenceError=GetDAQ::NativeReceiverFinalizationError(fSystem,fConfig,true);
        if(!evidenceError.isEmpty()) { Fail("Scan catalog evidence is incomplete: "+evidenceError); return false; }
        fScan.operationRunner->Complete(afterReceiver.PendingOperationId(),fStatusBody.toStdString());
        fScan.runOperationId.clear();
    }
    return true;
}
