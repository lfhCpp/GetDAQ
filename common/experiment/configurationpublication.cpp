#include "configurationpublication.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "effectiveconfigurationrecord.h"
#include <map>
#include <set>
#include <stdexcept>
#include <iomanip>
#include <sstream>
namespace GetDAQ {
namespace {
void Keys(const JsonValue &v,std::initializer_list<const char *> names) {
    if (v.type!=JsonValue::Object || v.members.size()!=names.size()) throw std::invalid_argument("Unexpected publication fields");
    for (const char *name:names) v.At(name);
}
std::string Text(const JsonValue &v) {
    if (v.type!=JsonValue::String || v.text.find('\0')!=std::string::npos) throw std::invalid_argument("Publication requires string field");
    return v.text;
}
void Digest(const std::string &s) {
    if (s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("Publication requires lowercase SHA-256");
}
std::uint64_t Size(const JsonValue &v) {
    const auto s=Text(v); std::uint64_t value=0;
    if (s.empty() || s.size()>9 || (s.size()>1 && s[0]=='0')) throw std::invalid_argument("Publication size must be canonical decimal");
    for (char c:s) { if (c<'0' || c>'9') throw std::invalid_argument("Invalid publication size"); value=value*10+unsigned(c-'0'); }
    return value;
}
PublicationFile File(const JsonValue &v) {
    Keys(v,{"stored","sha256","bytes"}); PublicationFile f; f.stored=Text(v.At("stored")); f.sha256=Text(v.At("sha256")); f.bytes=Size(v.At("bytes")); return f;
}
void CheckFile(const PublicationFile &f,const std::string &name,std::uint64_t maximum) {
    Digest(f.sha256); if (f.stored!=name || !f.bytes || f.bytes>maximum) throw std::invalid_argument("Invalid publication file name/size");
}
void Check(const ConfigurationPublication &p) {
    Digest(p.configurationSha256); Digest(p.effectiveSha256); Digest(p.snapshotSha256);
    CheckFile(p.artifact,"experiment.json",64*1024*1024); CheckFile(p.profile,"profile.json",1024*1024); CheckFile(p.effective,"effective.json",64*1024*1024);
    if (p.sources.empty() || p.sources.size()>256) throw std::invalid_argument("Publication requires 1..256 sources");
    std::string previous; bool root=false; std::uint64_t total=0;
    for (std::size_t i=0;i<p.sources.size();++i) {
        const auto &s=p.sources[i]; ValidateJsonUtf8(s.sourcePath);
        if (s.sourcePath.empty() || s.sourcePath.size()>4096 || s.sourcePath.find('\0')!=std::string::npos || (!previous.empty() && previous>=s.sourcePath)) throw std::invalid_argument("Invalid/unsorted/duplicate publication source");
        previous=s.sourcePath; root=root || s.sourcePath==p.rootSource;
        CheckFile(s.file,PublicationSourceName(i),8*1024*1024); total+=s.file.bytes;
    }
    if (!root || total>64*1024*1024) throw std::invalid_argument("Publication root/source byte budget differs");
}
std::string EncodeFile(const PublicationFile &f) {
    return "{\"stored\":"+EncodeJsonString(f.stored)+",\"sha256\":"+EncodeJsonString(f.sha256)+",\"bytes\":"+EncodeJsonString(std::to_string(f.bytes))+"}";
}
}
std::string PublicationSourceName(std::size_t index) {
    if (index>=256) throw std::invalid_argument("Publication source index limit");
    std::ostringstream out; out<<"source-"<<std::setw(3)<<std::setfill('0')<<index<<".xcfg"; return out.str();
}
std::string EncodeConfigurationPublication(const ConfigurationPublication &p) {
    Check(p);
    std::string result="{\"schema_version\":1,\"kind\":\"getdaq.system-configuration-publication\",\"root_source\":"+EncodeJsonString(p.rootSource)+
        ",\"configuration_sha256\":"+EncodeJsonString(p.configurationSha256)+",\"effective_sha256\":"+EncodeJsonString(p.effectiveSha256)+",\"snapshot_sha256\":"+EncodeJsonString(p.snapshotSha256)+
        ",\"artifact\":"+EncodeFile(p.artifact)+",\"profile\":"+EncodeFile(p.profile)+",\"effective_snapshot\":"+EncodeFile(p.effective)+",\"sources\":[";
    for (const auto &s:p.sources) { if (result.back()!='[') result+=','; result+="{\"source_path\":"+EncodeJsonString(s.sourcePath)+",\"file\":"+EncodeFile(s.file)+"}"; }
    result+="]}\n";
    if (result.size()>1024*1024) throw std::invalid_argument("Publication manifest exceeds 1 MiB");
    return result;
}
ConfigurationPublication DecodeConfigurationPublication(const std::string &bytes) {
    if (bytes.size()>1024*1024) throw std::invalid_argument("Publication manifest exceeds 1 MiB");
    const auto root=ParseStrictJson(bytes); Keys(root,{"schema_version","kind","root_source","configuration_sha256","effective_sha256","snapshot_sha256","artifact","profile","effective_snapshot","sources"});
    if (root.At("schema_version").type!=JsonValue::Number || root.At("schema_version").text!="1" || Text(root.At("kind"))!="getdaq.system-configuration-publication") throw std::invalid_argument("Unsupported publication manifest");
    ConfigurationPublication p; p.rootSource=Text(root.At("root_source")); p.configurationSha256=Text(root.At("configuration_sha256")); p.effectiveSha256=Text(root.At("effective_sha256")); p.snapshotSha256=Text(root.At("snapshot_sha256"));
    p.artifact=File(root.At("artifact")); p.profile=File(root.At("profile")); p.effective=File(root.At("effective_snapshot"));
    const auto &sources=root.At("sources"); if (sources.type!=JsonValue::Array || sources.items.size()>256) throw std::invalid_argument("Invalid publication source array");
    for (const auto &item:sources.items) { Keys(item,{"source_path","file"}); PublicationSource s; s.sourcePath=Text(item.At("source_path")); s.file=File(item.At("file")); p.sources.push_back(s); }
    Check(p); return p;
}
SystemExperimentArtifact VerifyConfigurationPublication(const ConfigurationPublication &p,const std::function<std::string(const PublicationFile &)> &read) {
    Check(p);
    const auto content=[&](const PublicationFile &f) {
        const auto bytes=read(f);
        if (bytes.size()!=f.bytes || ConfigurationSha256(bytes)!=f.sha256) throw std::invalid_argument("Publication file bytes/SHA differ: "+f.stored);
        return bytes;
    };
    const auto artifact=DecodeSystemExperimentArtifact(content(p.artifact));
    if (artifact.configurationSha256!=p.configurationSha256 || ConfigurationSha256(content(p.profile))!=artifact.profileSha256) throw std::invalid_argument("Publication parameters/profile binding differs");
    const auto effective=DecodeEffectiveConfigurationRecord(content(p.effective));
    Keys(effective,{"schema_version","kind","root_file","root_path","sources","objects","scalars","effective_sha256","snapshot_sha256"});
    if (effective.At("schema_version").type!=JsonValue::Number || effective.At("schema_version").text!="1" || Text(effective.At("kind"))!="getdaq.effective-configuration-snapshot" ||
        Text(effective.At("root_file"))!=p.rootSource || Text(effective.At("effective_sha256"))!=p.effectiveSha256 || Text(effective.At("snapshot_sha256"))!=p.snapshotSha256)
        throw std::invalid_argument("Publication effective snapshot binding differs");
    std::map<std::string,std::string> expected,compiled,snapshot;
    for (const auto &s:p.sources) { content(s.file); expected.emplace(s.sourcePath,s.file.sha256); }
    for (const auto &s:artifact.sources) if (!compiled.emplace(s.path,s.sha256).second) throw std::invalid_argument("Duplicate compiled publication source");
    const auto &sources=effective.At("sources"); if (sources.type!=JsonValue::Array || sources.items.size()!=p.sources.size()) throw std::invalid_argument("Publication snapshot source count differs");
    std::map<std::string,std::uint64_t> sizes; for (const auto &s:p.sources) sizes.emplace(s.sourcePath,s.file.bytes);
    for (const auto &s:sources.items) {
        Keys(s,{"path","sha256","bytes"}); const auto path=Text(s.At("path"));
        if (!sizes.count(path) || Size(s.At("bytes"))!=sizes.at(path) || !snapshot.emplace(path,Text(s.At("sha256"))).second) throw std::invalid_argument("Publication snapshot source bytes differ");
    }
    if (compiled!=expected || snapshot!=expected) throw std::invalid_argument("Publication artifact/snapshot/source table differs");
    return artifact;
}
}
