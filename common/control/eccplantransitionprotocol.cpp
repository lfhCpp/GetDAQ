#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"

#include "configurationsha256.h"
#include "strictjson.h"

#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace EccProtocolInternal;

EccSuccessorIdentity DecodeEccSuccessorIdentity(const std::string &bytes) {
    const auto value=Parse(bytes,65536);
    Fields(value,{"schema","kind","plan_file","service_id","controller_id","input_binding_sha256","snapshot_sha256","data_lifecycle_binding_sha256","data_controller_scope_sha256","audit_root","receiver_controller_epoch"});
    if (Number(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.ecc-successor-plan") throw std::invalid_argument("Unsupported ECC successor identity");
    EccSuccessorIdentity result;result.planFile=String(value.At("plan_file"));result.serviceId=String(value.At("service_id"));result.controllerId=String(value.At("controller_id"));
    result.inputBindingSha256=String(value.At("input_binding_sha256"));result.snapshotSha256=String(value.At("snapshot_sha256"));
    result.dataBindingSha256=String(value.At("data_lifecycle_binding_sha256"));result.scopeSha256=String(value.At("data_controller_scope_sha256"));
    result.auditRoot=String(value.At("audit_root"));result.epoch=String(value.At("receiver_controller_epoch"));
    Text(result.serviceId,128);Text(result.controllerId,128);
    for(const auto &value:{result.inputBindingSha256,result.snapshotSha256,result.dataBindingSha256,result.scopeSha256,result.epoch})Digest(value);
    const auto path=[](const std::string &value) {
        Text(value,4096);
        if(value.size()<2 || value[0]!='/' || value.back()=='/' || value.find("//")!=std::string::npos || value.find("/./")!=std::string::npos || value.find("/../")!=std::string::npos ||
            value.substr(value.size()-2)=="/." || (value.size()>2 && value.substr(value.size()-3)=="/.."))throw std::invalid_argument("Successor identity requires canonical service-host paths");
    };
    path(result.planFile);path(result.auditRoot);return result;
}
std::string EncodeEccSuccessorIdentity(const EccSuccessorIdentity &identity) {
    std::string bytes="{\"schema\":1,\"kind\":\"getdaq.ecc-successor-plan\"";
    const std::pair<const char *,const std::string *> fields[]={
        {"plan_file",&identity.planFile},{"service_id",&identity.serviceId},{"controller_id",&identity.controllerId},
        {"input_binding_sha256",&identity.inputBindingSha256},{"snapshot_sha256",&identity.snapshotSha256},{"data_lifecycle_binding_sha256",&identity.dataBindingSha256},
        {"data_controller_scope_sha256",&identity.scopeSha256},{"audit_root",&identity.auditRoot},{"receiver_controller_epoch",&identity.epoch}};
    for(const auto &field:fields)bytes+=','+EncodeJsonString(field.first)+':'+EncodeJsonString(*field.second);
    bytes+="}\n";DecodeEccSuccessorIdentity(bytes);return bytes;
}
EccPlanTransitionReview ReviewEccPlanTransition(const std::string &successorBytes,const std::string &targetsBytes) {
    EccPlanTransitionReview review;review.successor=DecodeEccSuccessorIdentity(successorBytes);
    if(EncodeEccSuccessorIdentity(review.successor)!=successorBytes)throw std::invalid_argument("Review requires exact sealed canonical successor bytes; do not reformat the identity copy");
    review.successorBytes=successorBytes;review.successorSha256=ConfigurationSha256(successorBytes);review.targetsBytes=targetsBytes;
    const auto document=Parse(targetsBytes,8192);Fields(document,{"schema","kind","plans"});
    if(Number(document.At("schema"))!=1 || String(document.At("kind"))!="getdaq.reviewed-data-plan-targets")throw std::invalid_argument("Unsupported reviewed data targets");
    const auto &plans=document.At("plans");Array(plans,195);
    for(const auto &entry:plans.items) {
        Fields(entry,{"agent_id","plan_sha256","plan_file"});EccReceiverPlanTarget target;
        target.agentId=String(entry.At("agent_id"));target.planSha256=String(entry.At("plan_sha256"));target.planFile=String(entry.At("plan_file"));review.targets.push_back(target);
    }
    EccControlRequest request;request.command="retire-data-plans";request.operationId="review";request.clientId="offline-review";
    request.instanceId=request.clientEpoch=std::string(64,'0');request.snapshotSha256=review.successor.snapshotSha256;
    request.successorScopeSha256=review.successor.scopeSha256;request.successorEpoch=review.successor.epoch;request.successorPlanSha256=review.successorSha256;
    request.successorDataBindingSha256=review.successor.dataBindingSha256;request.successorPlanFile=review.successor.planFile;request.successorReceiverPlans=review.targets;
    EncodeEccControlRequest(request);return review;
}
void ValidateEccPlanRetirementReceipt(const std::string &bytes,const EccControlRequest &request,const EccControlSystemStatus &original) {
    ValidateEccControlRequest(request);
    if(request.command!="retire-data-plans" || original.instanceId!=request.instanceId || original.snapshotSha256!=request.snapshotSha256 ||
        original.operationActive || original.controller.transition || original.controller.outcomeUnknown || original.controller.error ||
        (original.controller.needsReconciliation && (!original.controlNeedsReconciliation || !original.dataNeedsReconciliation)) ||
        (original.controller.state!=1 && original.controller.state!=2))throw std::invalid_argument("Receipt requires the original known stopped ECC instance");
    if(bytes.size()>1024*1024)throw std::invalid_argument("Retirement receipt exceeds 1MiB");
    JsonLimits limits;limits.stringBytes=8192;const auto value=ParseStrictJson(bytes,limits);
    Fields(value,{"schema","kind","original_scope_sha256","original_epoch","successor_scope_sha256","successor_epoch","audit_directory","nodes"});
    if(Number(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.remote-plan-retirement" ||
        String(value.At("original_scope_sha256"))!=original.dataScopeSha256 || String(value.At("original_epoch"))!=original.dataEpoch ||
        String(value.At("successor_scope_sha256"))!=request.successorScopeSha256 || String(value.At("successor_epoch"))!=request.successorEpoch)
        throw std::invalid_argument("Retirement receipt scope/epoch differs");
    Digest(original.dataScopeSha256);Digest(original.dataEpoch);const auto directory=String(value.At("audit_directory"));Text(directory,4096);
    if(request.successorScopeSha256==original.dataScopeSha256 || request.successorEpoch==original.dataEpoch)throw std::invalid_argument("Retirement receipt requires a distinct successor scope and epoch");
    if(directory.empty() || directory[0]!='/')throw std::invalid_argument("Invalid original retirement audit directory");
    const auto &nodes=value.At("nodes");Array(nodes,195);
    if(nodes.items.size()!=request.successorReceiverPlans.size() || nodes.items.size()!=original.nodes.size())throw std::invalid_argument("Retirement receipt lacks exact original/target node set");
    std::set<std::string> seen;
    for(const auto &node:nodes.items) {
        Fields(node,{"agent_id","instance_id","operation_id","request_key","proof_sha256","request"});const auto agent=String(node.At("agent_id"));
        if(!seen.insert(agent).second)throw std::invalid_argument("Duplicate retirement receipt agent");
        const EccReceiverPlanTarget *target=nullptr;const EccControlDataNodeStatus *old=nullptr;
        for(const auto &entry:request.successorReceiverPlans)if(entry.agentId==agent)target=&entry;
        for(const auto &entry:original.nodes)if(entry.agentId==agent)old=&entry;
        if(!target || !old || old->state!="stopped" || old->needsReconciliation || old->receivers.empty())throw std::invalid_argument("Receipt original node is not the normally sealed cycle");
        for(const auto &receiver:old->receivers)if(!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)throw std::invalid_argument("Receipt original receiver is not normally sealed");
        const auto childBytes=String(node.At("request"));if(childBytes.size()>8192)throw std::invalid_argument("Node retirement request exceeds limit");
        const auto child=ParseStrictJson(childBytes);
        Fields(child,{"schema","operation_id","command","plan_sha256","instance_id","controller_id","controller_snapshot_sha256","controller_epoch","configuration_sha256","artifact_sha256","parent_audit_reference","successor_controller_snapshot_sha256","successor_controller_epoch","successor_plan_file","successor_plan_sha256","expected_session_id"});
        const auto operation=String(node.At("operation_id")),key=String(node.At("request_key"));Id(operation);Digest(key);Digest(String(node.At("proof_sha256")));
        if(Number(child.At("schema"))!=1 || String(child.At("command"))!="retire-plan" || String(child.At("operation_id"))!=operation ||
            String(child.At("instance_id"))!=old->instanceId || String(node.At("instance_id"))!=old->instanceId || String(child.At("plan_sha256"))!=old->planSha256 ||
            String(child.At("expected_session_id"))!=old->sessionId || String(child.At("controller_snapshot_sha256"))!=original.dataScopeSha256 || String(child.At("controller_epoch"))!=original.dataEpoch ||
            String(child.At("successor_controller_snapshot_sha256"))!=request.successorScopeSha256 || String(child.At("successor_controller_epoch"))!=request.successorEpoch ||
            String(child.At("successor_plan_sha256"))!=target->planSha256 || String(child.At("successor_plan_file"))!=target->planFile || String(child.At("parent_audit_reference"))!=directory ||
            ConfigurationSha256("getdaq.supervisor-request:1\n"+childBytes)!=key)throw std::invalid_argument("Node retirement receipt differs from original cycle/target/request key");
        Text(String(child.At("controller_id")),128);Digest(String(child.At("configuration_sha256")));Digest(String(child.At("artifact_sha256")));
    }
}

}
