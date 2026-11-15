#include "coldhandoff_evidence_p.h"

#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"

#include <functional>
#include <map>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace {
using namespace SupervisorIO;

void Require(bool value,const std::string &message) {if(!value)throw std::runtime_error(message);}

void EachRecord(const std::string &bytes,const std::function<void(const JsonValue &)> &consume,std::size_t maximum) {
    Require(bytes.empty() || bytes.back()=='\n',"Incomplete cold completion journal");
    std::size_t position=0,count=0;
    while(position<bytes.size()) {
        while(position<bytes.size() && (bytes[position]==' ' || bytes[position]=='\t' || bytes[position]=='\r' || bytes[position]=='\n'))++position;
        if(position==bytes.size())break;
        Require(bytes[position]=='{' && count++<maximum,"Invalid/budgeted cold completion record");
        const auto begin=position;int depth=0;bool quoted=false,escape=false;
        do {
            const char c=bytes[position++];
            if(quoted) {if(escape)escape=false;else if(c=='\\')escape=true;else if(c=='"')quoted=false;}
            else if(c=='"')quoted=true;
            else if(c=='{' || c=='[')++depth;
            else if(c=='}' || c==']')--depth;
            Require(depth>=0 && depth<=64,"Invalid cold completion JSON nesting");
        }while(position<bytes.size() && (depth || quoted));
        Require(!depth && !quoted && position<bytes.size() && bytes[position]=='\n',"Incomplete/non-delimited cold completion record");
        JsonLimits limits;limits.stringBytes=1024*1024;consume(ParseStrictJson(bytes.substr(begin,position-begin),limits));
    }
}

std::vector<JsonValue> Records(const std::string &bytes) {
    std::vector<JsonValue> rows;EachRecord(bytes,[&](const JsonValue &value){rows.push_back(value);},8192);
    return rows;
}
}

void ValidateColdParentHistory(const std::string &bytes,const EccControlRequest &original) {
    Require(!bytes.empty(),"Original ECC network history is absent");
    bool found=false;std::string instance;std::uint64_t sequence=0;
    std::map<std::string,std::pair<std::string,std::string>> operations;
    EachRecord(bytes,[&](const JsonValue &row) {
        Require(Unsigned(row.At("schema"))==1 && Unsigned(row.At("sequence"))==sequence++ &&
            String(row.At("snapshot_sha256"))==original.snapshotSha256,"Original ECC network sequence/snapshot differs");
        const auto event=String(row.At("event"));
        if(event=="opened") {ExactSupervisorFields(row,{"schema","sequence","snapshot_sha256","event","instance_id"});instance=String(row.At("instance_id"));Digest(instance);return;}
        if(event=="accepted") {
            const auto request=DecodeEccControlRequest(String(row.At("request")));
            Require(String(row.At("operation_id"))==request.operationId && String(row.At("request_key"))==EccControlRequestKey(request) &&
                instance==request.instanceId,"Original ECC accepted request binding differs");
            if(request.operationId==original.operationId) {
                Require(!found && EncodeEccControlRequest(request)==EncodeEccControlRequest(original),"Original ECC parent request differs/repeats");found=true;
            }else if(found) {
                Require(request.command=="complete-data-handoff" && request.failedOperationId==original.operationId &&
                    request.instanceId==original.instanceId && request.clientId==original.clientId && request.clientEpoch==original.clientEpoch,
                    "ECC history contains an unrelated action after the original handoff");
            }
            if(found) {
                ExactSupervisorFields(row,{"schema","sequence","snapshot_sha256","event","operation_id","request_key","request"});
                Require(operations.size()<8192,"Parent completion operation budget exceeded");
                Require(operations.insert(std::make_pair(request.operationId,std::make_pair(EccControlRequestKey(request),"accepted"))).second,"Repeated parent completion operation");
            }
        }else if(found) {
            Require(event=="running" || event=="complete" || event=="failed","Independent ECC hold/action remains after original handoff");
            const auto id=String(row.At("operation_id"));auto op=operations.find(id);
            Require(op!=operations.end() && op->second.first==String(row.At("request_key")) &&
                (op->second.second=="accepted" || op->second.second=="running"),"Parent handoff history transition differs");
            if(event=="running")Require(op->second.second=="accepted","Repeated running parent completion");
            op->second.second=event;
        }
    },bytes.size()/2+1); // Input is capped at the service's 64MiB; avoid retaining all old rows.
    Require(found,"Original handoff was not accepted by this ECC network instance");
}

std::string BuildColdCompletionReceipt(const std::string &scope,const std::string &epoch,const std::string &originalPath,
                    const std::vector<RemoteHandoffNodeEvidence> &nodes,const std::string &journal) {
    const auto rows=Records(journal);Require(!rows.empty() && String(rows.back().At("phase"))=="complete",
        "Cold attempt is not durably complete; preserve evidence and review before a new explicit attempt");
    std::map<std::string,JsonValue> proofs;
    for(const auto &row:rows)if(String(row.At("phase"))=="node_control_handoff_complete") {
        const auto &node=row.At("receipt");Require(proofs.insert(std::make_pair(String(node.At("agent_id")),node)).second,"Repeated cold node proof");
    }
    Require(proofs.size()==nodes.size(),"Cold completion lacks every original node proof");
    std::string encoded="[";
    for(const auto &old:nodes) {
        const auto found=proofs.find(old.agentId);Require(found!=proofs.end(),"Cold receipt node differs");const auto &node=found->second;
        std::set<std::string> fields={"agent_id","instance_id","operation_id","request_key","proof_sha256","request"};
        Require(node.members.count("completion_operation_id")==node.members.count("completion_request"),
            "Cold node proof has incomplete completion metadata");
        const bool hasCompletion=node.members.count("completion_operation_id")!=0;
        if(hasCompletion) {fields.insert("completion_operation_id");fields.insert("completion_request");}
        ExactSupervisorFields(node,fields);
        const auto &r=old.request;
        Require(String(node.At("instance_id"))==r.instanceId && String(node.At("operation_id"))==r.operationId &&
            String(node.At("request_key"))==SupervisorControlRequestKey(r) && String(node.At("request"))==EncodeSupervisorControlRequest(r),"Cold receipt changed original child request");
        std::string completionFields;
        if(hasCompletion) {
            const auto completionId=String(node.At("completion_operation_id"));
            const auto completionBytes=String(node.At("completion_request"));
            Require(completionId.empty()==completionBytes.empty(),"Cold node completion identity/request pair differs");
            if(!completionId.empty()) {
                SupervisorOperationId(completionId);
                const auto completion=DecodeSupervisorControlRequest(completionBytes);
                Require(completion.operationId==completionId && completion.command==SupervisorCommand::CompleteControlHandoff &&
                    EncodeSupervisorControlRequest(completion)==completionBytes &&
                    EncodeSupervisorControlRequest(OriginalControlHandoffRequest(completion))==EncodeSupervisorControlRequest(r),
                    "Cold node completion metadata does not bind the original control handoff");
            }
            completionFields+=",\"completion_operation_id\":"+EncodeJsonString(completionId)+
                ",\"completion_request\":"+EncodeJsonString(completionBytes);
        }
        Digest(String(node.At("proof_sha256")));if(encoded.back()!='[')encoded+=',';
        encoded+="{\"agent_id\":"+EncodeJsonString(old.agentId)+",\"instance_id\":"+EncodeJsonString(r.instanceId)+
            ",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(SupervisorControlRequestKey(r))+
            ",\"proof_sha256\":"+EncodeJsonString(String(node.At("proof_sha256")))+",\"request\":"+EncodeJsonString(EncodeSupervisorControlRequest(r))+
            completionFields+"}";
    }
    const auto &first=nodes.front().request;
    const auto receipt="{\"schema\":1,\"kind\":"+EncodeJsonString(first.command==SupervisorCommand::RetirePlan ? "getdaq.remote-plan-retirement" : "getdaq.remote-control-handoff")+
        ",\"original_scope_sha256\":"+EncodeJsonString(scope)+",\"original_epoch\":"+EncodeJsonString(epoch)+
        ",\"successor_scope_sha256\":"+EncodeJsonString(first.successorControllerSnapshotSha256)+",\"successor_epoch\":"+EncodeJsonString(first.successorControllerEpoch)+
        ",\"audit_directory\":"+EncodeJsonString(originalPath)+",\"nodes\":"+encoded+"]}\n";
    Require(String(rows.back().At("receipt_sha256"))==ConfigurationSha256(receipt) && Boolean(rows.back().At("original_control_remains_fenced")) &&
        !Boolean(rows.back().At("raw_data_forwarded")),"Cold completed receipt digest/fence differs");return receipt;
}

} // namespace GetDAQ
