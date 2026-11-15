#include "transferprotocol.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <set>
#include <stdexcept>
#include <limits>
namespace GetDAQ {
namespace {
void Require(bool value) {if(!value)throw std::invalid_argument("Invalid bounded native transfer metadata");}
void Fields(const JsonValue &v,std::initializer_list<const char *> fields) {Require(v.type==JsonValue::Object && v.members.size()==fields.size());for(const auto f:fields)v.At(f);}
std::string String(const JsonValue &v) {Require(v.type==JsonValue::String);return v.text;}
void Digest(const std::string &s) {Require(s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos);}
void Id(const std::string &s) {Require(!s.empty() && s.size()<=80 && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos);}
void Path(const std::string &s) {Require(s.size()>1 && s.size()<=3500 && s[0]=='/' && s.back()!='/' && s.find("//")==std::string::npos && s.find("/../")==std::string::npos && s.find("/./")==std::string::npos && s.substr(s.size()-2)!="/." && (s.size()<3 || s.substr(s.size()-3)!="/.."));ValidateJsonUtf8(s);for(unsigned char c:s)Require(c>=32 && c!=127);}
std::uint64_t Counter(const JsonValue &v,bool quoted=true) {
    Require(v.type==(quoted ? JsonValue::String : JsonValue::Number));const auto s=v.text;Require(!s.empty() && s.size()<=20 && s.find_first_not_of("0123456789")==std::string::npos && (s.size()==1 || s[0]!='0'));
    std::uint64_t result=0;for(char c:s){Require(result<=(UINT64_MAX-unsigned(c-'0'))/10);result=result*10+unsigned(c-'0');}return result;
}
JsonValue Parse(const std::string &s,std::size_t cap) {Require(s.size()<=cap);return ParseStrictJson(s);}
void Request(const TransferRequest &r) {Id(r.operationId);Id(r.experiment);Require(r.experiment.front()!='_' && r.experiment.front()!='-');Digest(r.instanceId);Digest(r.policySha256);Digest(r.sourceRequestKey);Require(r.runNumber<unsigned(std::numeric_limits<int>::max()));}
}
std::string EncodeTransferIdentity(const TransferIdentity &i) {
    Id(i.agentId);Digest(i.instanceId);Digest(i.policySha256);Path(i.sourceRoot);Path(i.backupRoot);
    return "{\"schema\":1,\"kind\":\"getdaq.transfer-identity\",\"agent_id\":"+EncodeJsonString(i.agentId)+",\"instance_id\":"+EncodeJsonString(i.instanceId)+",\"policy_sha256\":"+EncodeJsonString(i.policySha256)+",\"source_root\":"+EncodeJsonString(i.sourceRoot)+",\"backup_root\":"+EncodeJsonString(i.backupRoot)+",\"raw_in_qt\":false}\n";
}
TransferIdentity DecodeTransferIdentity(const std::string &s) {
    const auto v=Parse(s,16384);Fields(v,{"schema","kind","agent_id","instance_id","policy_sha256","source_root","backup_root","raw_in_qt"});Require(Counter(v.At("schema"),false)==1 && String(v.At("kind"))=="getdaq.transfer-identity" && v.At("raw_in_qt").type==JsonValue::Boolean && v.At("raw_in_qt").text=="false");
    TransferIdentity i;i.agentId=String(v.At("agent_id"));i.instanceId=String(v.At("instance_id"));i.policySha256=String(v.At("policy_sha256"));i.sourceRoot=String(v.At("source_root"));i.backupRoot=String(v.At("backup_root"));EncodeTransferIdentity(i);return i;
}
std::string EncodeTransferRequest(const TransferRequest &r) {
    Request(r);return "{\"schema\":1,\"kind\":\"getdaq.transfer-request\",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"instance_id\":"+EncodeJsonString(r.instanceId)+",\"policy_sha256\":"+EncodeJsonString(r.policySha256)+",\"experiment\":"+EncodeJsonString(r.experiment)+",\"run_number\":"+std::to_string(r.runNumber)+",\"source_request_key\":"+EncodeJsonString(r.sourceRequestKey)+"}\n";
}
TransferRequest DecodeTransferRequest(const std::string &s) {
    const auto v=Parse(s,16384);Fields(v,{"schema","kind","operation_id","instance_id","policy_sha256","experiment","run_number","source_request_key"});Require(Counter(v.At("schema"),false)==1 && String(v.At("kind"))=="getdaq.transfer-request");
    TransferRequest r;r.operationId=String(v.At("operation_id"));r.instanceId=String(v.At("instance_id"));r.policySha256=String(v.At("policy_sha256"));r.experiment=String(v.At("experiment"));const auto run=Counter(v.At("run_number"),false);Require(run<unsigned(std::numeric_limits<int>::max()));r.runNumber=unsigned(run);r.sourceRequestKey=String(v.At("source_request_key"));Request(r);return r;
}
std::string TransferRequestKey(const TransferRequest &r) {return ConfigurationSha256("getdaq.transfer-request:1\n"+EncodeTransferRequest(r));}
static std::string EncodeRecoveryRequest(const TransferReconciliationRequest &r,const std::string &kind) {
    Id(r.verificationId);Id(r.operationId);Digest(r.instanceId);Digest(r.policySha256);Digest(r.originalRequestKey);
    return "{\"schema\":1,\"kind\":"+EncodeJsonString(kind)+",\"verification_id\":"+EncodeJsonString(r.verificationId)+",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"instance_id\":"+EncodeJsonString(r.instanceId)+",\"policy_sha256\":"+EncodeJsonString(r.policySha256)+",\"original_request_key\":"+EncodeJsonString(r.originalRequestKey)+"}\n";
}
static TransferReconciliationRequest DecodeRecoveryRequest(const std::string &bytes,const std::string &kind) {
    const auto v=Parse(bytes,16384);
    Fields(v,{"schema","kind","verification_id","operation_id","instance_id","policy_sha256","original_request_key"});
    Require(Counter(v.At("schema"),false)==1 && String(v.At("kind"))==kind);
    TransferReconciliationRequest r;r.verificationId=String(v.At("verification_id"));r.operationId=String(v.At("operation_id"));r.instanceId=String(v.At("instance_id"));r.policySha256=String(v.At("policy_sha256"));r.originalRequestKey=String(v.At("original_request_key"));
    EncodeRecoveryRequest(r,kind);return r;
}
std::string EncodeTransferReconciliationRequest(const TransferReconciliationRequest &r){return EncodeRecoveryRequest(r,"getdaq.transfer-reconciliation-request");}
TransferReconciliationRequest DecodeTransferReconciliationRequest(const std::string &s){return DecodeRecoveryRequest(s,"getdaq.transfer-reconciliation-request");}
std::string EncodeTransferResumeRequest(const TransferReconciliationRequest &r){return EncodeRecoveryRequest(r,"getdaq.transfer-resume-request");}
TransferReconciliationRequest DecodeTransferResumeRequest(const std::string &s){return DecodeRecoveryRequest(s,"getdaq.transfer-resume-request");}
std::string EncodeTransferOperation(const TransferOperation &o) {
    Request(o.request);Digest(o.requestKey);Require(o.requestKey==TransferRequestKey(o.request));Path(o.source);Path(o.destination);
    Require(std::set<std::string>{"accepted","running","verifying","complete","failed","unknown"}.count(o.phase) && o.message.size()<=16384 && o.receipt.size()<=65536);ValidateJsonUtf8(o.message);
    Require((o.phase=="complete")==!o.receipt.empty());
    if(!o.receipt.empty()) {
        const auto v=Parse(o.receipt,65536);Fields(v,{"schema","kind","complete","destination","manifest_sha256","source_request_key","copied_files","copied_bytes","content_verified","source_preserved"});
        Require(Counter(v.At("schema"),false)==1 && String(v.At("kind"))=="getdaq.catalog-transfer-complete" && String(v.At("destination"))==o.destination && String(v.At("source_request_key"))==o.request.sourceRequestKey && Counter(v.At("copied_bytes"))==o.copiedBytes);
        Digest(String(v.At("manifest_sha256")));Require(Counter(v.At("copied_files"))<=65536);
        for(const auto field:{"complete","content_verified","source_preserved"})Require(v.At(field).type==JsonValue::Boolean && v.At(field).text=="true");
    }
    return "{\"schema\":1,\"kind\":\"getdaq.transfer-operation\",\"request\":"+EncodeTransferRequest(o.request)+",\"request_key\":"+EncodeJsonString(o.requestKey)+",\"phase\":"+EncodeJsonString(o.phase)+",\"message\":"+EncodeJsonString(o.message)+",\"source\":"+EncodeJsonString(o.source)+",\"destination\":"+EncodeJsonString(o.destination)+",\"copied_bytes\":"+EncodeJsonString(std::to_string(o.copiedBytes))+",\"receipt\":"+(o.receipt.empty() ? "null" : o.receipt)+"}\n";
}
TransferOperation DecodeTransferOperation(const std::string &s) {
    const auto v=Parse(s,128*1024);Fields(v,{"schema","kind","request","request_key","phase","message","source","destination","copied_bytes","receipt"});Require(Counter(v.At("schema"),false)==1 && String(v.At("kind"))=="getdaq.transfer-operation");
    const auto &r=v.At("request");Fields(r,{"schema","kind","operation_id","instance_id","policy_sha256","experiment","run_number","source_request_key"});
    TransferOperation o;o.request.operationId=String(r.At("operation_id"));o.request.instanceId=String(r.At("instance_id"));o.request.policySha256=String(r.At("policy_sha256"));o.request.experiment=String(r.At("experiment"));const auto run=Counter(r.At("run_number"),false);Require(run<unsigned(std::numeric_limits<int>::max()));o.request.runNumber=unsigned(run);o.request.sourceRequestKey=String(r.At("source_request_key"));Require(Counter(r.At("schema"),false)==1 && String(r.At("kind"))=="getdaq.transfer-request");
    o.requestKey=String(v.At("request_key"));o.phase=String(v.At("phase"));o.message=String(v.At("message"));o.source=String(v.At("source"));o.destination=String(v.At("destination"));o.copiedBytes=Counter(v.At("copied_bytes"));
    if(v.At("receipt").type!=JsonValue::Null) {
        const auto canonicalReceipt=v.At("receipt");Fields(canonicalReceipt,{"schema","kind","complete","destination","manifest_sha256","source_request_key","copied_files","copied_bytes","content_verified","source_preserved"});
        Require(Counter(canonicalReceipt.At("schema"),false)==1 && String(canonicalReceipt.At("kind"))=="getdaq.catalog-transfer-complete");
        for(const auto field:{"complete","content_verified","source_preserved"})Require(canonicalReceipt.At(field).type==JsonValue::Boolean && canonicalReceipt.At(field).text=="true");
        o.receipt="{\"schema\":1,\"kind\":\"getdaq.catalog-transfer-complete\",\"complete\":"+canonicalReceipt.At("complete").text+",\"destination\":"+EncodeJsonString(String(canonicalReceipt.At("destination")))+",\"manifest_sha256\":"+EncodeJsonString(String(canonicalReceipt.At("manifest_sha256")))+",\"source_request_key\":"+EncodeJsonString(String(canonicalReceipt.At("source_request_key")))+",\"copied_files\":"+EncodeJsonString(std::to_string(Counter(canonicalReceipt.At("copied_files"))))+",\"copied_bytes\":"+EncodeJsonString(std::to_string(Counter(canonicalReceipt.At("copied_bytes"))))+",\"content_verified\":"+canonicalReceipt.At("content_verified").text+",\"source_preserved\":"+canonicalReceipt.At("source_preserved").text+"}\n";
    }
    EncodeTransferOperation(o);return o;
}
}
