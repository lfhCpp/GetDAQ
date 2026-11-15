#include "controljobs_p.h"
#include "receivercatalog.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <mutex>
#include <stdexcept>
#include <utility>

namespace GetDAQ {
using namespace SupervisorIO;
std::string SupervisorControl::InstanceId() const { return fImpl->instance; }
std::string SupervisorControl::PlanSha256() const { return fImpl->planHash; }
std::string SupervisorControl::PlanRetirementJson() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operationMutex,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Original plan retirement observation busy");
    p.CheckStore();
    {
        std::lock_guard<std::mutex> state(p.stateMutex);
        const auto id=p.planRetirementCompletionOperation.empty() ? p.planRetirementOperation : p.planRetirementCompletionOperation;
        const auto found=p.jobs.find(id);
        if (!p.planRetired || found==p.jobs.end() || found->second.phase!="complete" || found->second.request.instanceId!=p.instance)
            throw std::runtime_error("No complete retirement from this original live data service");
    }
    return ReadFile(p.directory+"/operations/"+p.planRetirementOperation+"/plan-retirement.json",4*1024*1024);
}
std::string SupervisorControl::PlanRetirementCompletionJson() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operationMutex,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Retirement completion observation busy");
    p.CheckStore();
    std::lock_guard<std::mutex> state(p.stateMutex);
    const auto original=p.jobs.find(p.planRetirementOperation),completion=p.jobs.find(p.planRetirementCompletionOperation);
    if (!p.planRetired || original==p.jobs.end() || original->second.phase!="failed" ||
        completion==p.jobs.end() || completion->second.phase!="complete" ||
        completion->second.request.command!=SupervisorCommand::CompletePlanRetirement ||
        completion->second.request.instanceId!=p.instance ||
        EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(completion->second.request))!=EncodeSupervisorControlRequest(original->second.request))
        throw std::runtime_error("No explicit completed retirement repair from this original live data service");
    return "{\"schema\":1,\"kind\":\"getdaq.supervisor-plan-retirement-completion\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+
        ",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(completion->second.request))+
        ",\"operation\":"+EncodeSupervisorControlOperation(completion->second)+"}\n";
}
std::string SupervisorControl::ControlHandoffCompletionJson() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operationMutex,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Control handoff completion observation busy");
    p.CheckStore();std::lock_guard<std::mutex> state(p.stateMutex);
    const SupervisorControlOperation *original=nullptr,*completion=nullptr;
    for (const auto &entry:p.jobs) {
        const auto &op=entry.second;
        if (op.request.command!=SupervisorCommand::CompleteControlHandoff || op.phase!="complete" || op.request.instanceId!=p.instance) continue;
        const auto found=p.jobs.find(op.request.failedHandoffOperationId);
        if (found==p.jobs.end() || found->second.request.command!=SupervisorCommand::HandoffControl || found->second.phase!="failed" ||
            found->second.request.instanceId!=p.instance ||
            EncodeSupervisorControlRequest(OriginalControlHandoffRequest(op.request))!=EncodeSupervisorControlRequest(found->second.request)) continue;
        if (completion) throw std::runtime_error("More than one completed live handoff repair is ambiguous");
        completion=&op;original=&found->second;
    }
    if (!original || !completion)
        throw std::runtime_error("No exact explicitly completed failed handoff from this live data service");
    const auto proof=p.handoffProofs.find(original->request.operationId);
    if (proof==p.handoffProofs.end()) throw std::runtime_error("Completed live handoff proof is not bound to the original operation");
    proof->second->Check();
    const auto bytes=ReadFile(p.directory+"/operations/"+original->request.operationId+"/handoff-proof.json",1024*1024);
    if (ConfigurationSha256(bytes)!=completion->handoffProofSha256 || completion->handoffProofSha256.empty())
        throw std::runtime_error("Completed live handoff proof differs from its operation digest");
    const auto oldStatus=DecodeSupervisorStatus(ParseStrictJson(original->resultStatusJson));
    const auto completedStatus=DecodeSupervisorStatus(ParseStrictJson(completion->resultStatusJson));
    if (oldStatus.state!=DataSupervisorStatus::Stopped || oldStatus.needsReconciliation ||
        EncodeSupervisorStatus(oldStatus)!=EncodeSupervisorStatus(completedStatus))
        throw std::runtime_error("Completed handoff does not preserve the original normally sealed cycle");
    return "{\"schema\":1,\"kind\":\"getdaq.supervisor-control-handoff-completion\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+
        ",\"original_request\":"+EncodeJsonString(EncodeSupervisorControlRequest(original->request))+
        ",\"original_operation\":"+EncodeSupervisorControlOperation(*original)+
        ",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(completion->request))+
        ",\"operation\":"+EncodeSupervisorControlOperation(*completion)+",\"proof\":"+EncodeJsonString(bytes,1024*1024)+"}\n";
}
std::string SupervisorControl::PredecessorJson() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.operationMutex,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Predecessor observation busy");
    p.CheckStore();
    if (!p.predecessor) throw std::runtime_error("Data service has no activated original plan predecessor");
    return "{\"schema\":1,\"kind\":\"getdaq.supervisor-plan-predecessor\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+
        ",\"instance_id\":"+EncodeJsonString(p.instance)+",\"plan_sha256\":"+EncodeJsonString(p.planHash)+
        ",\"proof_sha256\":"+EncodeJsonString(p.predecessor->Sha256())+",\"proof\":"+p.predecessor->Json()+"}\n";
}
SupervisorControlOperation SupervisorControl::Operation(const std::string &id) const {
    SupervisorOperationId(id); std::lock_guard<std::mutex> lock(fImpl->stateMutex); const auto found=fImpl->jobs.find(id);
    if (found==fImpl->jobs.end()) throw std::out_of_range("Supervisor operation not found");
    return found->second;
}
std::string SupervisorControl::CatalogReplacementObservationJson(const std::string &publishOperationId) const {
    SupervisorOperationId(publishOperationId);auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if(!operation.owns_lock())throw std::runtime_error("Catalog replacement observation busy; not queued");
    p.CheckStore();SupervisorControlOperation original;
    {
        std::lock_guard<std::mutex> state(p.stateMutex);const auto found=p.jobs.find(publishOperationId);
        if(found==p.jobs.end())throw std::out_of_range("Catalog Publish operation not found");
        original=found->second;
    }
    const auto &request=original.request;
    if(request.command!=SupervisorCommand::Catalog || !request.catalogReplaceExisting ||
       (request.catalogPhase!="publish" && request.catalogPhase!="finalize"))
        throw std::invalid_argument("Observation requires a retained Native replacement Publish/Finalize operation");
    const auto target=ReceiverCatalogPublicationTarget(p.options.receiver.catalogRoot,p.options.receiver.deploymentId,
                                                        request.catalogExperiment,request.catalogRunNumber);
    const auto stage=ReceiverCatalogReplacementStagePath(target,request.catalogReplacementId);
    const auto candidate=ReceiverCatalogReplacementCandidatePath(target,request.catalogReplacementId);
    const auto observation=InspectReceiverCatalogReplacement(target,stage,candidate,request.catalogReplacementId);
    p.CheckStore();
    const auto result="{\"schema\":1,\"kind\":\"getdaq.supervisor-catalog-replacement-observation\",\"agent_id\":"+
        EncodeJsonString(p.options.agentId)+",\"current_instance_id\":"+EncodeJsonString(p.instance)+
        ",\"plan_sha256\":"+EncodeJsonString(p.planHash)+",\"publish_operation_id\":"+EncodeJsonString(publishOperationId)+
        ",\"request_key\":"+EncodeJsonString(original.requestKey)+",\"original_instance_id\":"+EncodeJsonString(request.instanceId)+
        ",\"controller_id\":"+EncodeJsonString(request.controllerId)+",\"controller_snapshot_sha256\":"+EncodeJsonString(request.controllerSnapshotSha256)+
        ",\"controller_epoch\":"+EncodeJsonString(request.controllerEpoch)+",\"catalog_replacement_id\":"+EncodeJsonString(request.catalogReplacementId)+
        ",\"operation_phase\":"+EncodeJsonString(original.phase)+",\"observation\":"+observation.substr(0,observation.size()-1)+"}\n";
    if(result.size()>65536)throw std::runtime_error("Catalog replacement observation exceeds 64 KiB");
    return result;
}
std::string SupervisorControl::IdentityJson() const {
    auto &p=*fImpl;
    return "{\"schema\":1,\"service\":\"getdaq-supervisor\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+",\"controller_id\":"+EncodeJsonString(p.options.controllerId)+
        ",\"instance_id\":"+EncodeJsonString(p.instance)+",\"plan_sha256\":"+EncodeJsonString(p.planHash)+",\"plan\":"+p.data.PlanBytes()+"}\n";
}
std::string SupervisorControl::StatusJson() const {
    auto &p=*fImpl; std::string control;
    {
        std::lock_guard<std::mutex> lock(p.stateMutex);
        control="\"operation_active\":"+std::string(p.active ? "true" : "false")+",\"operation_id\":"+EncodeJsonString(p.currentOperation)+
            ",\"control_needs_reconciliation\":"+(p.needsReconciliation ? "true" : "false")+",\"message\":"+EncodeJsonString(p.message)+
            ",\"controller_snapshot_sha256\":"+EncodeJsonString(p.ownerSnapshot)+",\"controller_epoch\":"+EncodeJsonString(p.ownerEpoch);
    }
    return "{\"schema\":1,\"service\":\"getdaq-supervisor\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"plan_sha256\":"+EncodeJsonString(p.planHash)+','+control+",\"lifecycle\":"+EncodeSupervisorStatus(p.data.Status())+"}\n";
}
std::string SupervisorControl::ReceiversJson() const {
    auto &p=*fImpl;
    return "{\"schema\":1,\"service\":\"getdaq-supervisor\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+
        ",\"controller_id\":"+EncodeJsonString(p.options.controllerId)+",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"plan_sha256\":"+EncodeJsonString(p.planHash)+",\"observation\":"+EncodeSupervisorObservation(p.data.ObserveReceivers())+"}\n";
}
std::string SupervisorControl::ReceiverEventsJson(std::size_t sourceIndex,bool fileBytes) const {
    auto &p=*fImpl;
    const auto events=p.data.ObserveAcquisitionEvents(sourceIndex,fileBytes);
    return "{\"schema\":1,\"service\":\"getdaq-supervisor\",\"agent_id\":"+EncodeJsonString(p.options.agentId)+
        ",\"controller_id\":"+EncodeJsonString(p.options.controllerId)+",\"instance_id\":"+EncodeJsonString(p.instance)+
        ",\"plan_sha256\":"+EncodeJsonString(p.planHash)+",\"events\":{\"source_id\":"+EncodeJsonString(events.sourceId)+
        ",\"session_id\":"+EncodeJsonString(events.sessionId)+",\"pid\":"+std::to_string(events.pid)+
        ",\"available\":"+((fileBytes ? events.fileBytesAvailable : events.available) ? "true" : "false")+",\""+(fileBytes ? "accepted_file_bytes" : "acquisition_events")+"\":"+std::to_string(fileBytes ? events.fileBytes : events.acquisitionEvents)+
        ",\"observed_at_unix_ms\":"+EncodeJsonString(std::to_string(events.observedAtUnixMs))+"}}\n";
}
}
