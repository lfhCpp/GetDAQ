#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"
#include "systemexperimentconfiguration.h"
#include <set>
#include <stdexcept>

namespace GetDAQ {
using namespace EccProtocolInternal;
EccControlConfiguration DecodeEccControlConfiguration(const std::string &bytes) {
    const auto v=Parse(bytes,65536);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","input_binding_sha256","configuration_sha256","artifact_sha256",
        "selection","input_binding_live_verified","hardware_loaded_verified"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-configuration" || Boolean(v.At("input_binding_live_verified")) || Boolean(v.At("hardware_loaded_verified")))
        throw std::invalid_argument("Unsupported ECC configuration cache schema/scope");
    EccControlConfiguration c;c.instanceId=String(v.At("instance_id"));c.snapshotSha256=String(v.At("snapshot_sha256"));
    c.inputBindingSha256=String(v.At("input_binding_sha256"));c.configurationSha256=String(v.At("configuration_sha256"));c.artifactSha256=String(v.At("artifact_sha256"));
    for (const auto &digest:{c.instanceId,c.snapshotSha256,c.inputBindingSha256,c.configurationSha256,c.artifactSha256}) Digest(digest);
    if (v.At("selection").type!=JsonValue::Null) {c.selection=DecodeConfigurationSelection(v.At("selection"));c.hasSelection=true;}
    if (c.hasSelection && (c.selection.registration.configurationSha256!=c.configurationSha256 || c.selection.registration.artifactSha256!=c.artifactSha256))
        throw std::invalid_argument("ECC selected configuration differs from startup artifact");
    return c;
}
EccControlOwnership DecodeEccControlOwnership(const std::string &bytes) {
    const auto v=Parse(bytes,65536);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","ownership"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-ownership")
        throw std::invalid_argument("Unsupported ECC ownership cache");
    EccControlOwnership result;
    result.instanceId=String(v.At("instance_id")); result.snapshotSha256=String(v.At("snapshot_sha256"));
    Digest(result.instanceId); Digest(result.snapshotSha256);
    const auto &o=v.At("ownership");
    Fields(o,{"enabled","scope","directory","held_at_last_check","fault_latched","cached", "board_global_lock_verified","cross_host_lock_verified","resources"});
    if (String(o.At("scope"))!="single-host-cooperating-ecc" || !Boolean(o.At("cached")) ||
        Boolean(o.At("board_global_lock_verified")) || Boolean(o.At("cross_host_lock_verified")))
        throw std::invalid_argument("Ownership cache makes unsupported global/live claims");
    result.enabled=Boolean(o.At("enabled")); result.heldAtLastCheck=Boolean(o.At("held_at_last_check"));
    result.faultLatched=Boolean(o.At("fault_latched")); result.directory=String(o.At("directory"));
    Text(result.directory,3300,!result.enabled);
    const auto &resources=o.At("resources");
    if (resources.type!=JsonValue::Array || resources.items.size()>256) throw std::invalid_argument("Invalid ownership resource list");
    for (const auto &item:resources.items) {
        const auto address=String(item); unsigned int octets[4]={}; std::size_t begin=0;
        for (unsigned int i=0;i<4;++i) {
            const auto end=address.find('.',begin);
            if ((i<3)==(end==std::string::npos)) throw std::invalid_argument("Invalid ownership IPv4 address");
            const auto part=address.substr(begin,end==std::string::npos ? end : end-begin);
            if (part.empty() || part.size()>3 || (part.size()>1 && part[0]=='0') || part.find_first_not_of("0123456789")!=std::string::npos)
                throw std::invalid_argument("Noncanonical ownership IPv4 address");
            for (const char c:part) octets[i]=octets[i]*10+unsigned(c-'0');
            if (octets[i]>255) throw std::invalid_argument("Ownership IPv4 octet outside range");
            begin=end+1;
        }
        if (!octets[0] || octets[0]>=224 || (!result.resources.empty() && result.resources.back()>=address))
            throw std::invalid_argument("Ownership resources must be unicast, sorted and unique");
        result.resources.push_back(address);
    }
    if (result.enabled) {
        if (result.directory.size()<2 || result.directory[0]!='/' || result.directory.back()=='/' || result.directory.find("//")!=std::string::npos ||
            result.directory.find("/./")!=std::string::npos || result.directory.find("/../")!=std::string::npos ||
            result.directory.substr(result.directory.size()-2)=="/." ||
            (result.directory.size()>=3 && result.directory.substr(result.directory.size()-3)=="/..") || result.resources.empty() ||
            result.heldAtLastCheck==result.faultLatched)
            throw std::invalid_argument("Inconsistent enabled ownership cache");
    } else if (!result.directory.empty() || !result.resources.empty() || result.heldAtLastCheck || result.faultLatched)
        throw std::invalid_argument("Disabled ownership cache contains a reservation");
    return result;
}
std::string EncodeEccControlConfiguration(const EccControlConfiguration &c) {
    const auto bytes="{\"schema\":1,\"kind\":\"getdaq.ecc-configuration\",\"instance_id\":"+EncodeJsonString(c.instanceId)+
        ",\"snapshot_sha256\":"+EncodeJsonString(c.snapshotSha256)+",\"input_binding_sha256\":"+EncodeJsonString(c.inputBindingSha256)+
        ",\"configuration_sha256\":"+EncodeJsonString(c.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(c.artifactSha256)+
        ",\"selection\":"+(c.hasSelection ? EncodeConfigurationSelection(c.selection) : "null")+",\"input_binding_live_verified\":false,\"hardware_loaded_verified\":false}\n";
    DecodeEccControlConfiguration(bytes);return bytes;
}
EccControlIdentity DecodeEccControlIdentity(const std::string &bytes) {
    const auto v=Parse(bytes,65536); Fields(v,{"schema","service","service_id","client_id","instance_id","snapshot_sha256","input_binding_sha256","configuration_sha256","artifact_sha256","data_binding_sha256"});
    if (Number(v.At("schema"))!=1 || String(v.At("service"))!="getdaq-ecc-server") throw std::invalid_argument("Unsupported ECC service identity");
    EccControlIdentity s; s.serviceId=String(v.At("service_id")); s.clientId=String(v.At("client_id")); Text(s.serviceId,128); Text(s.clientId,128);
    s.instanceId=String(v.At("instance_id")); s.snapshotSha256=String(v.At("snapshot_sha256")); s.inputBindingSha256=String(v.At("input_binding_sha256"));
    s.configurationSha256=String(v.At("configuration_sha256")); s.artifactSha256=String(v.At("artifact_sha256")); s.dataBindingSha256=String(v.At("data_binding_sha256"));
    for (const auto &digest:{s.instanceId,s.snapshotSha256,s.inputBindingSha256,s.configurationSha256,s.artifactSha256,s.dataBindingSha256}) Digest(digest);
    return s;
}
EccControlConfigureReadbacks DecodeEccControlConfigureReadbacks(const std::string &bytes) {
    if (bytes.size()>1024*1024) throw std::invalid_argument("Configure readback cache exceeds 1MiB");
    JsonLimits limits;limits.stringBytes=16384;const auto v=ParseStrictJson(bytes,limits);
    Fields(v,{"schema","kind","instance_id","snapshot_sha256","configuration_sha256","artifact_sha256","controller","readbacks"});
    if (Number(v.At("schema"))!=1 || String(v.At("kind"))!="getdaq.ecc-configure-readbacks") throw std::invalid_argument("Unsupported Configure readback cache");
    EccControlConfigureReadbacks result;
    result.instanceId=String(v.At("instance_id"));result.snapshotSha256=String(v.At("snapshot_sha256"));
    result.configurationSha256=String(v.At("configuration_sha256"));result.artifactSha256=String(v.At("artifact_sha256"));
    for (const auto &digest:{result.instanceId,result.snapshotSha256,result.configurationSha256,result.artifactSha256}) Digest(digest);
    result.controller=ControllerStatus(v.At("controller"));
    const auto &r=v.At("readbacks");Fields(r,{"phase","configure_operation_id","historical","live_hardware_checked","actual_loaded_configuration_verified","entries"});
    if (!Boolean(r.At("historical")) || Boolean(r.At("live_hardware_checked")) || Boolean(r.At("actual_loaded_configuration_verified")))
        throw std::invalid_argument("Configure evidence falsely claims live/full loading");
    result.phase=String(r.At("phase"));result.configureOperationId=String(r.At("configure_operation_id"));
    if (result.phase!="not-observed" && result.phase!="pending" && result.phase!="unavailable" && result.phase!="captured")
        throw std::invalid_argument("Unknown Configure capture phase");
    if (result.phase=="not-observed") {if (!result.configureOperationId.empty()) throw std::invalid_argument("Unobserved Configure has an operation");}
    else Id(result.configureOperationId);
    const auto &entries=r.At("entries");Array(entries,195);
    if (result.phase!="captured" && !entries.items.empty()) throw std::invalid_argument("Incomplete Configure cache contains completed entries");
    std::set<std::string> ids;
    for (const auto &entry:entries.items) {
        Fields(entry,{"id","stage","audit_file","evidence"});EccControlReadbackEntry item;
        item.id=String(entry.At("id"));Text(item.id,128);
        if(item.id=="*" || item.id.find_first_of("[]/")!=std::string::npos)throw std::invalid_argument("Invalid concrete Configure hardware ID");
        if(!ids.insert(item.id).second)throw std::invalid_argument("Duplicate Configure hardware entry");
        item.stage=String(entry.At("stage"));if(item.stage!="cobo-configure" && item.stage!="mutant-configure")throw std::invalid_argument("Non-Configure audit stage");
        item.auditFile=String(entry.At("audit_file"));Text(item.auditFile,128);
        if (item.auditFile.size()<16 || item.auditFile.compare(0,9,"hardware-") || item.auditFile.substr(item.auditFile.size()-6)!=".jsonl" ||
            item.auditFile.substr(9,item.auditFile.size()-15).find_first_not_of("0123456789")!=std::string::npos)
            throw std::invalid_argument("Unsafe Configure audit filename");
        const auto &e=entry.At("evidence");
        Fields(e,{"audit_sha256","audit_chain_sha256","node_proxy","started_at_unix_ms","steps","writes_acknowledged","writes_readback_verified",
            "writes_without_full_readback","checks_matched","checks_with_warnings","manager_commands_acknowledged","delays_completed","read_only_observations","masked_updates_verified"});
        auto &evidence=item.evidence;evidence.auditSha256=String(e.At("audit_sha256"));evidence.auditChainSha256=String(e.At("audit_chain_sha256"));
        Digest(evidence.auditSha256);Digest(evidence.auditChainSha256);evidence.nodeProxy=String(e.At("node_proxy"));Text(evidence.nodeProxy,4096);
        evidence.startedAtUnixMs=Counter(e.At("started_at_unix_ms"));evidence.steps=Number(e.At("steps"),4096);
        evidence.writesAcknowledged=Number(e.At("writes_acknowledged"),4096);evidence.writesReadbackVerified=Number(e.At("writes_readback_verified"),4096);
        const auto unverified=Number(e.At("writes_without_full_readback"),4096);
        evidence.checksMatched=Number(e.At("checks_matched"),4096);evidence.checksWithWarnings=Number(e.At("checks_with_warnings"),4096);
        evidence.managerCommandsAcknowledged=Number(e.At("manager_commands_acknowledged"),4096);evidence.delaysCompleted=Number(e.At("delays_completed"),4096);
        evidence.readOnlyObservations=Number(e.At("read_only_observations"),4096);evidence.maskedUpdatesVerified=Number(e.At("masked_updates_verified"),4096);
        if (evidence.writesReadbackVerified>evidence.writesAcknowledged || unverified!=evidence.writesAcknowledged-evidence.writesReadbackVerified)
            throw std::invalid_argument("Configure write coverage counters differ");
        EncodeConfigurationReadbackEvidence(evidence); // Reuses consistency/bounds, without trusting a full-loading flag.
        result.entries.push_back(item);
    }
    return result;
}

}
