#include "controlprotocol.h"
#include "planio.h"
#include "supervisorcommon.h"
#include "systemexperimentconfiguration.h"
#include <limits>
#include <set>
#include <stdexcept>
namespace GetDAQ {
std::string EncodeSupervisorStatus(const DataSupervisorStatus &s) {
    const char *states[]={"fresh","starting","ready","finishing","stopped","failed","aborted","retired"};
    if (static_cast<unsigned int>(s.state)>static_cast<unsigned int>(DataSupervisorStatus::Retired)) throw std::invalid_argument("Invalid supervisor state");
    std::string json="{\"schema\":1,\"state\":"+EncodeJsonString(states[s.state])+",\"needs_reconciliation\":"+(s.needsReconciliation ? "true" : "false")+
        ",\"plan_sha256\":"+EncodeJsonString(s.planSha256)+",\"session_id\":"+EncodeJsonString(s.sessionId)+",\"message\":"+EncodeJsonString(s.message)+",\"receivers\":[";
    for (const auto &r:s.receivers) {
        if (json.back()!='[') json+=',';
        json+="{\"id\":"+EncodeJsonString(r.id)+",\"session_id\":"+EncodeJsonString(r.sessionId)+",\"output_directory\":"+EncodeJsonString(r.outputDirectory)+
            ",\"pid\":"+std::to_string(r.pid)+",\"exit_code\":"+std::to_string(r.exitCode)+",\"reaped\":"+(r.reaped ? "true" : "false")+
            ",\"seal_verified\":"+(r.sealVerified ? "true" : "false")+",\"received_bytes\":"+EncodeJsonString(std::to_string(r.receivedBytes))+
            ",\"frames\":"+EncodeJsonString(std::to_string(r.frames))+",\"files\":"+EncodeJsonString(std::to_string(r.files))+
            (r.sourceUnusedConfirmed ? ",\"source_unused_confirmed\":true" : "")+(r.sourceEndConfirmed ? ",\"source_end_confirmed\":true" : "")+"}";
    }
    return json+"]}\n";
}
DataSupervisorStatus DecodeSupervisorStatus(const JsonValue &v) {
    using namespace SupervisorIO;
    ExactSupervisorFields(v,{"schema","state","needs_reconciliation","plan_sha256","session_id","message","receivers"});
    if (Unsigned(v.At("schema"))!=1) throw std::invalid_argument("Unsupported supervisor status schema");
    const std::string states[]={"fresh","starting","ready","finishing","stopped","failed","aborted","retired"};
    const auto state=String(v.At("state")); unsigned int index=0;
    while (index<8 && states[index]!=state) ++index;
    if (index==8) throw std::invalid_argument("Invalid supervisor status state");
    DataSupervisorStatus s; s.state=DataSupervisorStatus::State(index); s.needsReconciliation=Boolean(v.At("needs_reconciliation"));
    s.planSha256=String(v.At("plan_sha256")); Digest(s.planSha256); s.sessionId=String(v.At("session_id")); if (!s.sessionId.empty()) Digest(s.sessionId);
    s.message=String(v.At("message")); if (!s.message.empty()) Text(s.message,16384);
    if (v.At("receivers").type!=JsonValue::Array || v.At("receivers").items.size()>195) throw std::invalid_argument("Invalid supervisor status receivers");
    std::set<std::string> ids;
    for (const auto &entry:v.At("receivers").items) {
        std::set<std::string> fields={"id","session_id","output_directory","pid","exit_code","reaped","seal_verified","received_bytes","frames","files"};
        if (entry.members.count("source_unused_confirmed")) fields.insert("source_unused_confirmed");
        if (entry.members.count("source_end_confirmed")) fields.insert("source_end_confirmed");
        ExactSupervisorFields(entry,fields);
        ManagedReceiverStatus r; r.id=String(entry.At("id")); Text(r.id,128);
        if (!ids.insert(r.id).second) throw std::invalid_argument("Duplicate supervisor receiver status ID");
        r.sessionId=String(entry.At("session_id")); if (!r.sessionId.empty()) Digest(r.sessionId);
        r.outputDirectory=String(entry.At("output_directory")); if (!r.outputDirectory.empty()) Path(r.outputDirectory);
        const auto pid=Unsigned(entry.At("pid"));
        if (pid>unsigned(std::numeric_limits<int>::max())) throw std::invalid_argument("Invalid receiver PID range");
        r.pid=int(pid);
        if (entry.At("exit_code").type==JsonValue::Number && entry.At("exit_code").text=="-1") r.exitCode=-1;
        else { const auto code=Unsigned(entry.At("exit_code")); if (code>255) throw std::invalid_argument("Invalid receiver exit code"); r.exitCode=int(code); }
        r.reaped=Boolean(entry.At("reaped")); r.sealVerified=Boolean(entry.At("seal_verified"));
        if (entry.members.count("source_unused_confirmed")) r.sourceUnusedConfirmed=Boolean(entry.At("source_unused_confirmed"));
        if (entry.members.count("source_end_confirmed")) r.sourceEndConfirmed=Boolean(entry.At("source_end_confirmed"));
        if (r.sourceEndConfirmed && (r.sourceUnusedConfirmed || !r.reaped || r.exitCode!=0 || !r.sealVerified)) throw std::invalid_argument("ICE source end lacks verified normal exit/seal");
        r.receivedBytes=Unsigned(entry.At("received_bytes"),true); r.frames=Unsigned(entry.At("frames"),true); r.files=Unsigned(entry.At("files"),true);
        if (r.sourceUnusedConfirmed && (!r.reaped || r.exitCode!=0 || !r.sealVerified || r.receivedBytes || r.frames || r.files)) throw std::invalid_argument("Unused receiver status lacks empty normal-exit proof");
        s.receivers.push_back(r);
    }
    if (s.state==DataSupervisorStatus::Retired) {
        if (s.needsReconciliation || s.sessionId.empty() || s.receivers.empty())
            throw std::invalid_argument("Retired status lacks a confirmed original failed session");
        for (const auto &r:s.receivers)
            if (!r.reaped || r.pid<=0 || r.exitCode<0 || r.sessionId!=s.sessionId)
                throw std::invalid_argument("Retired status lacks original reaped children");
    }
    return s;
}
std::string EncodeSupervisorObservation(const DataSupervisorObservation &o) {
    std::string json="{\"schema\":1,\"busy\":"+std::string(o.busy ? "true" : "false")+
        ",\"observed_at_unix_ms\":"+EncodeJsonString(std::to_string(o.observedAtUnixMs))+",\"lifecycle\":"+EncodeSupervisorStatus(o.lifecycle)+",\"receivers\":[";
    for (const auto &r:o.receivers) {
        if (json.back()!='[') json+=',';
        json+="{\"id\":"+EncodeJsonString(r.id)+",\"pid\":"+std::to_string(r.pid)+",\"state\":"+EncodeJsonString(r.state)+
            ",\"exit_code\":"+std::to_string(r.exitCode)+",\"signal\":"+std::to_string(r.signal)+",\"message\":"+EncodeJsonString(r.message)+"}";
    }
    return json+"]}\n";
}
DataSupervisorObservation DecodeSupervisorObservation(const JsonValue &v) {
    using namespace SupervisorIO;
    ExactSupervisorFields(v,{"schema","busy","observed_at_unix_ms","lifecycle","receivers"});
    if (Unsigned(v.At("schema"))!=1) throw std::invalid_argument("Unsupported receiver observation schema");
    DataSupervisorObservation o; o.busy=Boolean(v.At("busy")); o.observedAtUnixMs=Unsigned(v.At("observed_at_unix_ms"),true);
    o.lifecycle=DecodeSupervisorStatus(v.At("lifecycle")); const auto &rows=v.At("receivers");
    if (rows.type!=JsonValue::Array || (o.busy ? !rows.items.empty() : rows.items.size()!=o.lifecycle.receivers.size()))
        throw std::invalid_argument("Receiver observation count differs from lifecycle/busy state");
    for (std::size_t i=0;i<rows.items.size();++i) {
        const auto &row=rows.items[i]; const auto &child=o.lifecycle.receivers[i];
        ExactSupervisorFields(row,{"id","pid","state","exit_code","signal","message"});
        ReceiverProcessObservation r; r.id=String(row.At("id")); const auto pid=Unsigned(row.At("pid"));
        if (pid>unsigned(std::numeric_limits<int>::max()) || r.id!=child.id || int(pid)!=child.pid)
            throw std::invalid_argument("Receiver observation identity differs from lifecycle");
        r.pid=int(pid); r.state=String(row.At("state")); r.message=String(row.At("message"));
        if (!r.message.empty()) Text(r.message,512);
        if (row.At("exit_code").type==JsonValue::Number && row.At("exit_code").text=="-1") r.exitCode=-1;
        else { const auto code=Unsigned(row.At("exit_code")); if (code>255) throw std::invalid_argument("Invalid observed exit code"); r.exitCode=int(code); }
        const auto signal=Unsigned(row.At("signal")); if (signal>127) throw std::invalid_argument("Invalid observed signal"); r.signal=int(signal);
        const bool reaped=r.state=="reaped", exited=r.state=="exited", absent=r.state=="not-started", running=r.state=="running", unavailable=r.state=="unavailable";
        if ((!reaped && !exited && !absent && !running && !unavailable) ||
            (reaped && (!child.reaped || r.exitCode!=child.exitCode || r.exitCode<0 || r.signal)) ||
            (!reaped && child.reaped) || (absent && r.pid!=0) || ((running || exited) && r.pid<=0) ||
            (exited && (r.exitCode<0 || (r.signal && r.exitCode!=128+r.signal))) ||
            (!reaped && !exited && (r.exitCode!=-1 || r.signal)) || (unavailable && r.message.empty()) || (!unavailable && !r.message.empty()))
            throw std::invalid_argument("Invalid receiver process observation state");
        o.receivers.push_back(r);
    }
    return o;
}
}
