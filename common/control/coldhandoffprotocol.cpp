#include "coldhandoffprotocol.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <set>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Require(bool value) {if(!value)throw std::invalid_argument("Invalid bounded cold handoff metadata");}
void Fields(const JsonValue &v,std::initializer_list<const char *> fields) {
    Require(v.type==JsonValue::Object && v.members.size()==fields.size());for(const auto f:fields)v.At(f);
}
std::string String(const JsonValue &v) {Require(v.type==JsonValue::String);return v.text;}
void Digest(const std::string &s) {Require(s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos);}
void Id(const std::string &s) {Require(!s.empty() && s.size()<=128 && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos);}
JsonValue Parse(const std::string &bytes,std::size_t cap) {
    Require(bytes.size()<=cap);JsonLimits limits;limits.stringBytes=1024*1024;return ParseStrictJson(bytes,limits);
}
void Kind(const JsonValue &v,const std::string &kind) {Require(v.At("schema").type==JsonValue::Number && v.At("schema").text=="1" && String(v.At("kind"))==kind);}
ColdHandoffRequest Request(const JsonValue &v) {
    Fields(v,{"schema","kind","task_id","instance_id","policy_sha256","action","attempt_id"});Kind(v,"getdaq.cold-handoff-request");
    ColdHandoffRequest r;r.taskId=String(v.At("task_id"));r.instanceId=String(v.At("instance_id"));r.policySha256=String(v.At("policy_sha256"));r.action=String(v.At("action"));r.attemptId=String(v.At("attempt_id"));return r;
}
}
std::string EncodeColdHandoffIdentity(const ColdHandoffIdentity &i) {
    Id(i.serviceId);Digest(i.instanceId);Digest(i.policySha256);Id(i.originalOperationId);Digest(i.originalRequestSha256);Digest(i.successorPlanSha256);
    return "{\"schema\":1,\"kind\":\"getdaq.cold-handoff-identity\",\"service_id\":"+EncodeJsonString(i.serviceId)+",\"instance_id\":"+EncodeJsonString(i.instanceId)+",\"policy_sha256\":"+EncodeJsonString(i.policySha256)+",\"original_operation_id\":"+EncodeJsonString(i.originalOperationId)+",\"original_request_sha256\":"+EncodeJsonString(i.originalRequestSha256)+",\"successor_plan_sha256\":"+EncodeJsonString(i.successorPlanSha256)+",\"raw_in_qt\":false,\"old_hardware_authority_restored\":false}\n";
}
ColdHandoffIdentity DecodeColdHandoffIdentity(const std::string &s) {
    const auto v=Parse(s,8192);Fields(v,{"schema","kind","service_id","instance_id","policy_sha256","original_operation_id","original_request_sha256","successor_plan_sha256","raw_in_qt","old_hardware_authority_restored"});Kind(v,"getdaq.cold-handoff-identity");
    for(const auto field:{"raw_in_qt","old_hardware_authority_restored"})Require(v.At(field).type==JsonValue::Boolean && v.At(field).text=="false");
    ColdHandoffIdentity i;i.serviceId=String(v.At("service_id"));i.instanceId=String(v.At("instance_id"));i.policySha256=String(v.At("policy_sha256"));i.originalOperationId=String(v.At("original_operation_id"));i.originalRequestSha256=String(v.At("original_request_sha256"));i.successorPlanSha256=String(v.At("successor_plan_sha256"));EncodeColdHandoffIdentity(i);return i;
}
std::string EncodeColdHandoffRequest(const ColdHandoffRequest &r) {
    Id(r.taskId);Id(r.attemptId);Digest(r.instanceId);Digest(r.policySha256);Require(r.action=="submit" || r.action=="inspect");Require(r.action!="submit" || r.taskId==r.attemptId);
    return "{\"schema\":1,\"kind\":\"getdaq.cold-handoff-request\",\"task_id\":"+EncodeJsonString(r.taskId)+",\"instance_id\":"+EncodeJsonString(r.instanceId)+",\"policy_sha256\":"+EncodeJsonString(r.policySha256)+",\"action\":"+EncodeJsonString(r.action)+",\"attempt_id\":"+EncodeJsonString(r.attemptId)+"}\n";
}
ColdHandoffRequest DecodeColdHandoffRequest(const std::string &s) {auto r=Request(Parse(s,8192));EncodeColdHandoffRequest(r);return r;}
std::string ColdHandoffRequestKey(const ColdHandoffRequest &r) {return ConfigurationSha256("getdaq.cold-handoff-request:1\n"+EncodeColdHandoffRequest(r));}
std::string EncodeColdHandoffOperation(const ColdHandoffOperation &o) {
    const auto request=EncodeColdHandoffRequest(o.request);Require(o.requestKey==ColdHandoffRequestKey(o.request));
    Require(std::set<std::string>{"accepted","running","complete","failed","unknown"}.count(o.phase) && o.message.size()<=4096);ValidateJsonUtf8(o.message);
    Require((o.phase=="complete")==!o.receipt.empty());
    if(!o.receipt.empty()) {
        Require(o.receipt.size()<=1024*1024 && ConfigurationSha256(o.receipt)==o.receiptSha256);Digest(o.receiptSha256);
        const auto proof=Parse(o.receipt,1024*1024);Require(proof.type==JsonValue::Object && proof.At("schema").type==JsonValue::Number && proof.At("schema").text=="1");
        const auto kind=String(proof.At("kind"));Require(kind=="getdaq.remote-plan-retirement" || kind=="getdaq.remote-control-handoff");
    }else Require(o.receiptSha256.empty());
    return "{\"schema\":1,\"kind\":\"getdaq.cold-handoff-operation\",\"request\":"+request+",\"request_key\":"+EncodeJsonString(o.requestKey)+",\"phase\":"+EncodeJsonString(o.phase)+",\"message\":"+EncodeJsonString(o.message)+",\"receipt\":"+EncodeJsonString(o.receipt,1024*1024)+",\"receipt_sha256\":"+EncodeJsonString(o.receiptSha256)+"}\n";
}
ColdHandoffOperation DecodeColdHandoffOperation(const std::string &s) {
    const auto v=Parse(s,8*1024*1024);Fields(v,{"schema","kind","request","request_key","phase","message","receipt","receipt_sha256"});Kind(v,"getdaq.cold-handoff-operation");
    ColdHandoffOperation o;o.request=Request(v.At("request"));o.requestKey=String(v.At("request_key"));o.phase=String(v.At("phase"));o.message=String(v.At("message"));o.receipt=String(v.At("receipt"));o.receiptSha256=String(v.At("receipt_sha256"));EncodeColdHandoffOperation(o);return o;
}
}
