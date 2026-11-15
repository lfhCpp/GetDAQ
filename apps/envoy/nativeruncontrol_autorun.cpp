#include "nativeruncontrol.h"
#include <QDateTime>
#include <stdexcept>

AutoRunObservation NativeRunControl::AutomationObservation() const {
    AutoRunObservation v;v.connected=fConnected;v.active=fActive;
    v.busy=fBusy || fAwaitingStatus || fDeferredProgressState || !fClient || fClient->Busy() ||
        (fStore && !fStore->Pending().empty()) || (fHaveStatus && fSystem.operationActive);
    v.healthy=!fScan.runMonitoring && fHaveStatus && fStatusClock.isValid() && fStatusClock.elapsed()<=fConfig.nativeClient.statusMaxAgeMs &&
        fPhase!="Native requires inspection" && !fSystem.controlNeedsReconciliation && !fSystem.dataNeedsReconciliation &&
        !fSystem.controller.outcomeUnknown && !fSystem.controller.needsReconciliation && !fSystem.controller.error && fClient && fStore && !fStore->NeedsReconciliation();
    v.canControl=Available() && !v.busy;
    v.running=fHaveStatus && fSystem.controller.state==5;v.paused=fHaveStatus && fSystem.controller.state==6;
    v.ready=fHaveStatus && fSystem.controller.state==4;v.prepared=fHaveStatus && fSystem.controller.state==3;
    v.bytesKnown=fConfig.nativeClient.receiverStatusEnabled;
    for(const auto &n:fConfig.nodes) {
        if(n.nativeSenderId.isEmpty())continue;
        const auto source=n.nativeSenderId;
        const GetDAQ::EccControlReceiverStatus *bound=nullptr;
        for(const auto &node:fSystem.nodes)for(const auto &r:node.receivers)
            if(QString::fromStdString(node.agentId)==n.nativeAgentId && QString::fromStdString(r.id)==source)bound=&r;
        if(!bound || bound->pid<=0 || bound->reaped || bound->sealVerified){v.bytesKnown=false;continue;}
        const QString key=n.nativeAgentId+":"+source+":"+QString::fromStdString(bound->sessionId)+":"+QString::number(bound->pid);
        // Start already requires verified empty Ready receivers. Seed the new
        // session at zero so the first poll's data is included in the threshold.
        if(v.ready && !v.active && !bound->receivedBytes && !bound->files){v.fileBytes.insert(key,0);continue;}
        const auto found=fAutoFileBytes.constFind(source);
        if(found==fAutoFileBytes.cend() || found->sessionId!=bound->sessionId || found->pid!=bound->pid ||
           fClock.elapsed()-fAutoFileTimes.value(source,-1000000000)>fConfig.nativeClient.statusMaxAgeMs) {v.bytesKnown=false;continue;}
        v.fileBytes.insert(key,found->fileBytes);
    }
    v.bytesKnown=v.bytesKnown && !v.fileBytes.isEmpty();return v;
}
void NativeRunControl::RequestAutomaticFileBytes() {
    if(!fClient || !fConfig.nativeClient.receiverStatusEnabled || !fActive || fScan.runMonitoring ||
       !fHaveStatus || (fSystem.controller.state!=5 && fSystem.controller.state!=6) || fBusy || fClient->Busy())return;
    QStringList ids;for(const auto &n:fConfig.nodes)if(!n.nativeSenderId.isEmpty())ids.append(n.nativeSenderId);
    if(ids.isEmpty())return;
    const auto source=ids[fAutoFileIndex%ids.size()];
    if(fClock.elapsed()-fAutoFileTimes.value(source,-1000000000)<qMin(1000,fConfig.pollMs))return;
    if(fClient->QueryReceiverEvents(source,true))fAutoFileIndex=(fAutoFileIndex+1)%ids.size();
}
void NativeRunControl::AutomaticFileBytes(const QString &source,const QByteArray &body,const QString &error) {
    try {
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        const auto value=GetDAQ::DecodeEccControlReceiverEvents(body.toStdString(),true);
        if(!value.available)throw std::runtime_error("Receiver does not provide raw-file byte counts");
        const GetDAQ::EccControlReceiverStatus *bound=nullptr;QString agent;
        for(const auto &node:fSystem.nodes)for(const auto &r:node.receivers)
            if(QString::fromStdString(r.id)==source){bound=&r;agent=QString::fromStdString(node.agentId);}
        if(!bound || bound->reaped || bound->pid!=value.pid || bound->sessionId!=value.sessionId ||
           agent!=QString::fromStdString(value.agentId))throw std::runtime_error("Raw-file counter differs from the retained receiver source/session/PID");
        const auto old=fAutoFileBytes.constFind(source);
        if(old!=fAutoFileBytes.cend() && old->sessionId==value.sessionId &&
           (value.observedAtUnixMs<=old->observedAtUnixMs || value.fileBytes<old->fileBytes))
            throw std::runtime_error("Raw-file observation did not advance or its counter decreased");
        fAutoFileBytes.insert(source,value);fAutoFileTimes.insert(source,fClock.elapsed());
    } catch(const std::exception &e) {
        fAutoFileBytes.remove(source);fAutoFileTimes.remove(source);
        emit AutoRunObservationErrorSignal(QString("Automatic Run raw-file size unavailable: %1. Check ECC/supervisor support and receiver observations.").arg(QString::fromUtf8(e.what())));
    }
}
