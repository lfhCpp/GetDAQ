#include "planio.h"
#include "supervisorcommon.h"
#include <limits>
#include <stdexcept>
namespace GetDAQ {
void ExactSupervisorFields(const JsonValue &object,const std::set<std::string> &fields) {
    if (object.type!=JsonValue::Object) throw std::invalid_argument("Supervisor value is not an object");
    if (object.members.size()!=fields.size()) {
        std::string message="Supervisor object has missing/extra fields; expected=";
        for (const auto &field:fields) message+=field+",";
        message+=" actual=";
        for (const auto &entry:object.members) message+=entry.first+",";
        throw std::invalid_argument(message);
    }
    for (const auto &entry:object.members) if (!fields.count(entry.first)) throw std::invalid_argument("Unsupported supervisor field: "+entry.first);
}
unsigned int SupervisorUnsigned32(const JsonValue &value) {
    const auto n=SupervisorIO::Unsigned(value);
    if (n>std::numeric_limits<unsigned int>::max()) throw std::invalid_argument("Supervisor 32-bit integer overflow");
    return static_cast<unsigned int>(n);
}
SupervisorPlanDocument DecodeSupervisorPlan(const JsonValue &v) {
    using namespace SupervisorIO;
    std::set<std::string> fields={"schema","kind","deployment_id","audit_root","executable","executable_sha256","configuration_sha256","artifact_sha256",
        "startup_timeout_ms","finish_timeout_ms","status_timeout_ms","max_total_queue_mib","max_total_frame_mib","sources"};
    if (v.type==JsonValue::Object && v.members.count("catalog_root")) fields.insert("catalog_root");
    if (v.type==JsonValue::Object && v.members.count("ice_executable")) fields.insert("ice_executable");
    if (v.type==JsonValue::Object && v.members.count("ice_executable_sha256")) fields.insert("ice_executable_sha256");
    ExactSupervisorFields(v,fields);
    if (Unsigned(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.data-supervisor-plan") throw std::invalid_argument("Unsupported supervisor plan version/kind");
    SupervisorPlanDocument d; auto &o=d.options;
    o.deploymentId=String(v.At("deployment_id")); Text(o.deploymentId,128);
    o.auditRoot=String(v.At("audit_root")); Path(o.auditRoot);
    o.receiverExecutable=String(v.At("executable")); Path(o.receiverExecutable);
    if (fields.count("ice_executable")) o.iceReceiverExecutable=String(v.At("ice_executable"));
    if (fields.count("ice_executable_sha256")) o.iceReceiverSha256=String(v.At("ice_executable_sha256"));
    d.executableSha256=String(v.At("executable_sha256")); Digest(d.executableSha256);
    o.configurationSha256=String(v.At("configuration_sha256")); Digest(o.configurationSha256);
    o.artifactSha256=String(v.At("artifact_sha256")); Digest(o.artifactSha256);
    if (fields.count("catalog_root")) { o.catalogRoot=String(v.At("catalog_root")); Path(o.catalogRoot); }
    o.startupTimeoutMs=SupervisorUnsigned32(v.At("startup_timeout_ms")); o.finishTimeoutMs=SupervisorUnsigned32(v.At("finish_timeout_ms"));
    o.statusTimeoutMs=SupervisorUnsigned32(v.At("status_timeout_ms"));
    o.maxTotalQueueMiB=Unsigned(v.At("max_total_queue_mib")); o.maxTotalFrameMiB=Unsigned(v.At("max_total_frame_mib"));
    if (v.At("sources").type!=JsonValue::Array || v.At("sources").items.empty() || v.At("sources").items.size()>195) throw std::invalid_argument("Invalid supervisor source array");
    for (const auto &source:v.At("sources").items) {
        std::set<std::string> sourceFields={"id","receiver_address","bind","output_root","data_port","status_port","export_port","queue_mib","max_frame_mib","rotate_mib","export_queue_mib"};
        if (source.type==JsonValue::Object && source.members.count("file_naming")) sourceFields.insert("file_naming");
        if (source.type==JsonValue::Object && source.members.count("input_protocol")) sourceFields.insert("input_protocol");
        if (source.type==JsonValue::Object && source.members.count("storage_mode")) sourceFields.insert("storage_mode");
        if (source.type==JsonValue::Object && source.members.count("min_free_mib")) sourceFields.insert("min_free_mib");
        ExactSupervisorFields(source,sourceFields);
        ReceiverSourcePlan s;
        s.id=String(source.At("id")); Text(s.id,128); s.receiverAddress=String(source.At("receiver_address")); Text(s.receiverAddress,64);
        s.bindAddress=String(source.At("bind")); Text(s.bindAddress,64); s.outputRoot=String(source.At("output_root")); Path(s.outputRoot);
        s.dataPort=SupervisorUnsigned32(source.At("data_port")); s.statusPort=SupervisorUnsigned32(source.At("status_port")); s.exportPort=SupervisorUnsigned32(source.At("export_port"));
        s.queueMiB=SupervisorUnsigned32(source.At("queue_mib")); s.maxFrameMiB=SupervisorUnsigned32(source.At("max_frame_mib"));
        s.rotateMiB=Unsigned(source.At("rotate_mib")); s.exportQueueMiB=SupervisorUnsigned32(source.At("export_queue_mib"));
        if (sourceFields.count("min_free_mib")) s.minFreeMiB=Unsigned(source.At("min_free_mib"));
        if (sourceFields.count("file_naming")) s.fileNaming=String(source.At("file_naming"));
        if (s.fileNaming!="native" && s.fileNaming!="get") throw std::invalid_argument("Supervisor file naming must be native or get");
        if (sourceFields.count("input_protocol")) s.inputProtocol=String(source.At("input_protocol"));
        if (s.inputProtocol!="TCP" && s.inputProtocol!="ZBUF" && s.inputProtocol!="FDT" && s.inputProtocol!="ICE") throw std::invalid_argument("Managed supervisor input protocol must be TCP, ZBUF, FDT or ICE");
        if (sourceFields.count("storage_mode")) s.storageMode=String(source.At("storage_mode"));
        o.sources.push_back(s);
    }
    ValidatePlanOptions(o); // 控制主机检查远端计划，不用本机网卡去否定数据主机地址。
    return d;
}
SupervisorPlanDocument DecodeSupervisorPlan(const std::string &bytes) {
    if (bytes.size()>1024*1024) throw std::invalid_argument("Supervisor plan exceeds 1 MiB");
    return DecodeSupervisorPlan(ParseStrictJson(bytes));
}
}
