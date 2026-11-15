#include "scaneventcountobservation.h"

#include <stdexcept>

namespace GetDAQ {
namespace {
void Require(bool condition,const char *message) {
    if (!condition) throw std::runtime_error(message);
}
}

ScanEventCountBaseline BindScanEventCountObservation(
    const std::string &senderId,
    const std::string &responseBytes,
    bool systemStatusFresh,
    const EccControlSystemStatus &systemStatus,
    bool receiverStatusFresh,
    const EccControlReceiverObservation &receiverStatus,
    const ScanRuntimeIdentityExpectation &expected,
    const std::string &targetCoBo)
{
    Require(senderId==targetCoBo && !targetCoBo.empty(),
            "Fresh matching Native Running and receiver observations are required");
    Require(systemStatusFresh && receiverStatusFresh,
            "Fresh matching Native Running and receiver observations are required");
    Require(systemStatus.instanceId==expected.instanceId &&
            systemStatus.snapshotSha256==expected.snapshotSha256 &&
            systemStatus.controller.state==5 && systemStatus.controller.transition==0 &&
            !systemStatus.operationActive && !systemStatus.controller.outcomeUnknown &&
            !systemStatus.controller.needsReconciliation && !systemStatus.dataNeedsReconciliation,
            "Fresh matching Native Running and receiver observations are required");

    const EccControlReceiverEvents events=DecodeEccControlReceiverEvents(responseBytes);
    Require(events.instanceId==expected.instanceId &&
            events.snapshotSha256==expected.snapshotSha256 &&
            events.dataBindingSha256==expected.dataBindingSha256 &&
            events.sourceId==targetCoBo,
            "Receiver event response differs from this scan point deployment");
    Require(receiverStatus.instanceId==expected.instanceId &&
            receiverStatus.snapshotSha256==expected.snapshotSha256 &&
            receiverStatus.dataBindingSha256==expected.dataBindingSha256,
            "Receiver observation differs from this scan point deployment");

    const ScanRuntimeDataNodeExpectation *dataNode=nullptr;
    for (const auto &candidate:expected.dataNodes) if (candidate.agentId==events.agentId) {
        Require(dataNode==nullptr,"Receiver event response agent is ambiguous in this scan deployment");
        dataNode=&candidate;
    }
    Require(dataNode && dataNode->instanceId==events.receiverInstanceId &&
            dataNode->planSha256==events.planSha256,
            "Receiver event response differs from the frozen agent, instance or plan");

    const EccControlReceiverNodeObservation *observedNode=nullptr;
    for (const auto &candidate:receiverStatus.nodes) if (candidate.agentId==events.agentId) {
        Require(observedNode==nullptr,"Fresh receiver observation contains a duplicate scan agent");
        observedNode=&candidate;
    }
    Require(observedNode && observedNode->available && !observedNode->busy &&
            observedNode->instanceId==events.receiverInstanceId &&
            observedNode->planSha256==events.planSha256 &&
            observedNode->lifecycle.state=="ready" &&
            observedNode->lifecycle.sessionId==events.sessionId &&
            !observedNode->lifecycle.needsReconciliation,
            "Receiver event session differs from its fresh lifecycle observation");

    const EccControlReceiverProcess *process=nullptr;
    for (const auto &candidate:observedNode->receivers) if (candidate.id==events.sourceId) {
        Require(process==nullptr,"Selected scan receiver is duplicated in its lifecycle observation");
        process=&candidate;
    }
    Require(process && process->state=="running" && process->pid==events.pid,
            "Receiver event PID differs from the fresh running CoBo process");

    ScanEventCountBaseline observation;
    observation.instanceId=events.instanceId;
    observation.snapshotSha256=events.snapshotSha256;
    observation.dataBindingSha256=events.dataBindingSha256;
    observation.agentId=events.agentId;
    observation.planSha256=events.planSha256;
    observation.receiverInstanceId=events.receiverInstanceId;
    observation.sourceId=events.sourceId;
    observation.sessionId=events.sessionId;
    observation.pid=events.pid;
    observation.available=events.available;
    observation.acquisitionEvents=events.acquisitionEvents;
    observation.observedAtUnixMs=events.observedAtUnixMs;
    return observation;
}

}
