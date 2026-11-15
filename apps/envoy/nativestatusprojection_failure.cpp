#include "nativestatusprojection_p.h"
#include <QCoreApplication>

namespace {
QString Tr(const char *text) {
    return QCoreApplication::translate("NativeRunControl",text);
}
}

namespace NativeStatusProjectionInternal {
void ApplyReceiverFailureProjection(
    const RunConfig &config,
    QVector<NodeStatus> &rows,
    const GetDAQ::EccControlSystemStatus &system,
    bool haveSystemStatus,
    qint64 controllerAgeMs,
    const GetDAQ::EccControlReceiverFailureStatus &receiverFailure,
    bool receiverFailureFresh,
    const QString &receiverFailureError)
{
    if(config.nativeClient.receiverFailureStatusEnabled)for(auto &row:rows) {
        const NodeConfig *nodeConfig=nullptr;
        for(const auto &candidate:config.nodes)if(candidate.id==row.id){nodeConfig=&candidate;break;}
        if(!nodeConfig || nodeConfig->nativeSenderId.isEmpty())continue;
        row.nativeReceiverFailurePhase=Tr("Not observed");
        row.nativeReceiverFailureDetail.clear();row.nativeReceiverFailureOperation.clear();
        row.nativeReceiverFailureObserved=row.nativeReceiverFailureHeld=row.nativeReceiverFailureAffected=false;
        if(!receiverFailureError.isEmpty()){row.nativeReceiverFailureDetail=receiverFailureError;continue;}
        if(!receiverFailureFresh) {
            row.nativeReceiverFailureDetail=Tr("Receiver failure cache missing or stale");continue;
        }
        row.nativeReceiverFailureObserved=true;row.nativeReceiverFailureHeld=receiverFailure.latched;
        row.nativeReceiverFailurePhase=QString::fromStdString(receiverFailure.phase);
        row.nativeReceiverFailureOperation=QString::fromStdString(receiverFailure.operationId);
        if(receiverFailure.latched)
            row.nativeReceiverFailureDetail=Tr("Production stop result only; data and Run require inspection.");
        if(receiverFailure.hasEvidence) {
            row.nativeReceiverFailureDetail+=Tr(" Trigger controller operation: ")+
                QString::fromStdString(receiverFailure.bound.controller.operationId)+".";
            for(const auto &agent:receiverFailure.observation.nodes)
                if(QString::fromStdString(agent.agentId)==nodeConfig->nativeAgentId)
                    for(const auto &process:agent.receivers)
                        if(QString::fromStdString(process.id)==nodeConfig->nativeSenderId &&
                           std::find(receiverFailure.failedIds.begin(),receiverFailure.failedIds.end(),process.id)!=receiverFailure.failedIds.end()) {
                            bool sameNode=false;
                            for(const auto &cached:system.nodes)if(cached.agentId==agent.agentId)
                                sameNode=cached.instanceId==agent.instanceId && cached.planSha256==agent.planSha256;
                            const bool current=haveSystemStatus && row.eccTime>=0 &&
                                controllerAgeMs-row.eccTime<=config.nativeClient.statusMaxAgeMs && sameNode &&
                                receiverFailure.bound.instanceId==system.instanceId &&
                                row.nativeSessionId==QString::fromStdString(agent.lifecycle.sessionId) &&
                                row.nativePid==process.pid;
                            row.nativeReceiverFailureAffected=current;
                            row.nativeReceiverFailureDetail+=Tr(" Trigger receiver: ")+
                                QString::fromStdString(process.id)+Tr(", PID ")+QString::number(process.pid)+
                                Tr(", exit ")+QString::number(process.exitCode)+Tr(", signal ")+QString::number(process.signal)+
                                Tr(", session ")+QString::fromStdString(agent.lifecycle.sessionId)+
                                (current ? Tr(" (current retained session).") : Tr(" (historical session; not current process evidence)."));
                        }
        }
    }
}
}
