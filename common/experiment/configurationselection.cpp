#include "configurationselection.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Digest(const std::string &s) {
    if (s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos)
        throw std::invalid_argument("Configuration selection requires lowercase SHA-256");
}
void Check(const ConfigurationRegistration &r) {
    for (const auto &s:{r.repositoryId,r.publicationSha256,r.configurationSha256,r.artifactSha256,r.effectiveSha256,r.snapshotSha256}) Digest(s);
    ValidateJsonUtf8(r.manifestFile);
    const std::string suffix="/publication.json";
    if (r.manifestFile.size()<=suffix.size() || r.manifestFile.size()>3000 || r.manifestFile[0]!='/' ||
        r.manifestFile.compare(r.manifestFile.size()-suffix.size(),suffix.size(),suffix) ||
        r.manifestFile.find('\0')!=std::string::npos || r.manifestFile.find("//")!=std::string::npos ||
        r.manifestFile.find("/./")!=std::string::npos || r.manifestFile.find("/../")!=std::string::npos)
        throw std::invalid_argument("Selection requires normalized server publication path");
}
void Keys(const JsonValue &v,std::initializer_list<const char *> names) {
    if (v.type!=JsonValue::Object || v.members.size()!=names.size()) throw std::invalid_argument("Unexpected configuration selection fields");
    for (const char *name:names) v.At(name);
}
std::string Text(const JsonValue &v) {
    if (v.type!=JsonValue::String) throw std::invalid_argument("Configuration selection requires string");
    return v.text;
}
void Schema(const JsonValue &v,const char *kind) {
    if (v.At("schema_version").type!=JsonValue::Number || v.At("schema_version").text!="1" ||
        Text(v.At("kind"))!=kind || v.At("hardware_loaded_verified").type!=JsonValue::Boolean ||
        v.At("hardware_loaded_verified").text!="false") throw std::invalid_argument("Invalid configuration selection schema/scope");
}
std::string Body(const ConfigurationRegistration &r) {
    Check(r);
    return "\"repository_id\":"+EncodeJsonString(r.repositoryId)+",\"publication_sha256\":"+EncodeJsonString(r.publicationSha256)+
        ",\"configuration_sha256\":"+EncodeJsonString(r.configurationSha256)+",\"artifact_sha256\":"+EncodeJsonString(r.artifactSha256)+
        ",\"effective_sha256\":"+EncodeJsonString(r.effectiveSha256)+",\"snapshot_sha256\":"+EncodeJsonString(r.snapshotSha256)+
        ",\"manifest_file\":"+EncodeJsonString(r.manifestFile);
}
ConfigurationRegistration Record(const JsonValue &v) {
    ConfigurationRegistration r;r.repositoryId=Text(v.At("repository_id"));r.publicationSha256=Text(v.At("publication_sha256"));
    r.configurationSha256=Text(v.At("configuration_sha256"));r.artifactSha256=Text(v.At("artifact_sha256"));
    r.effectiveSha256=Text(v.At("effective_sha256"));r.snapshotSha256=Text(v.At("snapshot_sha256"));r.manifestFile=Text(v.At("manifest_file"));
    Check(r);return r;
}
JsonValue Read(const std::string &bytes) {
    if (bytes.empty() || bytes.size()>65536) throw std::invalid_argument("Configuration selection exceeds 64 KiB");
    return ParseStrictJson(bytes);
}
}
ConfigurationRegistration DecodeConfigurationRegistration(const std::string &bytes) {
    const auto v=Read(bytes);
    Keys(v,{"schema_version","kind","repository_id","publication_sha256","configuration_sha256","artifact_sha256",
        "effective_sha256","snapshot_sha256","manifest_file","hardware_loaded_verified"});
    Schema(v,"getdaq.configuration-registration");return Record(v);
}
std::string EncodeConfigurationRegistration(const ConfigurationRegistration &r) {
    return "{\"schema_version\":1,\"kind\":\"getdaq.configuration-registration\","+Body(r)+",\"hardware_loaded_verified\":false}\n";
}
ConfigurationSelection DecodeConfigurationSelection(const std::string &bytes) {
    return DecodeConfigurationSelection(Read(bytes));
}
ConfigurationSelection DecodeConfigurationSelection(const JsonValue &v) {
    Keys(v,{"schema_version","kind","repository_id","publication_sha256","configuration_sha256","artifact_sha256",
        "effective_sha256","snapshot_sha256","manifest_file","registration_sha256","hardware_loaded_verified"});
    Schema(v,"getdaq.configuration-selection");ConfigurationSelection s;s.registration=Record(v);
    s.registrationSha256=Text(v.At("registration_sha256"));Digest(s.registrationSha256);return s;
}
std::string EncodeConfigurationSelection(const ConfigurationSelection &s) {
    Digest(s.registrationSha256);
    return "{\"schema_version\":1,\"kind\":\"getdaq.configuration-selection\","+Body(s.registration)+
        ",\"registration_sha256\":"+EncodeJsonString(s.registrationSha256)+",\"hardware_loaded_verified\":false}\n";
}
ConfigurationSelection SelectConfigurationRegistration(const std::string &bytes) {
    ConfigurationSelection s;s.registration=DecodeConfigurationRegistration(bytes);s.registrationSha256=ConfigurationSha256(bytes);return s;
}
void VerifyConfigurationSelection(const ConfigurationSelection &s,const std::string &receiptBytes,const std::string &manifestFile,
    const std::string &manifestBytes,const ConfigurationPublication &p) {
    Digest(s.registrationSha256);
    const auto receipt=DecodeConfigurationRegistration(receiptBytes);
    if (ConfigurationSha256(receiptBytes)!=s.registrationSha256 || Body(receipt)!=Body(s.registration) ||
        manifestFile!=receipt.manifestFile || ConfigurationSha256(manifestBytes)!=receipt.publicationSha256 ||
        p.configurationSha256!=receipt.configurationSha256 || p.artifact.sha256!=receipt.artifactSha256 ||
        p.effectiveSha256!=receipt.effectiveSha256 || p.snapshotSha256!=receipt.snapshotSha256)
        throw std::invalid_argument("Configuration selection/registration/publication binding differs");
}
}
