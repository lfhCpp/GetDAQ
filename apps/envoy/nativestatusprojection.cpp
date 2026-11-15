#include "nativestatusprojection.h"
#include "nativestatusprojection_p.h"

#include <QCoreApplication>
#include <QMap>
#include <QSet>
#include <algorithm>
#include <stdexcept>

namespace {
QString Tr(const char *text) {
    return QCoreApplication::translate("NativeRunControl",text);
}
}

QVector<NodeStatus> BuildNativeSystemStatusRows(
    const RunConfig &config,
    const GetDAQ::EccControlSystemStatus &system,
    qint64 observationAgeMs,
    bool runActive)
{
    QVector<NodeStatus> rows;
    QMap<QString,const GetDAQ::EccControlReceiverStatus *> receivers;
    QMap<QString,QString> agents,states,catalogs;
    for(const auto &node:system.nodes)for(const auto &receiver:node.receivers) {
        const QString id=QString::fromStdString(receiver.id);
        receivers[id]=&receiver;agents[id]=QString::fromStdString(node.agentId);
        states[id]=QString::fromStdString(node.state);
        if(node.catalogVerified)catalogs[id]=QString::fromStdString(node.catalog.directory);
    }
    QSet<QString> expected;
    for(const auto &node:config.nodes) {
        NodeStatus row;
        row.id=node.id;row.state=int(system.controller.state);
        row.transition=int(system.controller.transition);row.eccOnline=true;
        row.eccTime=observationAgeMs;row.nativeHardwareOutcomeUnknown=system.controller.outcomeUnknown;
        if(system.controller.outcomeUnknown || system.controller.needsReconciliation || system.controlNeedsReconciliation)
            row.error=system.message.empty() ? QStringLiteral("UNKNOWN / reconciliation required") :
                QString::fromStdString(system.message);
        if(!node.nativeSenderId.isEmpty()) {
            expected.insert(node.nativeSenderId);
            const auto receiver=receivers.value(node.nativeSenderId,nullptr);
            if(receiver) {
                if(agents[node.nativeSenderId]!=node.nativeAgentId)
                    throw std::runtime_error("Native source mapped to a different data agent");
                row.sentryOnline=true;row.sentryTime=observationAgeMs;
                row.process=states[node.nativeSenderId]+QStringLiteral(" (cached)");
                row.nativeSessionId=QString::fromStdString(receiver->sessionId);
                row.dataPath=QString::fromStdString(receiver->outputDirectory);row.nativePid=receiver->pid;
                row.nativeReceivedBytes=QString::number(qulonglong(receiver->receivedBytes));
                row.nativeFrames=QString::number(qulonglong(receiver->frames));
                row.nativeFiles=QString::number(qulonglong(receiver->files));
                row.nativeReaped=receiver->reaped;row.nativeSealVerified=receiver->sealVerified;
                row.nativeSourceUnusedConfirmed=receiver->sourceUnusedConfirmed;
                row.nativeSourceEndConfirmed=receiver->sourceEndConfirmed;
                row.nativeCatalogDir=catalogs.value(node.nativeSenderId);
            } else row.process=QStringLiteral("No receiver observation yet");
        }
        rows.append(row);
    }
    const bool requireReceivers=system.controller.state>=4 || (runActive && system.controller.state==3);
    QSet<QString> observedReceivers;
    for(auto receiver=receivers.cbegin();receiver!=receivers.cend();++receiver)observedReceivers.insert(receiver.key());
    if(!system.operationActive && (requireReceivers || !receivers.isEmpty()) && observedReceivers!=expected)
        throw std::runtime_error("Native GUI source set differs from whole receiver set");
    return rows;
}

QVector<NodeStatus> ProjectNativeStatus(
    const RunConfig &config,
    const QVector<NodeStatus> &statusRows,
    const GetDAQ::EccControlSystemStatus &system,
    bool haveSystemStatus,
    qint64 controllerAgeMs,
    const GetDAQ::EccControlReceiverObservation &receivers,
    bool receiverFresh,
    const QString &receiverError,
    const GetDAQ::EccControlSourceObservation &sources,
    bool sourceFresh,
    const QString &sourceError,
    const GetDAQ::EccControlReceiverFailureStatus &receiverFailure,
    bool receiverFailureFresh,
    const QString &receiverFailureError)
{
    auto rows=statusRows;
    if(config.nativeClient.receiverStatusEnabled) for(auto &row:rows) {
        const NodeConfig *nodeConfig=nullptr;
        for(const auto &candidate:config.nodes) if(candidate.id==row.id) {nodeConfig=&candidate;break;}
        if(!nodeConfig || nodeConfig->nativeSenderId.isEmpty())continue;
        row.nativeReceiverObserved=false;row.nativeReceiverFault=false;
        row.process=Tr("Receiver observation unavailable");
        if(!receiverError.isEmpty()){row.nativeReceiverError=receiverError;continue;}
        if(!receiverFresh){row.nativeReceiverError=Tr("Receiver observation missing or stale");continue;}
        const GetDAQ::EccControlReceiverNodeObservation *agent=nullptr;
        for(const auto &candidate:receivers.nodes)
            if(QString::fromStdString(candidate.agentId)==nodeConfig->nativeAgentId){agent=&candidate;break;}
        if(!agent){row.nativeReceiverError=Tr("No observation for this data node");continue;}
        if(!agent->available){row.nativeReceiverError=QString::fromStdString(agent->message);continue;}
        if(agent->busy){row.process=Tr("Receiver manager busy; not observed");continue;}
        bool bound=true;
        for(const auto &candidate:system.nodes) if(candidate.agentId==agent->agentId)
            bound=candidate.planSha256==agent->planSha256 &&
                (candidate.instanceId.empty() || candidate.instanceId==agent->instanceId);
        if(!bound){row.nativeReceiverError=Tr("Data node identity differs from lifecycle cache");continue;}
        const GetDAQ::EccControlReceiverProcess *process=nullptr;
        for(const auto &candidate:agent->receivers)
            if(QString::fromStdString(candidate.id)==nodeConfig->nativeSenderId){process=&candidate;break;}
        if(!process) {
            row.process=Tr("No owned receiver in this observation");
            if(agent->lifecycle.needsReconciliation)
                row.nativeReceiverError=Tr("Data node requires reconciliation; empty list does not prove stopped");
            continue;
        }
        if(row.nativePid!=process->pid ||
           row.nativeSessionId!=QString::fromStdString(agent->lifecycle.sessionId)) {
            row.nativeReceiverError=Tr("Waiting for matching receiver lifecycle/session");continue;
        }
        row.nativeReceiverObserved=true;
        const auto state=QString::fromStdString(process->state);
        if(state=="running")row.process=Tr("Running (process observed)");
        else if(state=="exited" || state=="reaped") {
            row.nativeReceiverFault=process->exitCode!=0;
            row.process=row.nativeReceiverFault ? Tr("Receiver failed: exit %1").arg(process->exitCode) :
                state=="exited" ? Tr("Exited normally; Finish/seal pending") : Tr("Reaped; check seal");
            if(row.nativeReceiverFault)
                row.nativeReceiverError=Tr("Inspect receiver stderr and retained Run; no automatic restart");
        } else {
            row.nativeReceiverObserved=false;
            row.process=state=="not-started" ? Tr("Receiver not started") : Tr("Receiver ownership/status unavailable");
            row.nativeReceiverError=QString::fromStdString(process->message);
        }
    }
    if(config.nativeClient.sourceStatusEnabled)for(auto &row:rows) {
        const NodeConfig *nodeConfig=nullptr;
        for(const auto &candidate:config.nodes)if(candidate.id==row.id){nodeConfig=&candidate;break;}
        if(!nodeConfig || nodeConfig->nativeSenderId.isEmpty())continue;
        row.nativeSourceObserved=row.nativeSourceFault=false;
        row.nativeSourceState=Tr("Source not observed");
        if(!sourceError.isEmpty()){row.nativeSourceError=sourceError;continue;}
        if(!sourceFresh){row.nativeSourceError=Tr("Source observation missing or stale");continue;}
        if(sources.busy){row.nativeSourceState=Tr("Source observation busy");continue;}
        if(sources.controllerOperationId!=system.controller.operationId) {
            row.nativeSourceError=Tr("Waiting for matching controller operation");continue;
        }
        const GetDAQ::EccControlSourceObservation::Source *source=nullptr;
        for(const auto &candidate:sources.sources)
            if(QString::fromStdString(candidate.id)==nodeConfig->nativeSenderId){source=&candidate;break;}
        if(!source){row.nativeSourceState=Tr("No bound source");continue;}
        if(source->phase=="legacy"){row.nativeSourceState=Tr("Legacy source: state unavailable");continue;}
        if(source->phase!="available") {
            row.nativeSourceState=QString::fromStdString(source->phase);
            row.nativeSourceError=QString::fromStdString(source->message);continue;
        }
        if(row.nativeSessionId!=QString::fromStdString(source->receiverSession)) {
            row.nativeSourceError=Tr("Waiting for matching source/receiver session");continue;
        }
        row.nativeSourceObserved=true;
        row.nativeSourceFault=source->state=="unknown" || !source->error.empty() || source->localState==9;
        row.nativeSourceState=QString::fromStdString(source->state)+
            (source->localState==9 ? Tr(" / control UNKNOWN") : QString());
        row.nativeSourceError=QString::fromStdString(source->error);
    }
    NativeStatusProjectionInternal::ApplyReceiverFailureProjection(
        config,rows,system,haveSystemStatus,controllerAgeMs,receiverFailure,receiverFailureFresh,receiverFailureError);
    return rows;
}
