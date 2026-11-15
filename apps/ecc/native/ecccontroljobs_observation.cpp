#include "ecccontroljobs_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace EccControlJobsInternal {
using namespace SupervisorIO;
std::string AlarmObservation(const NativeEccAlarms &observation) {
    std::string nodes="[";
    for (const auto &node:observation.nodes) {
        const auto &s=node.snapshot; if (nodes.back()!='[') nodes+=',';
        nodes+="{\"id\":"+EncodeJsonString(node.id)+",\"subscription\":"+std::to_string(unsigned(s.subscription))+
            ",\"received\":"+EncodeJsonString(std::to_string(s.received))+",\"durable\":"+EncodeJsonString(std::to_string(s.durable))+
            ",\"lost\":"+EncodeJsonString(std::to_string(s.lost))+",\"invalid\":"+EncodeJsonString(std::to_string(s.invalid))+
            ",\"pending\":"+std::to_string(s.pending)+",\"alarm_mask\":"+std::to_string(s.alarmMask)+
            ",\"stop_requested\":"+(s.stopRequested ? "true" : "false")+",\"queue_overflow\":"+(s.queueOverflow ? "true" : "false")+
            ",\"audit_failed\":"+(s.auditFailed ? "true" : "false")+",\"budget_exceeded\":"+(s.budgetExceeded ? "true" : "false")+"}";
    }
    return "{\"controller_state\":"+std::to_string(int(observation.controller.state))+
        ",\"controller_outcome_unknown\":"+(observation.controller.outcomeUnknown ? "true" : "false")+
        ",\"controller_operation_id\":"+EncodeJsonString(observation.controller.operationId)+
        ",\"stop_requested\":"+(observation.stopRequested ? "true" : "false")+",\"nodes\":"+nodes+"]}";
}
}
std::string EccControlJobs::Impl::ControllerJson() const {
    auto s=controller.Status();if(inputFault){s.outcomeUnknown=s.needsReconciliation=true;s.message="External fixed input fault held; original hardware/data state requires reconciliation";}return "{\"state\":"+std::to_string(int(s.state))+",\"transition\":"+std::to_string(s.transition)+",\"error\":"+std::to_string(s.error)+
        ",\"outcome_unknown\":"+(s.outcomeUnknown ? "true" : "false")+",\"needs_reconciliation\":"+(s.needsReconciliation ? "true" : "false")+
        ",\"operation_id\":"+EncodeJsonString(s.operationId)+",\"message\":"+EncodeJsonString(s.message)+"}";
}
std::vector<const EccControlOperation *> EccControlJobs::Impl::OrderedOperations() const {
    std::vector<const EccControlOperation *> ordered;ordered.reserve(operationOrder.size());
    for(const auto &id:operationOrder) {const auto found=jobs.find(id);if(found!=jobs.end())ordered.push_back(&found->second);}
    return ordered;
}
EccControlControllerStatus EccControlJobs::Impl::ProjectionControllerStatus() const {
    const auto status=controller.Status();EccControlControllerStatus projected;
    projected.state=static_cast<unsigned int>(status.state);
    projected.transition=static_cast<unsigned int>(status.transition);projected.outcomeUnknown=status.outcomeUnknown;
    projected.needsReconciliation=status.needsReconciliation;return projected;
}
void EccControlJobs::Impl::ProjectTarget(EccControlMaintenanceTarget &target) const {
    const auto ordered=OrderedOperations();const auto projected=ProjectionControllerStatus();
    ProjectCoBoLifecycle(target,projected,ordered);
}
void EccControlJobs::Impl::ProjectTargets(std::vector<EccControlMaintenanceTarget> &targets) const {
    const auto ordered=OrderedOperations();const auto projected=ProjectionControllerStatus();
    ProjectCoBoLifecycles(targets,projected,ordered);
}
std::string EccControlJobs::Impl::ProjectedTargetLifecycle(const std::string &id) const {
    EccControlMaintenanceTarget target;target.id=id;ProjectTarget(target);return target.lifecycleState;
}

std::string EccControlJobs::ReceiverFailureJson() const {
    const auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.stateMutex);
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-failure\",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"enabled\":"+(p.configuration.ReceiverStopEnabled() ? "true" : "false")+
        ",\"latched\":"+(p.configuration.ReceiverStopEnabled() && p.receiverHandled ? "true" : "false")+
        ",\"operation_id\":"+EncodeJsonString(p.receiverOperationId)+",\"phase\":"+EncodeJsonString(p.receiverPhase)+",\"evidence\":"+p.receiverEvidence+"}\n";
}
std::string EccControlJobs::AlarmsJson() const {
    const auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.stateMutex);
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-alarms\",\"instance_id\":"+EncodeJsonString(p.instance)+",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+
        ",\"enabled\":"+(p.configuration.AlarmStopEnabled() ? "true" : "false")+",\"latched\":"+(p.configuration.AlarmStopEnabled() && p.alarmHandled ? "true" : "false")+
        ",\"operation_id\":"+EncodeJsonString(p.alarmOperationId)+",\"phase\":"+EncodeJsonString(p.alarmPhase)+",\"observation\":"+p.alarmObservation+"}\n";
}
std::string EccControlJobs::IdentityJson() const {
    const auto &p=*fImpl; return "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":"+EncodeJsonString(p.configuration.ServiceId())+",\"client_id\":"+EncodeJsonString(p.configuration.ClientId())+
        ",\"instance_id\":"+EncodeJsonString(p.instance)+",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"input_binding_sha256\":"+EncodeJsonString(p.configuration.BindingSha256())+
        ",\"configuration_sha256\":"+EncodeJsonString(p.controller.ConfigurationSha256())+",\"artifact_sha256\":"+EncodeJsonString(p.controller.ArtifactSha256())+",\"data_binding_sha256\":"+EncodeJsonString(p.data.BindingSha256())+"}\n";
}
std::string EccControlJobs::ControlHandoffJson() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("ECC control handoff observation busy; not queued");
    p.Check();
    {
        std::lock_guard<std::mutex> lock(p.stateMutex);
        const auto found=p.jobs.find(p.handoffOperationId);
        bool complete=found!=p.jobs.end() && found->second.phase=="complete";
        if(!complete && found!=p.jobs.end() && found->second.phase=="failed" && !p.handoffCompletionOperationId.empty()) {
            const auto completion=p.jobs.find(p.handoffCompletionOperationId);
            complete=completion!=p.jobs.end() && completion->second.phase=="complete" && completion->second.request.command=="complete-data-handoff" &&
                completion->second.request.failedOperationId==p.handoffOperationId && completion->second.request.instanceId==p.instance;
        }
        if (!p.handoffFenced || found==p.jobs.end() || !complete ||
            found->second.request.instanceId!=p.instance || (found->second.request.command!="handoff-data-control" && found->second.request.command!="retire-data-plans"))
            throw std::runtime_error("No completed handoff from this original live ECC instance");
    }
    return p.data.CompletedControlHandoffJson();
}

std::string EccControlJobs::ConfigurationJson() const {
    const auto &p=*fImpl;
    EccControlConfiguration c;c.instanceId=p.instance;c.snapshotSha256=p.snapshot;c.inputBindingSha256=p.configuration.BindingSha256();
    c.configurationSha256=p.controller.ConfigurationSha256();c.artifactSha256=p.controller.ArtifactSha256();
    const auto selection=p.configuration.SelectionJson();if (selection!="null") {c.selection=DecodeConfigurationSelection(selection);c.hasSelection=true;}
    return EncodeEccControlConfiguration(c);
}
std::string EccControlJobs::MaintenanceTargetsJson() const {
    const auto &p=*fImpl;p.configuration.Check();
    EccControlMaintenanceTargets targets;targets.instanceId=p.instance;targets.snapshotSha256=p.snapshot;
    targets.configurationSha256=p.controller.ConfigurationSha256();targets.artifactSha256=p.controller.ArtifactSha256();
    std::lock_guard<std::mutex> lock(p.stateMutex);
    for(const auto &node:p.controller.MaintenanceTargets()) {EccControlMaintenanceTarget target;target.id=node.first;target.nodeName=node.second;targets.cobos.push_back(target);}
    p.ProjectTargets(targets.cobos);
    return EncodeEccControlMaintenanceTargets(targets);
}
std::string EccControlJobs::OwnershipJson() const {
    const auto &p=*fImpl;
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-ownership\",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"ownership\":"+p.controller.OwnershipJson()+"}\n";
}
std::string EccControlJobs::ConfigureReadbacksJson() const {
    const auto &p=*fImpl;
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-configure-readbacks\",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"configuration_sha256\":"+EncodeJsonString(p.controller.ConfigurationSha256())+
        ",\"artifact_sha256\":"+EncodeJsonString(p.controller.ArtifactSha256())+",\"controller\":"+p.ControllerJson()+
        ",\"readbacks\":"+p.controller.ConfigureReadbacksJson()+"}\n";
}
std::string EccControlJobs::ReceiversJson() const {
    const auto &p=*fImpl;
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"data_binding_sha256\":"+EncodeJsonString(p.data.BindingSha256())+
        ",\"nodes\":"+p.data.ObserveReceiversJson()+"}\n";
}
std::string EccControlJobs::ReceiverEventsJson(const std::string &senderId,bool fileBytes) const {
    const auto &p=*fImpl;
    return "{\"schema\":1,\"kind\":"+EncodeJsonString(fileBytes ? "getdaq.ecc-receiver-file-bytes" : "getdaq.ecc-receiver-events")+",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+",\"data_binding_sha256\":"+EncodeJsonString(p.data.BindingSha256())+
        ",\"events\":"+p.data.ObserveAcquisitionEventsJson(senderId,fileBytes)+"}\n";
}
std::string EccControlJobs::SourcesJson() const {
    const auto &p=*fImpl;const auto json=p.controller.ObserveSourcesJson();
    return "{\"instance_id\":"+EncodeJsonString(p.instance)+','+json.substr(1)+'\n';
}
std::string EccControlJobs::HardwareStateJson() const {
    const auto &p=*fImpl;
    // Input checks are read-only and latch their own fault. Do not take the
    // operation lock or inspect its mutable request/journal containers here.
    p.configuration.Check();
    const auto json=p.controller.ObserveHardwareStateJson();
    p.configuration.Check();
    return "{\"instance_id\":"+EncodeJsonString(p.instance)+','+json.substr(1)+'\n';
}
std::string EccControlJobs::StatusJson() const {
    return fImpl->StatusJsonForPolicy();
}
std::string EccControlJobs::Impl::StatusJsonForPolicy() const {
    const auto &p=*this; std::string control;
    { std::lock_guard<std::mutex> lock(p.stateMutex); control="\"operation_active\":"+std::string(p.active ? "true" : "false")+",\"operation_id\":"+EncodeJsonString(p.activeId)+",\"client_epoch\":"+EncodeJsonString(p.ownerEpoch)+
        ",\"control_needs_reconciliation\":"+(p.needsReconciliation ? "true" : "false")+",\"message\":"+EncodeJsonString(p.message); }
    const auto data=p.data.Status(); std::string nodes="[";
    for (const auto &node:data.nodes) {
        if (nodes.back()!='[') nodes+=',';
        nodes+="{\"agent_id\":"+EncodeJsonString(node.agentId)+",\"instance_id\":"+EncodeJsonString(node.instanceId)+",\"plan_sha256\":"+EncodeJsonString(node.planSha256)+
            ",\"lifecycle\":"+EncodeSupervisorStatus(node.data)+(node.catalogJson.empty() ? "" : ",\"catalog\":"+node.catalogJson)+"}";
    }
    nodes+=']';
    return "{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":"+EncodeJsonString(p.instance)+",\"snapshot_sha256\":"+EncodeJsonString(p.snapshot)+','+control+",\"controller\":"+p.ControllerJson()+
        ",\"data\":{\"needs_reconciliation\":"+(data.needsReconciliation ? "true" : "false")+",\"controller_scope_sha256\":"+EncodeJsonString(data.controllerScopeSha256)+
        ",\"controller_epoch\":"+EncodeJsonString(data.controllerEpoch)+",\"message\":"+EncodeJsonString(data.message)+",\"nodes\":"+nodes+"}}\n";
}

} // namespace GetDAQ
