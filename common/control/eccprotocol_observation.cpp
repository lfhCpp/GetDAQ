#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"
#include <limits>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace GetDAQ {
using namespace EccProtocolInternal;
EccControlSourceObservation DecodeEccControlSourceObservation(const std::string &bytes) {
    const auto v=Parse(bytes,256*1024);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","configuration_sha256","artifact_sha256","controller_operation_id","busy","sources"});
    if(Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-sources")throw std::invalid_argument("Unsupported ECC source observation");
    EccControlSourceObservation result;result.instanceId=String(v.At("instance_id"));result.snapshotSha256=String(v.At("snapshot_sha256"));
    result.configurationSha256=String(v.At("configuration_sha256"));result.artifactSha256=String(v.At("artifact_sha256"));
    for(const auto *value:{&result.instanceId,&result.snapshotSha256,&result.configurationSha256,&result.artifactSha256})Digest(*value);
    result.controllerOperationId=String(v.At("controller_operation_id"));if(!result.controllerOperationId.empty())Id(result.controllerOperationId);
    result.busy=Boolean(v.At("busy"));Array(v.At("sources"),195);if(result.busy && !v.At("sources").items.empty())throw std::invalid_argument("Busy source observation cannot contain a partial source set");
    std::set<std::string> ids;
    for(const auto &row:v.At("sources").items) {
        Fields(row,{"id","protocol","phase","message","local_state","source_instance_id","receiver_session","observed_at_unix_ms","state","error","issued_bytes","locally_sent_bytes","start_issued","stop_acknowledged","drain_acknowledged","finish_issued","source_end_acknowledged","failure_stop_issued","failure_stop_acknowledged"});
        EccControlSourceObservation::Source s;s.id=String(row.At("id"));Text(s.id,128);if(!ids.insert(s.id).second)throw std::invalid_argument("Duplicate source observation ID");
        s.protocol=String(row.At("protocol"));if(s.protocol!="TCP" && s.protocol!="ZBUF" && s.protocol!="FDT" && s.protocol!="ICE")throw std::invalid_argument("Unsupported observed source protocol");
        s.phase=String(row.At("phase"));if(s.phase!="legacy" && s.phase!="unbound" && s.phase!="busy" && s.phase!="unavailable" && s.phase!="available")throw std::invalid_argument("Unsupported source observation phase");
        s.message=String(row.At("message"));DiagnosticText(s.message,512);s.localState=Number(row.At("local_state"),9);
        s.instanceId=String(row.At("source_instance_id"));s.receiverSession=String(row.At("receiver_session"));if(!s.instanceId.empty())Digest(s.instanceId);if(!s.receiverSession.empty())Digest(s.receiverSession);
        s.observedAtUnixMs=Counter(row.At("observed_at_unix_ms"));s.state=String(row.At("state"));s.error=String(row.At("error"));DiagnosticText(s.error,512);
        s.issuedBytes=Counter(row.At("issued_bytes"));s.locallySentBytes=Counter(row.At("locally_sent_bytes"));
        s.startIssued=Boolean(row.At("start_issued"));s.stopAcknowledged=Boolean(row.At("stop_acknowledged"));s.drainAcknowledged=Boolean(row.At("drain_acknowledged"));
        s.finishIssued=Boolean(row.At("finish_issued"));s.sourceEndAcknowledged=Boolean(row.At("source_end_acknowledged"));
        s.failureStopIssued=Boolean(row.At("failure_stop_issued"));s.failureStopAcknowledged=Boolean(row.At("failure_stop_acknowledged"));
        if(s.phase=="available") {
            const std::set<std::string> states={"fresh","connecting","connected","starting","running","stopping","stopped","disconnecting","disconnected","unknown"};
            if(s.protocol!="ICE" || s.instanceId.empty() || s.receiverSession.empty() || !s.observedAtUnixMs || !states.count(s.state) ||
               s.issuedBytes>std::uint64_t(std::numeric_limits<std::int64_t>::max()) || s.locallySentBytes>s.issuedBytes ||
               (s.sourceEndAcknowledged && (!s.finishIssued || !s.stopAcknowledged || !s.drainAcknowledged || (s.state!="disconnected" && s.state!="unknown"))) ||
               (s.failureStopAcknowledged && !s.failureStopIssued))throw std::invalid_argument("Invalid bound source observation result");
        } else if(!s.state.empty() || !s.error.empty() || s.issuedBytes || s.locallySentBytes || s.startIssued || s.stopAcknowledged || s.drainAcknowledged || s.finishIssued || s.sourceEndAcknowledged || s.failureStopIssued || s.failureStopAcknowledged)
            throw std::invalid_argument("Unobserved source cannot carry invented remote success fields");
        if((s.phase=="legacy")!=(s.protocol!="ICE"))throw std::invalid_argument("Legacy source observation/protocol differ");
        result.sources.push_back(s);
    }
    return result;
}
std::vector<std::string> FailedBoundReceivers(const EccControlSystemStatus &bound,
                                            const EccControlReceiverObservation &observed,
                                            const std::string &dataBinding) {
    std::vector<std::string> failed;
    if (bound.operationActive || bound.controller.transition ||
        observed.instanceId!=bound.instanceId || observed.snapshotSha256!=bound.snapshotSha256 ||
        observed.dataBindingSha256!=dataBinding || bound.nodes.size()!=observed.nodes.size()) return failed;
    for (std::size_t i=0;i<bound.nodes.size();++i) {
        const auto &expected=bound.nodes[i]; const auto &node=observed.nodes[i];
        if (!node.available || node.busy || !node.observedAtUnixMs || expected.instanceId.empty() ||
            node.agentId!=expected.agentId || node.instanceId!=expected.instanceId || node.planSha256!=expected.planSha256 ||
            expected.sessionId.empty() || node.lifecycle.sessionId!=expected.sessionId ||
            expected.state!="ready" || node.lifecycle.state!="ready" ||
            expected.receivers.size()!=node.lifecycle.receivers.size() || node.receivers.size()!=expected.receivers.size()) continue;
        bool same=true;
        for (std::size_t j=0;j<expected.receivers.size();++j) {
            const auto &a=expected.receivers[j]; const auto &b=node.lifecycle.receivers[j]; const auto &process=node.receivers[j];
            if (a.reaped || a.sealVerified || a.pid<=0 || a.id!=b.id || a.sessionId!=b.sessionId ||
                a.outputDirectory!=b.outputDirectory || a.pid!=b.pid || a.id!=process.id || a.pid!=process.pid) same=false;
        }
        if (!same) continue;
        for (const auto &process:node.receivers)
            if ((process.state=="exited" || process.state=="reaped") && (process.exitCode>0 || process.signal>0))
                failed.push_back(process.id);
    }
    return failed;
}
EccControlReceiverObservation DecodeEccControlReceiverObservation(const std::string &bytes) {
    const auto v=Parse(bytes,1024*1024);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","data_binding_sha256","nodes"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-receivers") throw std::invalid_argument("Unsupported ECC receiver observation");
    EccControlReceiverObservation result; result.instanceId=String(v.At("instance_id")); result.snapshotSha256=String(v.At("snapshot_sha256"));
    result.dataBindingSha256=String(v.At("data_binding_sha256")); Digest(result.instanceId); Digest(result.snapshotSha256); Digest(result.dataBindingSha256);
    Array(v.At("nodes"),195); std::set<std::string> agents,receiverIds; std::size_t count=0;
    for (const auto &entry:v.At("nodes").items) {
        Fields(entry,{"agent_id","plan_sha256","instance_id","available","message","observation"});
        EccControlReceiverNodeObservation node; node.agentId=String(entry.At("agent_id")); Text(node.agentId,128);
        if (!agents.insert(node.agentId).second) throw std::invalid_argument("Duplicate ECC receiver observation agent");
        node.planSha256=String(entry.At("plan_sha256")); Digest(node.planSha256); node.instanceId=String(entry.At("instance_id"));
        node.available=Boolean(entry.At("available")); node.message=String(entry.At("message")); DiagnosticText(node.message,512);
        const auto &o=entry.At("observation");
        if (!node.available) {
            if (o.type!=JsonValue::Null || !node.instanceId.empty() || node.message.empty()) throw std::invalid_argument("Unavailable node has trusted observation");
            result.nodes.push_back(node); continue;
        }
        Digest(node.instanceId); if (!node.message.empty()) throw std::invalid_argument("Available receiver node has failure message");
        Fields(o,{"schema","busy","observed_at_unix_ms","lifecycle","receivers"});
        if (Number(o.At("schema"))!=1) throw std::invalid_argument("Unsupported receiver process observation schema");
        node.busy=Boolean(o.At("busy")); node.observedAtUnixMs=Counter(o.At("observed_at_unix_ms"));
        node.lifecycle=Lifecycle(o.At("lifecycle"),node.planSha256,receiverIds,count);
        node.lifecycle.agentId=node.agentId; node.lifecycle.instanceId=node.instanceId;
        const auto &rows=o.At("receivers"); Array(rows,195);
        if (node.busy ? !rows.items.empty() : rows.items.size()!=node.lifecycle.receivers.size())
            throw std::invalid_argument("Receiver observation count differs from busy/lifecycle");
        for (std::size_t i=0;i<rows.items.size();++i) {
            const auto &row=rows.items[i]; const auto &child=node.lifecycle.receivers[i];
            Fields(row,{"id","pid","state","exit_code","signal","message"});
            EccControlReceiverProcess process; process.id=String(row.At("id")); process.pid=int(Number(row.At("pid")));
            process.state=String(row.At("state")); process.message=String(row.At("message")); DiagnosticText(process.message,512);
            process.exitCode=row.At("exit_code").type==JsonValue::Number && row.At("exit_code").text=="-1" ? -1 : int(Number(row.At("exit_code"),255));
            process.signal=int(Number(row.At("signal"),127));
            if (process.id!=child.id || process.pid!=child.pid) throw std::invalid_argument("Receiver process identity differs from lifecycle");
            const bool reaped=process.state=="reaped",exited=process.state=="exited",absent=process.state=="not-started",running=process.state=="running",unavailable=process.state=="unavailable";
            if ((!reaped && !exited && !absent && !running && !unavailable) ||
                (reaped && (!child.reaped || process.exitCode!=child.exitCode || process.exitCode<0 || process.signal)) ||
                (!reaped && child.reaped) || (absent && process.pid!=0) || ((running || exited) && process.pid<=0) ||
                (exited && (process.exitCode<0 || (process.signal && process.exitCode!=128+process.signal))) ||
                (!reaped && !exited && (process.exitCode!=-1 || process.signal)) ||
                (unavailable && process.message.empty()) || (!unavailable && !process.message.empty()))
                throw std::invalid_argument("Invalid ECC receiver process observation state");
            node.receivers.push_back(process);
        }
        result.nodes.push_back(node);
    }
    return result;
}
EccControlReceiverEvents DecodeEccControlReceiverEvents(const std::string &bytes,bool fileBytes) {
    const auto v=Parse(bytes,16384);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","data_binding_sha256","events"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!=(fileBytes ? "getdaq.ecc-receiver-file-bytes" : "getdaq.ecc-receiver-events"))
        throw std::invalid_argument("Unsupported ECC receiver event observation");
    EccControlReceiverEvents result;
    result.instanceId=String(v.At("instance_id")); result.snapshotSha256=String(v.At("snapshot_sha256"));
    result.dataBindingSha256=String(v.At("data_binding_sha256"));
    Digest(result.instanceId); Digest(result.snapshotSha256); Digest(result.dataBindingSha256);
    const auto &events=v.At("events");
    if(fileBytes)Fields(events,{"agent_id","plan_sha256","instance_id","source_id","session_id","pid","available","accepted_file_bytes","observed_at_unix_ms"});
    else Fields(events,{"agent_id","plan_sha256","instance_id","source_id","session_id","pid","available","acquisition_events","observed_at_unix_ms"});
    result.agentId=String(events.At("agent_id")); Text(result.agentId,128);
    result.planSha256=String(events.At("plan_sha256")); Digest(result.planSha256);
    result.receiverInstanceId=String(events.At("instance_id")); Digest(result.receiverInstanceId);
    result.sourceId=String(events.At("source_id")); Text(result.sourceId,128);
    result.sessionId=String(events.At("session_id")); Digest(result.sessionId);
    result.pid=int(Number(events.At("pid")));
    result.available=Boolean(events.At("available"));
    if(fileBytes)result.fileBytes=Counter(events.At("accepted_file_bytes"));
    else result.acquisitionEvents=Counter(events.At("acquisition_events"));
    result.observedAtUnixMs=Counter(events.At("observed_at_unix_ms"));
    if (result.pid<=0 || !result.observedAtUnixMs || (!result.available && (result.acquisitionEvents || result.fileBytes)))
        throw std::invalid_argument("Invalid ECC receiver event identity/count/availability");
    return result;
}
EccControlReceiverFailureStatus DecodeEccControlReceiverFailureStatus(const std::string &bytes) {
    if (bytes.size()>1024*1024) throw std::invalid_argument("ECC receiver failure cache exceeds 1 MiB");
    JsonLimits limits; limits.stringBytes=1024*1024;
    const auto v=ParseStrictJson(bytes,limits);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","enabled","latched","operation_id","phase","evidence"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-receiver-failure")
        throw std::invalid_argument("Unsupported ECC receiver failure cache");
    EccControlReceiverFailureStatus result;
    result.instanceId=String(v.At("instance_id")); result.snapshotSha256=String(v.At("snapshot_sha256"));
    Digest(result.instanceId); Digest(result.snapshotSha256);
    result.enabled=Boolean(v.At("enabled")); result.latched=Boolean(v.At("latched"));
    result.operationId=String(v.At("operation_id")); if (!result.operationId.empty()) Id(result.operationId);
    result.phase=String(v.At("phase"));
    const std::set<std::string> phases={"disabled","watching","observation-unavailable","held","accepted","complete","failed","unknown","reopened-held","startup-reconciliation"};
    if (!phases.count(result.phase) || (!result.enabled && (result.phase!="disabled" || result.latched || !result.operationId.empty())) ||
        (result.enabled && result.phase=="disabled") || (!result.latched && !result.operationId.empty()) ||
        (result.latched && !result.enabled))
        throw std::invalid_argument("Inconsistent ECC receiver failure policy state");
    const bool waiting=result.phase=="disabled" || result.phase=="watching" || result.phase=="observation-unavailable";
    if (waiting && result.latched) throw std::invalid_argument("Waiting receiver policy cannot claim a held failure");
    const auto &evidence=v.At("evidence");
    if (evidence.type!=JsonValue::Null) {
        Fields(evidence,{"bound_status","observation"});
        result.bound=DecodeEccControlSystemStatus(String(evidence.At("bound_status")));
        result.observation=DecodeEccControlReceiverObservation(String(evidence.At("observation")));
        result.failedIds=FailedBoundReceivers(result.bound,result.observation,result.observation.dataBindingSha256);
        if (!result.latched || waiting || result.operationId.compare(0,18,"internal-receiver-")!=0 ||
            result.bound.snapshotSha256!=result.snapshotSha256 || result.failedIds.empty() ||
            (result.phase!="reopened-held" && result.bound.instanceId!=result.instanceId) ||
            (result.bound.controller.state!=4 && result.bound.controller.state!=5 && result.bound.controller.state!=6))
            throw std::invalid_argument("Receiver failure evidence is not bound to the retained acquisition");
        result.hasEvidence=true;
    }
    const bool actionable=result.phase=="held" || result.phase=="accepted" || result.phase=="complete" || result.phase=="failed" || result.phase=="reopened-held";
    if ((actionable && (!result.latched || !result.hasEvidence || result.operationId.empty())) ||
        (result.phase=="unknown" && !result.latched) ||
        (result.phase=="startup-reconciliation" && (!result.latched || !result.operationId.empty() || result.hasEvidence)))
        throw std::invalid_argument("Receiver failure phase lacks required held evidence");
    return result;
}
}
