#ifndef GETDAQ_ECCPROTOCOLINTERNAL_P_H
#define GETDAQ_ECCPROTOCOLINTERNAL_P_H

#include "strictjson.h"
#include "systemexperimentconfiguration.h"

#include <initializer_list>
#include <limits>
#include <set>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace EccProtocolInternal {

EccControlRequest DecodeRequest(const JsonValue &v);

inline void Fields(const JsonValue &v,std::initializer_list<const char *> names) {
    if (v.type!=JsonValue::Object || v.members.size()!=names.size()) throw std::invalid_argument("Unexpected ECC protocol fields");
    for (const auto name:names) v.At(name);
}
inline std::string String(const JsonValue &v) {
    if (v.type!=JsonValue::String) throw std::invalid_argument("ECC protocol field must be a string");
    return v.text;
}
inline unsigned int Number(const JsonValue &v,unsigned int maximum=std::numeric_limits<int>::max()) {
    if (v.type!=JsonValue::Number || v.text.empty() || v.text.size()>10 || v.text.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("ECC protocol field must be a bounded unsigned integer");
    unsigned long long n=0;
    for (const char c:v.text) n=n*10+unsigned(c-'0');
    if (n>maximum) throw std::invalid_argument("ECC protocol integer outside range");
    return unsigned(n);
}
inline bool Boolean(const JsonValue &v) {
    if (v.type!=JsonValue::Boolean) throw std::invalid_argument("ECC protocol field must be boolean");
    return v.text=="true";
}
inline void Digest(const std::string &s) {
    if (s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("ECC protocol requires a 64-character lowercase SHA/session value");
}
inline void Text(const std::string &s,std::size_t maximum,bool allowEmpty=false) {
    ValidateJsonUtf8(s);
    if ((!allowEmpty && s.empty()) || s.size()>maximum) throw std::invalid_argument("ECC protocol text outside bounds");
    for (const unsigned char c:s) if (c<32 || c==127) throw std::invalid_argument("ECC protocol text contains control bytes");
}
inline void DiagnosticText(const std::string &s,std::size_t maximum) {
    ValidateJsonUtf8(s);
    if(s.size()>maximum)throw std::invalid_argument("ECC diagnostic text outside bounds");
    for(unsigned char c:s)
        if((c<32 && c!='\n' && c!='\r' && c!='\t') || c==127)
            throw std::invalid_argument("ECC diagnostic text contains unsupported control bytes");
}

inline void Id(const std::string &s) {
    if (s.empty() || s.size()>128 || s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
        throw std::invalid_argument("Invalid ECC operation ID");
}
inline JsonValue Parse(const std::string &s,std::size_t maximum) {
    if (s.size()>maximum) throw std::invalid_argument("ECC protocol body exceeds size limit");
    return ParseStrictJson(s);
}
inline std::uint64_t Counter(const JsonValue &v) {
    const auto text=String(v);
    if (text.empty() || text.size()>20 || text.find_first_not_of("0123456789")!=std::string::npos || (text.size()>1 && text[0]=='0'))
        throw std::invalid_argument("ECC receiver counter must be canonical unsigned decimal text");
    std::uint64_t count=0;
    for (const char c:text) {
        const auto digit=unsigned(c-'0');
        if (count>(std::numeric_limits<std::uint64_t>::max()-digit)/10) throw std::invalid_argument("ECC receiver counter exceeds 64 bits");
        count=count*10+digit;
    }
    return count;
}
inline void Array(const JsonValue &v,std::size_t maximum) {
    if (v.type!=JsonValue::Array || v.items.size()>maximum) throw std::invalid_argument("ECC status array outside bounds");
}

inline EccControlControllerStatus ControllerStatus(const JsonValue &v) {
    Fields(v,{"state","transition","error","outcome_unknown","needs_reconciliation","operation_id","message"});
    EccControlControllerStatus status;
    status.state=Number(v.At("state"),6); status.transition=Number(v.At("transition"),17); status.error=Number(v.At("error"));
    status.outcomeUnknown=Boolean(v.At("outcome_unknown")); status.needsReconciliation=Boolean(v.At("needs_reconciliation"));
    status.operationId=String(v.At("operation_id")); if (!status.operationId.empty()) Id(status.operationId);
    status.message=String(v.At("message")); DiagnosticText(status.message,16384);
    return status;
}


inline EccControlDataNodeStatus Lifecycle(const JsonValue &life,const std::string &plan,std::set<std::string> &receiverIds,std::size_t &receivers) {
    EccControlDataNodeStatus n; n.planSha256=plan; Fields(life,{"schema","state","needs_reconciliation","plan_sha256","session_id","message","receivers"});
    if (Number(life.At("schema"))!=1 || String(life.At("plan_sha256"))!=n.planSha256) throw std::invalid_argument("ECC lifecycle schema/plan differs");
    n.state=String(life.At("state"));
    if (n.state!="fresh" && n.state!="starting" && n.state!="ready" && n.state!="finishing" && n.state!="stopped" && n.state!="failed" && n.state!="aborted" && n.state!="retired") throw std::invalid_argument("Unsupported ECC data state");
    n.needsReconciliation=Boolean(life.At("needs_reconciliation")); n.sessionId=String(life.At("session_id")); if (!n.sessionId.empty()) Digest(n.sessionId);
    n.message=String(life.At("message")); DiagnosticText(n.message,16384); Array(life.At("receivers"),195);
    receivers+=life.At("receivers").items.size(); if (receivers>195) throw std::invalid_argument("ECC whole-system receiver count exceeds 195");
    for (const auto &receiver:life.At("receivers").items) {
        std::set<std::string> fields={"id","session_id","output_directory","pid","exit_code","reaped","seal_verified","received_bytes","frames","files"};
        if (receiver.members.count("source_unused_confirmed")) fields.insert("source_unused_confirmed");
        if (receiver.members.count("source_end_confirmed")) fields.insert("source_end_confirmed");
        if (receiver.type!=JsonValue::Object || receiver.members.size()!=fields.size()) throw std::invalid_argument("Unexpected ECC receiver fields");
        for (const auto &field:fields) receiver.At(field);
        EccControlReceiverStatus r;
        r.id=String(receiver.At("id")); Text(r.id,128); if (!receiverIds.insert(r.id).second) throw std::invalid_argument("Duplicate ECC whole-system receiver ID");
        r.sessionId=String(receiver.At("session_id")); Digest(r.sessionId); if (r.sessionId!=n.sessionId) throw std::invalid_argument("ECC receiver session differs from lifecycle");
        r.outputDirectory=String(receiver.At("output_directory")); Text(r.outputDirectory,3500);
        r.pid=int(Number(receiver.At("pid"))); r.exitCode=receiver.At("exit_code").type==JsonValue::Number && receiver.At("exit_code").text=="-1" ? -1 : int(Number(receiver.At("exit_code"),255));
        if (receiver.members.count("source_unused_confirmed")) r.sourceUnusedConfirmed=Boolean(receiver.At("source_unused_confirmed"));
        r.reaped=Boolean(receiver.At("reaped")); r.sealVerified=Boolean(receiver.At("seal_verified")); r.receivedBytes=Counter(receiver.At("received_bytes")); r.frames=Counter(receiver.At("frames")); r.files=Counter(receiver.At("files"));
        if (r.sourceUnusedConfirmed && (!r.reaped || r.exitCode!=0 || !r.sealVerified || r.receivedBytes || r.frames || r.files)) throw std::invalid_argument("Unused ECC receiver lacks empty exit proof");
        if (receiver.members.count("source_end_confirmed")) r.sourceEndConfirmed=Boolean(receiver.At("source_end_confirmed"));
        if (r.sourceEndConfirmed && (r.sourceUnusedConfirmed || !r.reaped || r.exitCode!=0 || !r.sealVerified)) throw std::invalid_argument("ICE ECC source end lacks verified exit/seal");
        n.receivers.push_back(r);
    }
    if(n.state=="retired") {
        if(n.needsReconciliation || n.sessionId.empty() || n.receivers.empty()) throw std::invalid_argument("Retired ECC node lacks original confirmed session");
        for(const auto &r:n.receivers)if(!r.reaped || r.pid<=0 || r.exitCode<0) throw std::invalid_argument("Retired ECC receiver lacks original reap proof");
    }
    return n;
}
}
}

#endif
