#include "nativeruncontrol.h"
#include "scaneventcountbaselinecoordinator.h"
#include "scaneventcountobservation.h"
#include "scanexecutionsnapshot.h"
#include "scanexecutionjournal.h"
#include "scanoperationrunner.h"
#include <QFileInfo>
#include <stdexcept>

void NativeRunControl::QueryScanEventCountBaseline(const QString &journalPath) {
    const auto reject=[this](const QString &message) {
        emit ScanEventCountBaselineBoundSignal({}, {}, message);
    };
    if(!fConnected || fShuttingDown || !fClient || fBusy || fActive || fDeferredProgressState) {
        reject("Scan event baseline requires an idle connected native controller");return;
    }
    if(!fScan.runtimeSnapshot || !fScan.runtimeExpectation ||
       QFileInfo(journalPath).absoluteFilePath()!=fScan.runtimeJournalPath) {
        reject("Scan event baseline requires the same point's successfully bound runtime ECC identity");return;
    }
    if(!fIdentityMatched || fIdentityBody.isEmpty() || !fIdentityClock.isValid() ||
       fIdentityClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs || !fHaveStatus ||
       !fStatusClock.isValid() || fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
       fSystem.controller.state!=4 || fSystem.controller.transition || fSystem.operationActive ||
       fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation ||
       !fConfig.nativeClient.receiverStatusEnabled ||
       !fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) ||
       !fObservations.ReceiverError().isEmpty() ||
       fObservations.Receivers().instanceId!=fSystem.instanceId || fObservations.Receivers().snapshotSha256!=fSystem.snapshotSha256 ||
       (fScan.runtimeExpectation && fObservations.Receivers().dataBindingSha256!=fScan.runtimeExpectation->dataBindingSha256) ||
       fClient->Busy() || fClient->Pending()) {
        reject("Fresh matched ECC Ready state and receiver observations are required for the event baseline");return;
    }
    try {
        const GetDAQ::ScanRuntimeIdentityExpectation &expected=*fScan.runtimeExpectation;
        const GetDAQ::ScanExecutionSnapshot &snapshot=*fScan.runtimeSnapshot;
        if(expected.pointIndex>=snapshot.Points().size() || expected.pointIndex!=fScan.identityPointIndex ||
           snapshot.TargetCoBo().empty())
            throw std::runtime_error("Cached scan point identity is incomplete or stale");

        const NodeConfig *sourceConfig=nullptr;
        for(const NodeConfig &node:fConfig.nodes)if(!node.mutant && node.nativeSenderId.toStdString()==snapshot.TargetCoBo()) {
            if(sourceConfig)throw std::runtime_error("Scan CoBo source maps to multiple configured receiver agents");
            sourceConfig=&node;
        }
        if(!sourceConfig)throw std::runtime_error("Scan CoBo source is absent from the fixed ECC deployment");

        const GetDAQ::ScanRuntimeDataNodeExpectation *expectedNode=nullptr;
        for(const auto &node:expected.dataNodes)if(node.agentId==sourceConfig->nativeAgentId.toStdString()) {
            if(expectedNode)throw std::runtime_error("Scan receiver agent has multiple runtime identity bindings");
            expectedNode=&node;
        }
        if(!expectedNode)throw std::runtime_error("Scan receiver agent is absent from the bound runtime identity");

        const GetDAQ::EccControlReceiverNodeObservation *observedNode=nullptr;
        for(const auto &node:fObservations.Receivers().nodes)if(node.agentId==expectedNode->agentId) {
            if(observedNode)throw std::runtime_error("Receiver agent is duplicated in the fresh ECC observation");
            observedNode=&node;
        }
        if(!observedNode || !observedNode->available || observedNode->busy ||
           observedNode->instanceId!=expectedNode->instanceId || observedNode->planSha256!=expectedNode->planSha256 ||
           observedNode->lifecycle.needsReconciliation || observedNode->lifecycle.planSha256!=expectedNode->planSha256 ||
           observedNode->lifecycle.sessionId.empty())
            throw std::runtime_error("Selected receiver is not a fresh, idle session in the fixed scan deployment");
        const GetDAQ::EccControlReceiverProcess *process=nullptr;
        for(const auto &candidate:observedNode->receivers)if(candidate.id==snapshot.TargetCoBo()) {
            if(process)throw std::runtime_error("Selected CoBo receiver is duplicated in the lifecycle observation");
            process=&candidate;
        }
        if(!process || process->state!="running" || process->pid<=0)
            throw std::runtime_error("Selected CoBo receiver process is not running before scan Run start");

        fScan.eventBaselineStore=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString());
        fScan.eventBaselineCoordinator.reset(new GetDAQ::ScanEventCountBaselineCoordinator(
            *fScan.eventBaselineStore,snapshot,expected));
        const std::string source=fScan.eventBaselineCoordinator->PrepareQuery();
        fBusy=true;fPhase="Checking scan receiver event baseline";Publish();
        if(!fClient->QueryReceiverEvents(QString::fromUtf8(source.data(),int(source.size()))) &&
           fScan.eventBaselineCoordinator)
            FinishScanEventCountBaselineQuery(QString::fromUtf8(source.data(),int(source.size())),{},
                "ECC client refused the read-only receiver event-count request");
    } catch(const std::exception &error) {
        fScan.eventBaselineCoordinator.reset();fScan.eventBaselineStore.reset();
        fBusy=false;fPhase="Native monitoring";
        reject(QString::fromUtf8(error.what()).left(512));
        Publish();
    }
}
void NativeRunControl::FinishScanEventCountBaselineQuery(const QString &senderId,
                                                          const QByteArray &responseBytes,
                                                          const QString &error) {
    if (fScan.runMonitoring) {
        FinishScanRunEventQuery(senderId,responseBytes,error);
        return;
    }
    if(!fScan.eventBaselineCoordinator)return;
    QString failure=error;QByteArray journalBytes;
    if(failure.isEmpty())try {
        if(!fConnected || fShuttingDown || !fClient || senderId.toStdString()!=
           fScan.runtimeSnapshot->TargetCoBo() || !fHaveStatus ||
           !fStatusClock.isValid() || fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
           !fIdentityMatched || !fIdentityClock.isValid() ||
           fIdentityClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
           fSystem.controller.state!=4 || fSystem.controller.transition || fSystem.operationActive ||
           fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation ||
           !fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) ||
           !fStatusClock.isValid() || fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
           !fObservations.ReceiverError().isEmpty() ||
           fObservations.Receivers().instanceId!=fSystem.instanceId ||
           fObservations.Receivers().snapshotSha256!=fSystem.snapshotSha256 ||
           fObservations.Receivers().dataBindingSha256!=fScan.runtimeExpectation->dataBindingSha256)
            throw std::runtime_error("ECC Ready state or selected receiver observation became stale during the event query");
        const GetDAQ::ScanExecutionJournal updated=
            fScan.eventBaselineCoordinator->BindResponse(responseBytes.toStdString(),fObservations.Receivers());
        journalBytes=QByteArray::fromStdString(updated.Encode());
        if (!fScan.thresholdManualOnly && fScan.eventsPerPoint && fScan.maximumDurationMs)
            fScan.eventMonitor.reset(new GetDAQ::ScanEventCountMonitor(
                updated.EventCountBaseline(),fScan.eventsPerPoint,fScan.maximumDurationMs));
    } catch(const std::exception &error) {failure=QString::fromUtf8(error.what()).left(512);}
    fScan.eventBaselineCoordinator.reset();fScan.eventBaselineStore.reset();fBusy=false;fPhase="Native monitoring";
    if(!failure.isEmpty())emit LogSignal("Scan receiver event baseline was not bound: "+failure);
    emit ScanEventCountBaselineBoundSignal(journalBytes,failure.isEmpty()?responseBytes:QByteArray(),failure);
    Publish();
}
