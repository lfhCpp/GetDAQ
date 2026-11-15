#include "controlprotocol.h"
#include "planio.h"
#include "supervisorcommon.h"
#include "systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include <stdexcept>
#include <limits>
namespace GetDAQ {
SupervisorControlRequest DecodeSupervisorControlRequest(const std::string &bytes) {
    using namespace SupervisorIO;
    if (bytes.size()>2U*1024U*1024U) throw std::invalid_argument("Supervisor request exceeds 2 MiB");
    const auto v=ParseStrictJson(bytes);
    std::set<std::string> fields={"schema","operation_id","command","plan_sha256","instance_id","controller_id","controller_snapshot_sha256","controller_epoch","configuration_sha256","artifact_sha256","parent_audit_reference"};
    const auto command=ParseSupervisorCommand(String(v.At("command")));
    if (command==SupervisorCommand::Catalog) { fields.insert("catalog_experiment"); fields.insert("catalog_run_number"); fields.insert("catalog_session_id"); if(v.members.count("catalog_replace_existing")) fields.insert("catalog_replace_existing"); if(v.members.count("catalog_phase")) fields.insert("catalog_phase"); if(v.members.count("catalog_replacement_id")) fields.insert("catalog_replacement_id"); }
    if(command==SupervisorCommand::AbortFailedSession || command==SupervisorCommand::RetireFailedSession || SupervisorCommandRetiresPlan(command)) fields.insert("expected_session_id");
    if(SupervisorCommandRetiresPlan(command)) {fields.insert("successor_plan_file");fields.insert("successor_plan_sha256");}
    if(command==SupervisorCommand::StageSuccessorPlan) {fields.insert("successor_plan_file");fields.insert("successor_plan_sha256");fields.insert("successor_plan");}
    if((command==SupervisorCommand::AbortFailedSession || command==SupervisorCommand::RetireFailedSession) && v.members.count("parent_failure_operation_id")) fields.insert("parent_failure_operation_id");
    if (command==SupervisorCommand::Finish && v.members.count("unused_source_ids")) fields.insert("unused_source_ids");
    if(command==SupervisorCommand::HandoffControl || command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(command)) {fields.insert("successor_controller_snapshot_sha256");fields.insert("successor_controller_epoch");}
    if (command==SupervisorCommand::CompletePlanRetirement) fields.insert("failed_retirement_operation_id");
    if (command==SupervisorCommand::CompleteControlHandoff) fields.insert("failed_handoff_operation_id");
    ExactSupervisorFields(v,fields);
    if (Unsigned(v.At("schema"))!=1) throw std::invalid_argument("Unsupported supervisor request version");
    SupervisorControlRequest r;
    r.operationId=String(v.At("operation_id")); r.command=command; r.planSha256=String(v.At("plan_sha256"));
    r.instanceId=String(v.At("instance_id")); r.controllerId=String(v.At("controller_id")); r.controllerSnapshotSha256=String(v.At("controller_snapshot_sha256"));
    r.controllerEpoch=String(v.At("controller_epoch")); r.configurationSha256=String(v.At("configuration_sha256")); r.artifactSha256=String(v.At("artifact_sha256"));
    r.parentAuditReference=String(v.At("parent_audit_reference"));
    if (command==SupervisorCommand::CompletePlanRetirement) r.failedRetirementOperationId=String(v.At("failed_retirement_operation_id"));
    if (command==SupervisorCommand::CompleteControlHandoff) r.failedHandoffOperationId=String(v.At("failed_handoff_operation_id"));
    if(command==SupervisorCommand::HandoffControl || command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(command)) {r.successorControllerSnapshotSha256=String(v.At("successor_controller_snapshot_sha256"));r.successorControllerEpoch=String(v.At("successor_controller_epoch"));}
    if(SupervisorCommandRetiresPlan(command) || command==SupervisorCommand::StageSuccessorPlan) {r.successorPlanFile=String(v.At("successor_plan_file"));r.successorPlanSha256=String(v.At("successor_plan_sha256"));}
    if(command==SupervisorCommand::StageSuccessorPlan) {const auto plan=DecodeSupervisorPlan(v.At("successor_plan"));r.successorPlanJson=EncodeOptions(plan.options,plan.executableSha256);}
    if(command==SupervisorCommand::AbortFailedSession || command==SupervisorCommand::RetireFailedSession || SupervisorCommandRetiresPlan(command)) r.expectedSessionId=String(v.At("expected_session_id"));
    if(fields.count("parent_failure_operation_id")) {r.parentFailureOperationId=String(v.At("parent_failure_operation_id"));SupervisorOperationId(r.parentFailureOperationId);}
    if (command==SupervisorCommand::Catalog) { r.catalogExperiment=String(v.At("catalog_experiment")); r.catalogRunNumber=SupervisorUnsigned32(v.At("catalog_run_number")); r.catalogSessionId=String(v.At("catalog_session_id")); if(v.members.count("catalog_replace_existing"))r.catalogReplaceExisting=Boolean(v.At("catalog_replace_existing")); if(v.members.count("catalog_phase"))r.catalogPhase=String(v.At("catalog_phase")); if(v.members.count("catalog_replacement_id"))r.catalogReplacementId=String(v.At("catalog_replacement_id")); }
    if (fields.count("unused_source_ids")) {
        const auto &ids=v.At("unused_source_ids"); if (ids.type!=JsonValue::Array || ids.items.empty() || ids.items.size()>195) throw std::invalid_argument("Unused sources must be a nonempty bounded array");
        for (const auto &id:ids.items) r.unusedSourceIds.push_back(String(id));
    }
    ValidateSupervisorControlRequest(r);
    if(bytes.size()>SupervisorControlRequestMaxBytes(command)) throw std::invalid_argument("Supervisor request exceeds its command-specific bound");
    return r;
}
std::string EncodeSupervisorControlRequest(const SupervisorControlRequest &r) {
    ValidateSupervisorControlRequest(r);
    auto bytes="{\"schema\":1,\"operation_id\":"+EncodeJsonString(r.operationId)+",\"command\":"+EncodeJsonString(SupervisorCommandName(r.command))+
        ",\"plan_sha256\":"+EncodeJsonString(r.planSha256)+",\"instance_id\":"+EncodeJsonString(r.instanceId)+",\"controller_id\":"+EncodeJsonString(r.controllerId)+
        ",\"controller_snapshot_sha256\":"+EncodeJsonString(r.controllerSnapshotSha256)+",\"controller_epoch\":"+EncodeJsonString(r.controllerEpoch)+
        ",\"configuration_sha256\":"+EncodeJsonString(r.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(r.artifactSha256)+
        ",\"parent_audit_reference\":"+EncodeJsonString(r.parentAuditReference);
    if(r.command==SupervisorCommand::HandoffControl || r.command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(r.command)) bytes+=",\"successor_controller_snapshot_sha256\":"+EncodeJsonString(r.successorControllerSnapshotSha256)+",\"successor_controller_epoch\":"+EncodeJsonString(r.successorControllerEpoch);
    if(SupervisorCommandRetiresPlan(r.command) || r.command==SupervisorCommand::StageSuccessorPlan)
        bytes+=",\"successor_plan_file\":"+EncodeJsonString(r.successorPlanFile)+",\"successor_plan_sha256\":"+EncodeJsonString(r.successorPlanSha256);
    if(r.command==SupervisorCommand::StageSuccessorPlan) {
        std::string plan=r.successorPlanJson;
        if(!plan.empty() && plan.back()=='\n')plan.pop_back();
        bytes+=",\"successor_plan\":"+plan;
    }
    if (r.command==SupervisorCommand::Catalog) bytes+=",\"catalog_experiment\":"+EncodeJsonString(r.catalogExperiment)+",\"catalog_run_number\":"+std::to_string(r.catalogRunNumber)+",\"catalog_session_id\":"+EncodeJsonString(r.catalogSessionId)+
        (r.catalogReplaceExisting ? ",\"catalog_replace_existing\":true" : "")+
        (r.catalogPhase!="single" ? ",\"catalog_phase\":"+EncodeJsonString(r.catalogPhase)+",\"catalog_replacement_id\":"+EncodeJsonString(r.catalogReplacementId) : "");
    if(r.command==SupervisorCommand::AbortFailedSession || r.command==SupervisorCommand::RetireFailedSession || SupervisorCommandRetiresPlan(r.command)) bytes+=",\"expected_session_id\":"+EncodeJsonString(r.expectedSessionId);
    if(r.command==SupervisorCommand::CompletePlanRetirement) bytes+=",\"failed_retirement_operation_id\":"+EncodeJsonString(r.failedRetirementOperationId);
    if(r.command==SupervisorCommand::CompleteControlHandoff) bytes+=",\"failed_handoff_operation_id\":"+EncodeJsonString(r.failedHandoffOperationId);
    if(!r.parentFailureOperationId.empty()) bytes+=",\"parent_failure_operation_id\":"+EncodeJsonString(r.parentFailureOperationId);
    if (!r.unusedSourceIds.empty()) { bytes+=",\"unused_source_ids\":["; for (const auto &id:r.unusedSourceIds) { if (bytes.back()!='[') bytes+=',';bytes+=EncodeJsonString(id); } bytes+=']'; }
    bytes+="}\n";
    if (bytes.size()>SupervisorControlRequestMaxBytes(r.command)) throw std::invalid_argument("Encoded supervisor request exceeds its command-specific bound");
    return bytes;
}
SupervisorControlRequest OriginalPlanRetirementRequest(const SupervisorControlRequest &completion) {
    ValidateSupervisorControlRequest(completion);
    if (completion.command!=SupervisorCommand::CompletePlanRetirement) throw std::invalid_argument("An explicit retirement completion is required");
    auto original=completion; original.command=SupervisorCommand::RetirePlan;
    original.operationId=completion.failedRetirementOperationId; original.failedRetirementOperationId.clear();
    ValidateSupervisorControlRequest(original); return original;
}
SupervisorControlRequest OriginalControlHandoffRequest(const SupervisorControlRequest &completion) {
    ValidateSupervisorControlRequest(completion);
    if (completion.command!=SupervisorCommand::CompleteControlHandoff) throw std::invalid_argument("An explicit control handoff completion is required");
    auto original=completion; original.command=SupervisorCommand::HandoffControl;
    original.operationId=completion.failedHandoffOperationId; original.failedHandoffOperationId.clear();
    ValidateSupervisorControlRequest(original); return original;
}
std::string SupervisorControlRequestKey(const SupervisorControlRequest &r) { return ConfigurationSha256("getdaq.supervisor-request:1\n"+EncodeSupervisorControlRequest(r)); }

}
