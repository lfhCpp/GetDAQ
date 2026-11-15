#include "nativeruncontrol.h"
#include "nativestatusprojection.h"
#include <QSet>
#include <stdexcept>

void NativeRunControl::Poll() {
    if (!fConnected || fShuttingDown || !fClient) return;
    // Receiver event reads are control-plane metadata too. Give the scan
    // monitor the idle client before starting this tick's routine status and
    // receiver refreshes; otherwise QueryStatus() makes Busy() true below and
    // the event counter is never polled while a point is Running.
    if (fScan.runMonitoring && fActive && fSystem.controller.state==5 &&
        fScan.eventMonitor && fHaveStatus &&
        fStatusClock.isValid() && fStatusClock.elapsed()<=fConfig.nativeClient.statusMaxAgeMs &&
        fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) &&
        fObservations.ReceiverError().isEmpty() && fScan.runtimeSnapshot && !fClient->Busy()) {
        if (fClient->QueryReceiverEvents(QString::fromStdString(fScan.runtimeSnapshot->TargetCoBo()))) {
            Publish();
            return;
        }
    }
    fClient->QueryStatus();
    // The metrics request uses the same authenticated metadata gate as source
    // observations. Do not race it with a routine receiver/source refresh.
    if(fClient->ReceiverMetricsPending()){Publish();return;}
    if (fConfig.nativeClient.receiverStatusEnabled) { fClient->QueryReceivers(); EmitStatus(); }
    if(fConfig.nativeClient.sourceStatusEnabled) {fClient->QuerySources();EmitStatus();}
    if(fConfig.nativeClient.receiverFailureStatusEnabled) {fClient->QueryReceiverFailure();EmitStatus();}
    if (fStore) {
        const auto pending=fStore->Pending();
        if (!pending.empty()) fClient->Observe(pending.front());
    }
    Publish();
}
void NativeRunControl::Status(const QByteArray &body,const QString &error) {
    if (!fConnected || fShuttingDown) return;
    if(error.startsWith("Pre-recovery ")){fHaveStatus=false;fAwaitingStatus=true;fDiscardStatus=false;fClient->QueryStatus();Publish();return;}
    if (!error.isEmpty()) { fHaveStatus=false; Fail(error); return; }
    if (fDiscardStatus) {
        // 此GET在操作完成前已发出，不能据其旧缓存安排下一条命令。
        fDiscardStatus=false; fClient->QueryStatus(); Publish(); return;
    }
    try {
        fSystem=GetDAQ::DecodeEccControlSystemStatus(body.toStdString());
        if(!fSystem.operationActive && !fSystem.controller.operationId.empty()) {
            const auto id=fSystem.controller.operationId;
            if(!fAutoObservedOperation.empty() && id!=fAutoObservedOperation && id!=fAutoOwnedOperation)
                emit ExternalOperationSignal(int(fSystem.controller.state));
            fAutoObservedOperation=id;
        }
        fStatusBody=body;fStatusClock.restart();fHaveStatus=true; fAwaitingStatus=false;
        if(fPlanRetirement.receiptAfterStatus && fPlanRetirement.complete && !fSystem.operationActive) {fPlanRetirement.receiptAfterStatus=false;InspectPlanRetirementReceipt();}
        fStatusRows=BuildNativeSystemStatusRows(fConfig,fSystem,fClock.elapsed(),fActive);
        EmitStatus();
        if (fPhase=="Native: checking fixed identity") { fBusy=false; fPhase=fStore->Pending().empty() ? "Native monitoring" : "Native pending intent: read-only observation"; }
        if (fConfig.nativeClient.alarmStatusEnabled) fClient->QueryAlarms();
        if (fScan.awaitingControllerState) ConfirmScanOperationControllerState();
        ContinueFromStatus();
        Publish();
    } catch (const std::exception &e) { fHaveStatus=false; Fail(QString::fromUtf8(e.what())); }
}
void NativeRunControl::Receivers(const QByteArray &body,const QString &error) {
    if (!fConnected || fShuttingDown) return;
    QSet<QString> expected;
    for(const auto &node:fConfig.nodes)if(!node.nativeSenderId.isEmpty())expected.insert(node.nativeAgentId);
    const NativeObservationUpdate update=fObservations.ObserveReceivers(body,error,expected);
    if(!update.error.isEmpty() && update.errorChanged)
        emit LogSignal("Receiver observation unavailable: "+update.error);
    EmitStatus();
}
void NativeRunControl::Sources(const QByteArray &body,const QString &error) {
    if(!fConnected || fShuttingDown)return;
    const NativeObservationUpdate update=fObservations.ObserveSources(body,error);
    if(!update.error.isEmpty() && update.errorChanged)
        emit LogSignal("Source observation unavailable: "+update.error);
    EmitStatus();
}
void NativeRunControl::EmitStatus() {
    const bool receiverFresh=fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs);
    const bool sourceFresh=fObservations.SourcesFresh(fConfig.nativeClient.statusMaxAgeMs);
    const bool receiverFailureFresh=fObservations.ReceiverFailureFresh(fConfig.nativeClient.statusMaxAgeMs);
    const qint64 controllerAgeMs=fClock.isValid() ? fClock.elapsed() : -1;
    const auto rows=ProjectNativeStatus(
        fConfig,fStatusRows,fSystem,fHaveStatus,controllerAgeMs,
        fObservations.Receivers(),receiverFresh,fObservations.ReceiverError(),
        fObservations.Sources(),sourceFresh,fObservations.SourceError(),
        fObservations.ReceiverFailure(),receiverFailureFresh,fObservations.ReceiverFailureError());
    emit StatusSignal(rows);
}
void NativeRunControl::ContinueFromStatus() {
    if(fDeferredProgressState) {
        const bool expired=!fDeferredProgressClock.isValid() || fDeferredProgressClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs;
        const bool changed=fHaveStatus && (fSystem.controller.state!=fDeferredProgressState || fSystem.controller.operationId!=fDeferredProgressOperation ||
            fSystem.operationActive || fSystem.controlNeedsReconciliation || fSystem.controller.outcomeUnknown || fSystem.controller.needsReconciliation || fSystem.dataNeedsReconciliation);
        if(expired || changed){fDeferredProgressState=0;emit LogSignal("Pending manual Progress canceled: status changed or expired; inspect before clicking again");}
        else if(Available()){const auto state=fDeferredProgressState;fDeferredProgressState=0;
            Send(state==1 ? "describe" : state==2 ? "prepare" : "configure",int(state)+1);return;}
    }

    ContinueRunFinalizationFromStatus();
}
void NativeRunControl::ReceiverFailure(const QByteArray &body,const QString &error) {
    if (!fConnected || fShuttingDown) return;
    const NativeObservationUpdate update=fObservations.ObserveReceiverFailure(body,error);
    if(update.error.isEmpty()) {
        const GetDAQ::EccControlReceiverFailureStatus &policy=fObservations.ReceiverFailure();
        if (policy.latched && !fBusy && !fClient->Pending()) fPhase="Native requires inspection";
        if (update.bodyChanged) {
            if (policy.latched) {
                emit LogSignal("Receiver failure stop: "+QString::fromStdString(policy.phase)+"; original operation "+
                    QString::fromStdString(policy.operationId)+"; data and Run still require inspection");
                if (fActive) {
                    fJournal["native_receiver_failure_cache"]=QString::fromUtf8(body);
                    fJournal["duration_incomplete"]=true;
                    if (!Journal("Receiver failure held; Run incomplete"))
                        emit LogSignal("Cannot save receiver failure evidence; Run remains incomplete");
                }
            }
        }
    } else {
        if(update.errorChanged) emit LogSignal("Receiver failure cache unavailable: "+update.error);
    }
    EmitStatus(); ContinueFromStatus(); Publish();
}
void NativeRunControl::QueryStatusAfterOperation() {
    fHaveStatus=false; fAwaitingStatus=true;
    // 已有GET/告警读取时不能立即发出新GET；丢弃下一份状态，再明确查询。
    fDiscardStatus=!fClient->QueryStatus();
}
void NativeRunControl::Alarms(const QByteArray &body,const QString &error) {
    if (!fConnected || fShuttingDown) return;
    const NativeObservationUpdate update=fObservations.ObserveAlarms(body,error);
    if (!error.isEmpty()) { fScan.recoveryControlHoldOperationId.clear(); if(fRecovery.awaitingMetadata && !error.startsWith("Pre-recovery ")){fRecovery.awaitingMetadata=false;Fail("Recovery metadata requires inspection: "+error);return;} emit LogSignal("Native alarm cache unavailable: "+error+"; inspect before commands"); Publish(); return; }
    if(!update.error.isEmpty()) {emit LogSignal(update.error);Publish();return;}
    try {
        const auto &alarm=fObservations.Alarms();
        if ((alarm.latched || alarm.stopRequested) && !fBusy && !fClient->Pending()) fPhase="Native requires inspection";
        if (update.bodyChanged) {
            if (alarm.latched || alarm.stopRequested) {
                emit LogSignal("ECC alarm held: "+QString::fromStdString(alarm.phase)+"; original stop ID "+QString::fromStdString(alarm.operationId)+"; recovery required");
                for (const auto &node:alarm.nodes) if (node.stopRequested) emit LogSignal(QString::fromStdString(node.id)+": alarm mask "+QString::number(node.alarmMask)+
                    ", received "+QString::number(qulonglong(node.received))+", durable "+QString::number(qulonglong(node.durable))+
                    ", lost "+QString::number(qulonglong(node.lost))+"; cached original observation");
                if (fActive) {
                    fJournal["native_alarm_cache"]=QString::fromUtf8(body);
                    if (!Journal("Native alarm held; inspect original stop and receiver records")) emit LogSignal("Cannot persist local alarm observation; Run remains incomplete");
                }
            }
        }
        if (!fScan.recoveryControlHoldOperationId.isEmpty()) {
            const QString operationId=fScan.recoveryControlHoldOperationId;
            fScan.recoveryControlHoldOperationId.clear();
            if (fClient->CompleteScanLifecycleObservation(operationId)) fClient->QueryIdentity();
            else emit LogSignal("The scan intent was reconciled, but an independent Native control hold remains latched");
        }
        ContinueFromStatus(); Publish();
    } catch (const std::exception &e) { emit LogSignal(QString::fromUtf8(e.what())); Publish(); }
}
