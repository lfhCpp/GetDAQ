#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include <limits>
#include <stdexcept>
#include <set>
#include <utility>
namespace GetDAQ {
namespace {
using namespace EccProtocolInternal;
EccControlCatalogReceipt Catalog(const JsonValue &v) {
    Fields(v,{"schema","kind","complete","request_key","manifest_sha256","operation_id","directory","payload_method","raw_payload_bytes_read","linked_files"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.receiver-catalog-complete" || !Boolean(v.At("complete")) ||
        String(v.At("payload_method"))!="hardlink" || Counter(v.At("raw_payload_bytes_read"))!=0) throw std::invalid_argument("Invalid data catalog completion method/status");
    EccControlCatalogReceipt r; r.requestKey=String(v.At("request_key")); r.manifestSha256=String(v.At("manifest_sha256")); Digest(r.requestKey); Digest(r.manifestSha256);
    r.operationId=String(v.At("operation_id")); Id(r.operationId); r.directory=String(v.At("directory")); Text(r.directory,3500);
    r.linkedFiles=Counter(v.At("linked_files")); if (r.linkedFiles>65536) throw std::invalid_argument("Catalog file count exceeds budget"); return r;
}
void ValidateOperation(const EccControlOperation &op) {
    ValidateEccControlRequest(op.request);
    if (op.requestKey!=EccControlRequestKey(op.request)) throw std::invalid_argument("ECC operation request key differs");
    if (op.phase!="accepted" && op.phase!="running" && op.phase!="complete" && op.phase!="failed" && op.phase!="unknown") throw std::invalid_argument("Unsupported ECC operation phase");
    DiagnosticText(op.message,16384);
    if (op.resultJson.empty()) {
        if (op.phase=="complete" || op.phase=="failed") throw std::invalid_argument("Terminal ECC operation lacks controller result");
    } else {
        DecodeEccControlControllerStatus(op.resultJson);
        if (op.phase=="accepted" || op.phase=="running") throw std::invalid_argument("Unfinished ECC operation has a terminal result");
    }
}
}
EccControlAlarmStatus DecodeEccControlAlarmStatus(const std::string &bytes) {
    const auto v=Parse(bytes,128*1024);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","enabled","latched","operation_id","phase","observation"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-alarms") throw std::invalid_argument("Unsupported ECC alarm schema");
    EccControlAlarmStatus s; s.instanceId=String(v.At("instance_id")); s.snapshotSha256=String(v.At("snapshot_sha256")); Digest(s.instanceId); Digest(s.snapshotSha256);
    s.enabled=Boolean(v.At("enabled")); s.latched=Boolean(v.At("latched")); s.operationId=String(v.At("operation_id")); if (!s.operationId.empty()) Id(s.operationId);
    s.phase=String(v.At("phase"));
    const std::set<std::string> phases={"disabled","watching","held","accepted","running","complete","failed","unknown","held-no-run","reopened-held","startup-reconciliation"};
    if (!phases.count(s.phase)) throw std::invalid_argument("Unsupported ECC alarm phase");
    if (!s.enabled && (s.latched || !s.operationId.empty() || s.phase!="disabled")) throw std::invalid_argument("Disabled ECC alarm policy has an active hold");
    if (s.enabled && ((s.phase=="watching" && (s.latched || !s.operationId.empty())) ||
        (s.phase!="watching" && (!s.latched || s.phase=="disabled")))) throw std::invalid_argument("ECC alarm phase/hold differs");
    if (!s.operationId.empty() && s.operationId.compare(0,15,"internal-alarm-")!=0) throw std::invalid_argument("ECC alarm operation is not an internal ID");
    if ((s.phase=="complete" || s.phase=="failed" || s.phase=="accepted" || s.phase=="running") && s.operationId.empty()) throw std::invalid_argument("ECC alarm action phase lacks original ID");
    const auto &observation=v.At("observation");
    if (observation.type==JsonValue::Null) return s;
    if (!s.enabled) throw std::invalid_argument("Disabled ECC alarm policy has an observation");
    Fields(observation,{"controller_state","controller_outcome_unknown","controller_operation_id","stop_requested","nodes"});
    s.hasObservation=true; s.controllerState=Number(observation.At("controller_state"),6); s.controllerOutcomeUnknown=Boolean(observation.At("controller_outcome_unknown"));
    s.controllerOperationId=String(observation.At("controller_operation_id")); if (!s.controllerOperationId.empty()) Id(s.controllerOperationId);
    s.stopRequested=Boolean(observation.At("stop_requested")); Array(observation.At("nodes"),192); std::set<std::string> ids;
    for (const auto &entry:observation.At("nodes").items) {
        Fields(entry,{"id","subscription","received","durable","lost","invalid","pending","alarm_mask","stop_requested","queue_overflow","audit_failed","budget_exceeded"});
        EccControlAlarmNode node; node.id=String(entry.At("id")); Text(node.id,128); if (!ids.insert(node.id).second) throw std::invalid_argument("Duplicate ECC alarm node ID");
        node.subscription=Number(entry.At("subscription"),5); node.pending=Number(entry.At("pending"),4096); node.alarmMask=Number(entry.At("alarm_mask"),15);
        node.received=Counter(entry.At("received")); node.durable=Counter(entry.At("durable")); node.lost=Counter(entry.At("lost")); node.invalid=Counter(entry.At("invalid"));
        node.stopRequested=Boolean(entry.At("stop_requested")); node.queueOverflow=Boolean(entry.At("queue_overflow")); node.auditFailed=Boolean(entry.At("audit_failed")); node.budgetExceeded=Boolean(entry.At("budget_exceeded"));
        s.nodes.push_back(node);
    }
    return s;
}
EccControlMaintenanceTargets DecodeEccControlMaintenanceTargets(const std::string &bytes) {
    const auto value=Parse(bytes,131072);Fields(value,{"schema","kind","instance_id","snapshot_sha256","configuration_sha256","artifact_sha256","cobos"});
    if(Number(value.At("schema"))!=2 || String(value.At("kind"))!="getdaq.ecc-cobo-maintenance-targets")throw std::invalid_argument("Unsupported CoBo maintenance targets");
    EccControlMaintenanceTargets targets;targets.instanceId=String(value.At("instance_id"));targets.snapshotSha256=String(value.At("snapshot_sha256"));
    targets.configurationSha256=String(value.At("configuration_sha256"));targets.artifactSha256=String(value.At("artifact_sha256"));
    for(const auto &text:{targets.instanceId,targets.snapshotSha256,targets.configurationSha256,targets.artifactSha256})Digest(text);
    const auto &nodes=value.At("cobos");if(nodes.type!=JsonValue::Array || nodes.items.empty() || nodes.items.size()>192)throw std::invalid_argument("Invalid active CoBo maintenance set");
    std::set<std::string> ids;
    for(const auto &node:nodes.items) {Fields(node,{"id","node_name","lifecycle_state","operation_id","operation","operation_phase"});EccControlMaintenanceTarget target;target.id=String(node.At("id"));target.nodeName=String(node.At("node_name"));
        target.lifecycleState=String(node.At("lifecycle_state"));target.operationId=String(node.At("operation_id"));target.operation=String(node.At("operation"));target.operationPhase=String(node.At("operation_phase"));
        Text(target.id,128);Text(target.nodeName,128);Text(target.lifecycleState,32);if(!target.operation.empty())Text(target.operation,64);Text(target.operationPhase,16);
        if(!target.operationId.empty())Id(target.operationId);
        if(target.lifecycleState!="unknown" && target.lifecycleState!="uninitialized" && target.lifecycleState!="described" && target.lifecycleState!="prepared" && target.lifecycleState!="configured" && target.lifecycleState!="running" && target.lifecycleState!="paused" && target.lifecycleState!="transitioning")throw std::invalid_argument("Invalid CoBo lifecycle state");
        if(target.operationPhase!="idle" && target.operationPhase!="accepted" && target.operationPhase!="running" && target.operationPhase!="complete" && target.operationPhase!="failed" && target.operationPhase!="unknown")throw std::invalid_argument("Invalid CoBo operation phase");
        if(target.operationPhase!="idle" && (target.operation.empty() || target.operationId.empty()))throw std::invalid_argument("CoBo operation phase requires original operation identity");
        if(!ids.insert(target.id).second)throw std::invalid_argument("Duplicate CoBo maintenance target");
        targets.cobos.push_back(target);
    }
    return targets;
}
std::string EncodeEccControlMaintenanceTargets(const EccControlMaintenanceTargets &targets) {
    std::string bytes="{\"schema\":2,\"kind\":\"getdaq.ecc-cobo-maintenance-targets\",\"instance_id\":"+EncodeJsonString(targets.instanceId)+
        ",\"snapshot_sha256\":"+EncodeJsonString(targets.snapshotSha256)+",\"configuration_sha256\":"+EncodeJsonString(targets.configurationSha256)+
        ",\"artifact_sha256\":"+EncodeJsonString(targets.artifactSha256)+",\"cobos\":[";
    for(const auto &node:targets.cobos) {if(bytes.back()!='[')bytes+=',';bytes+="{\"id\":"+EncodeJsonString(node.id)+",\"node_name\":"+EncodeJsonString(node.nodeName)+
        ",\"lifecycle_state\":"+EncodeJsonString(node.lifecycleState)+",\"operation_id\":"+EncodeJsonString(node.operationId)+
        ",\"operation\":"+EncodeJsonString(node.operation)+",\"operation_phase\":"+EncodeJsonString(node.operationPhase)+"}";}
    bytes+="]}\n";DecodeEccControlMaintenanceTargets(bytes);return bytes;
}
EccControlCatalogReceipt DecodeEccControlCatalogReceipt(const std::string &bytes) { return Catalog(Parse(bytes,65536)); }
std::string EncodeEccControlCatalogReceipt(const EccControlCatalogReceipt &r) {
    Digest(r.requestKey); Digest(r.manifestSha256); Id(r.operationId); Text(r.directory,3500);
    if (r.linkedFiles>65536) throw std::invalid_argument("Catalog file count exceeds budget");
    return "{\"schema\":1,\"kind\":\"getdaq.receiver-catalog-complete\",\"complete\":true,\"request_key\":"+EncodeJsonString(r.requestKey)+",\"manifest_sha256\":"+EncodeJsonString(r.manifestSha256)+
        ",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"directory\":"+EncodeJsonString(r.directory)+",\"payload_method\":\"hardlink\",\"raw_payload_bytes_read\":\"0\",\"linked_files\":"+EncodeJsonString(std::to_string(r.linkedFiles))+"}\n";
}
EccControlControllerStatus DecodeEccControlControllerStatus(const std::string &bytes) { return ControllerStatus(Parse(bytes,65536)); }
std::string EncodeEccControlControllerStatus(const EccControlControllerStatus &s) {
    if (s.state>6 || s.transition>17 || s.error>unsigned(std::numeric_limits<int>::max())) throw std::invalid_argument("Invalid ECC controller status");
    if (!s.operationId.empty()) Id(s.operationId);
    DiagnosticText(s.message,16384);
    return "{\"state\":"+std::to_string(s.state)+",\"transition\":"+std::to_string(s.transition)+",\"error\":"+std::to_string(s.error)+
        ",\"outcome_unknown\":"+(s.outcomeUnknown ? "true" : "false")+",\"needs_reconciliation\":"+(s.needsReconciliation ? "true" : "false")+
        ",\"operation_id\":"+EncodeJsonString(s.operationId)+",\"message\":"+EncodeJsonString(s.message)+"}";
}
EccControlSystemStatus DecodeEccControlSystemStatus(const std::string &bytes) {
    const auto v=Parse(bytes,1024*1024);
    Fields(v,{"schema","service","instance_id","snapshot_sha256","operation_active","operation_id","client_epoch","control_needs_reconciliation","message","controller","data"});
    if (Number(v.At("schema"))!=1 || String(v.At("service"))!="getdaq-ecc-server") throw std::invalid_argument("Unsupported ECC system status");
    EccControlSystemStatus s; s.instanceId=String(v.At("instance_id")); s.snapshotSha256=String(v.At("snapshot_sha256")); Digest(s.instanceId); Digest(s.snapshotSha256);
    s.operationActive=Boolean(v.At("operation_active")); s.operationId=String(v.At("operation_id"));
    if (!s.operationId.empty()) Id(s.operationId);
    if (s.operationActive && s.operationId.empty()) throw std::invalid_argument("Active ECC status lacks operation ID");
    s.clientEpoch=String(v.At("client_epoch")); if (!s.clientEpoch.empty()) Digest(s.clientEpoch);
    s.controlNeedsReconciliation=Boolean(v.At("control_needs_reconciliation")); s.message=String(v.At("message")); DiagnosticText(s.message,16384); s.controller=ControllerStatus(v.At("controller"));
    const auto &data=v.At("data"); Fields(data,{"needs_reconciliation","controller_scope_sha256","controller_epoch","message","nodes"});
    s.dataNeedsReconciliation=Boolean(data.At("needs_reconciliation")); s.dataScopeSha256=String(data.At("controller_scope_sha256")); s.dataEpoch=String(data.At("controller_epoch"));
    Digest(s.dataScopeSha256); Digest(s.dataEpoch); s.dataMessage=String(data.At("message")); DiagnosticText(s.dataMessage,16384); Array(data.At("nodes"),195);
    std::set<std::string> agents,receiverIds; std::size_t receivers=0;
    for (const auto &node:data.At("nodes").items) {
        const bool hasCatalog=node.type==JsonValue::Object && node.members.count("catalog");
        if (hasCatalog) Fields(node,{"agent_id","instance_id","plan_sha256","lifecycle","catalog"});
        else Fields(node,{"agent_id","instance_id","plan_sha256","lifecycle"});
        EccControlDataNodeStatus n; if (hasCatalog) { n.catalog=Catalog(node.At("catalog")); n.catalogVerified=true; }
        n.agentId=String(node.At("agent_id")); Text(n.agentId,128); if (!agents.insert(n.agentId).second) throw std::invalid_argument("Duplicate ECC data agent ID");
        n.instanceId=String(node.At("instance_id")); if (!n.instanceId.empty()) Digest(n.instanceId);
        n.planSha256=String(node.At("plan_sha256")); Digest(n.planSha256);
        const auto life=Lifecycle(node.At("lifecycle"),n.planSha256,receiverIds,receivers);
        n.state=life.state; n.needsReconciliation=life.needsReconciliation; n.sessionId=life.sessionId; n.message=life.message; n.receivers=life.receivers;
        s.nodes.push_back(n);
    }
    return s;
}
std::string EncodeEccControlOperation(const EccControlOperation &op) {
    ValidateOperation(op);
    return "{\"schema\":1,\"request\":"+EncodeEccControlRequest(op.request)+",\"request_key\":"+EncodeJsonString(op.requestKey)+",\"phase\":"+EncodeJsonString(op.phase)+
        ",\"message\":"+EncodeJsonString(op.message)+",\"result\":"+(op.resultJson.empty() ? "null" : op.resultJson)+"}\n";
}
EccControlOperation DecodeEccControlOperation(const std::string &bytes) {
    const auto v=Parse(bytes,65536); Fields(v,{"schema","request","request_key","phase","message","result"});
    if (Number(v.At("schema"))!=1) throw std::invalid_argument("Unsupported ECC operation schema");
    EccControlOperation op; op.request=EccProtocolInternal::DecodeRequest(v.At("request")); op.requestKey=String(v.At("request_key")); op.phase=String(v.At("phase")); op.message=String(v.At("message"));
    if (v.At("result").type!=JsonValue::Null) op.resultJson=EncodeEccControlControllerStatus(ControllerStatus(v.At("result")));
    ValidateOperation(op); return op;
}
}
