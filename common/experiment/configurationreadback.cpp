#include "configurationreadback.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include <limits>
#include <stdexcept>
#include <vector>
namespace GetDAQ {
namespace {
std::string String(const JsonValue &v) {
    if (v.type!=JsonValue::String) throw std::invalid_argument("Readback audit field must be a string");
    return v.text;
}
std::uint64_t Integer(const JsonValue &v,bool quoted=true) {
    if (v.type!=(quoted ? JsonValue::String : JsonValue::Number) || v.text.empty() || v.text.size()>20 ||
        v.text.find_first_not_of("0123456789")!=std::string::npos || (v.text.size()>1 && v.text[0]=='0'))
        throw std::invalid_argument("Readback audit requires canonical unsigned values");
    std::uint64_t result=0;
    for (const char c:v.text) {
        const auto digit=unsigned(c-'0');
        if (result>(std::numeric_limits<std::uint64_t>::max()-digit)/10) throw std::invalid_argument("Readback value overflow");
        result=result*10+digit;
    }
    return result;
}
void False(const JsonValue &v) {
    if (v.type!=JsonValue::Boolean || v.text!="false") throw std::invalid_argument("Audit makes unsupported automatic readback claim");
}
bool SameTarget(const JsonValue &a,const JsonValue &b) {
    for (const auto key:{"device_proxy","device","register","field"}) if (String(a.At(key))!=String(b.At(key))) return false;
    return true;
}
}
ConfigurationReadbackEvidence DecodeConfigurationReadbackAudit(const std::string &bytes,const std::string &expectedChain) {
    if (expectedChain.size()!=64 || expectedChain.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Configure evidence requires the retained completed-execution digest");
    if (bytes.empty() || bytes.size()>8*1024*1024 || bytes.back()!='\n') throw std::invalid_argument("Readback audit missing or exceeds 8MiB");
    std::vector<JsonValue> rows;std::size_t begin=0;auto chain=ConfigurationSha256("getdaq.hardware-audit-chain:1\n");
    while (begin<bytes.size()) {
        const auto end=bytes.find('\n',begin);
        if (end==std::string::npos || end==begin || rows.size()>20482) throw std::invalid_argument("Readback audit framing/count invalid");
        chain=ConfigurationSha256(chain+'\n'+bytes.substr(begin,end-begin+1));
        rows.push_back(ParseStrictJson(bytes.substr(begin,end-begin)));begin=end+1;
    }
    if (chain!=expectedChain) throw std::invalid_argument("Configure audit differs from the completed hardware execution");
    if (rows.size()<4 || String(rows.front().At("phase"))!="intent" || Integer(rows.front().At("schema_version"),false)!=1 ||
        String(rows.back().At("phase"))!="complete") throw std::invalid_argument("Configure audit lacks completed intent");
    False(rows.back().At("automatic_readback"));
    const auto &steps=rows.front().At("steps");
    if (steps.type!=JsonValue::Array || steps.items.empty() || steps.items.size()>4096)
        throw std::invalid_argument("Configure audit step count invalid");
    ConfigurationReadbackEvidence result;result.auditSha256=ConfigurationSha256(bytes);result.auditChainSha256=chain;
    result.nodeProxy=String(rows.front().At("node"));
    if (result.nodeProxy.empty() || result.nodeProxy.size()>4096) throw std::invalid_argument("Readback node proxy outside bounds");
    result.startedAtUnixMs=Integer(rows.front().At("started_unix_ms"));result.steps=unsigned(steps.items.size());
    std::size_t cursor=1;
    const auto take=[&](std::size_t step,const char *phase) -> const JsonValue & {
        if (cursor>=rows.size()-1) throw std::invalid_argument("Configure audit ended before step result");
        const auto &row=rows[cursor++];
        if (Integer(row.At("step"),false)!=step || String(row.At("phase"))!=phase)
            throw std::invalid_argument("Configure audit intermediate step order differs");
        return row;
    };
    for (std::size_t i=0;i<steps.items.size();++i) {
        const auto &step=steps.items[i];take(i,"begin");
        const auto operation=String(step.At("operation"));
        if (operation=="masked-update") {
            const auto &observed=take(i,"observed");
            const auto previous=Integer(observed.At("previous")),effective=Integer(observed.At("effective_write_value"));
            const auto mask=Integer(step.At("mask")),value=Integer(step.At("value"));
            if (!mask || (value&~mask) || effective!=((previous&~mask)|value)) throw std::invalid_argument("Masked Configure update calculation differs");
            take(i,"write_begin");False(take(i,"acknowledged").At("readback_verified"));
            if (Integer(take(i,"matched").At("actual"))!=effective) throw std::invalid_argument("Masked Configure update readback differs");
            ++result.writesAcknowledged;++result.writesReadbackVerified;++result.maskedUpdatesVerified;continue;
        }
        if (cursor>=rows.size()-1) throw std::invalid_argument("Configure audit lacks terminal result");
        const auto &finished=rows[cursor++];
        if (Integer(finished.At("step"),false)!=i)
            throw std::invalid_argument("Configure audit step order differs");
        const auto phase=String(finished.At("phase"));
        if (operation=="write" || operation.compare(0,7,"pulser-")==0) {
            if (phase!="acknowledged") throw std::invalid_argument("Configure write/manager action not acknowledged");
            False(finished.At("readback_verified"));
            if (operation=="write") ++result.writesAcknowledged;else ++result.managerCommandsAcknowledged;
        } else if (operation=="delay") {
            if (phase!="completed") throw std::invalid_argument("Configure delay not completed");
            ++result.delaysCompleted;
        } else if (operation=="read") {
            if (phase!="observed") throw std::invalid_argument("Configure observation did not complete");
            Integer(finished.At("actual"));++result.readOnlyObservations;
        } else if (operation=="expect") {
            const auto actual=Integer(finished.At("actual")),expected=Integer(step.At("value")),mask=Integer(step.At("mask")),allowed=Integer(step.At("allowed_difference"));
            const auto difference=(actual^expected)&mask;
            if (!mask || (allowed&~mask) || Integer(finished.At("difference"))!=difference || (difference&~allowed) ||
                phase!=(difference ? "matched_with_warning" : "matched"))
                throw std::invalid_argument("Configure readback mismatch/inconsistent evidence");
            ++result.checksMatched;if (difference) ++result.checksWithWarnings;
            if (i && String(steps.items[i-1].At("operation"))=="write" && SameTarget(steps.items[i-1],step) &&
                Integer(steps.items[i-1].At("value"))==expected && Integer(steps.items[i-1].At("mask"))==mask && !difference)
                ++result.writesReadbackVerified;
        } else throw std::invalid_argument("Unsupported Configure audit operation; no verification claim");
    }
    if (cursor!=rows.size()-1) throw std::invalid_argument("Unexpected records after Configure steps");
    return result;
}
std::string EncodeConfigurationReadbackEvidence(const ConfigurationReadbackEvidence &e) {
    if (e.auditSha256.size()!=64 || e.auditSha256.find_first_not_of("0123456789abcdef")!=std::string::npos ||
        e.auditChainSha256.size()!=64 || e.auditChainSha256.find_first_not_of("0123456789abcdef")!=std::string::npos ||
        !e.steps || e.steps>4096 || e.writesReadbackVerified>e.writesAcknowledged || e.checksWithWarnings>e.checksMatched ||
        e.maskedUpdatesVerified>e.writesReadbackVerified ||
        e.writesAcknowledged+e.checksMatched+e.managerCommandsAcknowledged+e.delaysCompleted+e.readOnlyObservations!=e.steps)
        throw std::invalid_argument("Invalid configure evidence counters");
    return "{\"audit_sha256\":"+EncodeJsonString(e.auditSha256)+",\"audit_chain_sha256\":"+EncodeJsonString(e.auditChainSha256)+",\"node_proxy\":"+EncodeJsonString(e.nodeProxy)+
        ",\"started_at_unix_ms\":"+EncodeJsonString(std::to_string(e.startedAtUnixMs))+
        ",\"steps\":"+std::to_string(e.steps)+",\"writes_acknowledged\":"+std::to_string(e.writesAcknowledged)+
        ",\"writes_readback_verified\":"+std::to_string(e.writesReadbackVerified)+
        ",\"writes_without_full_readback\":"+std::to_string(e.writesAcknowledged-e.writesReadbackVerified)+
        ",\"checks_matched\":"+std::to_string(e.checksMatched)+",\"checks_with_warnings\":"+std::to_string(e.checksWithWarnings)+
        ",\"manager_commands_acknowledged\":"+std::to_string(e.managerCommandsAcknowledged)+",\"delays_completed\":"+std::to_string(e.delaysCompleted)+
        ",\"read_only_observations\":"+std::to_string(e.readOnlyObservations)+",\"masked_updates_verified\":"+std::to_string(e.maskedUpdatesVerified)+"}";
}
}
