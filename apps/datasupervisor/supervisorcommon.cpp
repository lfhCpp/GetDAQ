#include "supervisorcommon.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>
namespace GetDAQ { namespace SupervisorIO {
void Text(const std::string &s,std::size_t maximum) {
    if (s.empty() || s.size()>maximum) throw std::invalid_argument("Invalid bounded supervisor text");
    ValidateJsonUtf8(s);
    for (unsigned char c:s) if (c<32 || c==127) throw std::invalid_argument("Control byte in supervisor text");
}
void Digest(const std::string &s) {
    if (s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Supervisor requires lowercase SHA-256 values");
}
void Path(const std::string &s) {
    Text(s,3500);
    if (s.size()<2 || s[0]!='/' || s.back()=='/' || s.find("//")!=std::string::npos || s.find("/./")!=std::string::npos ||
        s.find("/../")!=std::string::npos || s.substr(s.size()-2)=="/." || (s.size()>=3 && s.substr(s.size()-3)=="/.."))
        throw std::invalid_argument("Supervisor path must be absolute and normalized");
}
std::uint64_t Unsigned(const JsonValue &v,bool string) {
    if (v.type!=(string ? JsonValue::String : JsonValue::Number) || v.text.empty() || v.text.size()>20 ||
        v.text.find_first_not_of("0123456789")!=std::string::npos || (v.text.size()>1 && v.text[0]=='0'))
        throw std::runtime_error("Invalid supervisor unsigned JSON value");
    std::uint64_t result=0;
    for (char c:v.text) {
        if (result>(UINT64_MAX-unsigned(c-'0'))/10) throw std::runtime_error("Supervisor integer overflow");
        result=result*10+unsigned(c-'0');
    }
    return result;
}
std::string String(const JsonValue &v) {
    if (v.type!=JsonValue::String) throw std::runtime_error("Expected supervisor JSON string");
    return v.text;
}
bool Boolean(const JsonValue &v) {
    if (v.type!=JsonValue::Boolean) throw std::runtime_error("Expected supervisor JSON boolean");
    return v.text=="true";
}
namespace {
bool ParseIPv4Octets(const std::string &text,unsigned char octets[4]) {
    std::size_t begin=0;
    for (unsigned i=0;i<4;++i) {
        const auto end=i==3 ? text.size() : text.find('.',begin);
        if (end==std::string::npos || end==begin || end-begin>3) return false;
        const auto part=text.substr(begin,end-begin);
        if (part.find_first_not_of("0123456789")!=std::string::npos || (part.size()>1 && part[0]=='0')) return false;
        unsigned value=0;
        for (char c:part) { value=value*10+unsigned(c-'0'); if (value>255) return false; }
        octets[i]=static_cast<unsigned char>(value); begin=end+1;
    }
    return begin==text.size()+1;
}
}
bool ParseCanonicalIPv4(const std::string &text,std::uint32_t &hostOrderAddress) {
    unsigned char octets[4]={};
    if (!ParseIPv4Octets(text,octets)) return false;
    hostOrderAddress=(std::uint32_t(octets[0])<<24)|(std::uint32_t(octets[1])<<16)|
        (std::uint32_t(octets[2])<<8)|std::uint32_t(octets[3]);
    return true;
}
void ValidatePlanOptions(const DataSupervisorOptions &o) {
    Path(o.auditRoot); Path(o.receiverExecutable); Text(o.deploymentId,128); Digest(o.configurationSha256); Digest(o.artifactSha256);
    if (!o.catalogRoot.empty()) Path(o.catalogRoot);
    const bool ice=std::any_of(o.sources.begin(),o.sources.end(),[](const ReceiverSourcePlan &s) {return s.inputProtocol=="ICE";});
    if (ice) { Path(o.iceReceiverExecutable); Digest(o.iceReceiverSha256); }
    else if (!o.iceReceiverExecutable.empty() || !o.iceReceiverSha256.empty()) throw std::invalid_argument("ICE executable binding requires an ICE source");
    if (o.sources.empty() || o.sources.size()>195 || o.startupTimeoutMs<100 || o.startupTimeoutMs>600000 || o.finishTimeoutMs<100 || o.finishTimeoutMs>600000 ||
        o.statusTimeoutMs<100 || o.statusTimeoutMs>10000 || o.maxTotalQueueMiB==0 || o.maxTotalFrameMiB==0)
        throw std::invalid_argument("Invalid supervisor sources/time/budget");
    std::set<std::string> ids;
    std::set<unsigned int> ports;
    std::uint64_t queue=0,frames=0;
    for (const auto &s:o.sources) {
        Text(s.id,128); Path(s.outputRoot);
        if (!ids.insert(s.id).second) throw std::invalid_argument("Duplicate supervisor source ID");
        const auto colon=s.receiverAddress.find(':');
        const auto target=s.receiverAddress.substr(0,colon);
        std::uint32_t address=0;
        if (colon==std::string::npos || s.receiverAddress!=target+':'+std::to_string(s.dataPort) ||
            !ParseCanonicalIPv4(target,address) || address==0 || (address>>24)>=224 ||
            (s.bindAddress!="0.0.0.0" && s.bindAddress!=target))
            throw std::invalid_argument("Supervisor requires explicit IPv4 target/bind/port; remote host needs a remote supervisor adapter");
        for (auto port:{s.dataPort,s.statusPort,s.exportPort}) {
            if (!port && port==s.exportPort && s.exportPort==0) continue;
            if (!port || port>65535 || !ports.insert(port).second) throw std::invalid_argument("Duplicate/invalid supervisor port");
        }
        if (!s.dataPort || !s.statusPort || s.queueMiB<1 || s.queueMiB>4096 || s.maxFrameMiB<1 || s.maxFrameMiB>1024 ||
            s.rotateMiB<1 || s.rotateMiB>1048576 || s.exportQueueMiB<1 || s.exportQueueMiB>1024 || s.minFreeMiB>1048576)
            throw std::invalid_argument("Invalid supervisor receiver memory/rotation");
        if (s.fileNaming!="native" && s.fileNaming!="get") throw std::invalid_argument("Invalid supervisor file naming");
        if (s.inputProtocol!="TCP" && s.inputProtocol!="ZBUF" && s.inputProtocol!="FDT" && s.inputProtocol!="ICE")
            throw std::invalid_argument("Invalid managed supervisor input protocol");
        if (s.storageMode!="frame-storage" && s.storageMode!="byte-storage") throw std::invalid_argument("Invalid managed receiver storage mode");
        if (s.storageMode=="byte-storage" && (s.inputProtocol=="FDT" || s.exportPort))
            throw std::invalid_argument("Byte storage requires TCP, ZBUF or ICE input and disables frame export");
        queue+=s.queueMiB+(s.exportPort ? s.exportQueueMiB : 0); frames+=s.maxFrameMiB;
    }
    if (queue>o.maxTotalQueueMiB || frames>o.maxTotalFrameMiB) throw std::invalid_argument("Supervisor aggregate pool/frame budget exceeded");
}
std::string EncodeOptions(const DataSupervisorOptions &o,const std::string &executable) {
    ValidatePlanOptions(o); Digest(executable);
    std::string result="{\"schema\":1,\"kind\":\"getdaq.data-supervisor-plan\",\"deployment_id\":"+EncodeJsonString(o.deploymentId)+
        ",\"audit_root\":"+EncodeJsonString(o.auditRoot)+
        ",\"executable\":"+EncodeJsonString(o.receiverExecutable)+",\"executable_sha256\":"+EncodeJsonString(executable)+
        ",\"configuration_sha256\":"+EncodeJsonString(o.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(o.artifactSha256)+
        ",\"startup_timeout_ms\":"+std::to_string(o.startupTimeoutMs)+",\"finish_timeout_ms\":"+std::to_string(o.finishTimeoutMs)+
        ",\"status_timeout_ms\":"+std::to_string(o.statusTimeoutMs)+",\"max_total_queue_mib\":"+std::to_string(o.maxTotalQueueMiB)+
        ",\"max_total_frame_mib\":"+std::to_string(o.maxTotalFrameMiB);
    if (!o.iceReceiverExecutable.empty()) result+=",\"ice_executable\":"+EncodeJsonString(o.iceReceiverExecutable)+",\"ice_executable_sha256\":"+EncodeJsonString(o.iceReceiverSha256);
    if (!o.catalogRoot.empty()) result+=",\"catalog_root\":"+EncodeJsonString(o.catalogRoot);
    result+=",\"sources\":[";
    for (const auto &s:o.sources) {
        if (result.back()!='[') result+=',';
        result+="{\"id\":"+EncodeJsonString(s.id)+",\"receiver_address\":"+EncodeJsonString(s.receiverAddress)+",\"bind\":"+EncodeJsonString(s.bindAddress)+
            ",\"output_root\":"+EncodeJsonString(s.outputRoot)+",\"data_port\":"+std::to_string(s.dataPort)+",\"status_port\":"+std::to_string(s.statusPort)+
            ",\"export_port\":"+std::to_string(s.exportPort)+",\"queue_mib\":"+std::to_string(s.queueMiB)+",\"max_frame_mib\":"+std::to_string(s.maxFrameMiB)+
            ",\"rotate_mib\":"+std::to_string(s.rotateMiB)+",\"export_queue_mib\":"+std::to_string(s.exportQueueMiB);
        if (s.minFreeMiB) result+=",\"min_free_mib\":"+std::to_string(s.minFreeMiB);
        if (s.fileNaming!="native") result+=",\"file_naming\":"+EncodeJsonString(s.fileNaming);
        if (s.inputProtocol!="TCP") result+=",\"input_protocol\":"+EncodeJsonString(s.inputProtocol);
        if (s.storageMode!="frame-storage") result+=",\"storage_mode\":"+EncodeJsonString(s.storageMode);
        result+="}";
    }
    return result+"]}\n";
}
} }
