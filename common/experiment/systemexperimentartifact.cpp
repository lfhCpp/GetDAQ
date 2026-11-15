#include "systemexperimentfields.h"
#include "strictjson.h"
#include "configurationsha256.h"
#include <cmath>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>
namespace GetDAQ {
namespace {
std::uint64_t Unsigned(const std::string &text,std::uint64_t maximum) {
    if (text.empty() || text.size()>20 || (text.size()>1 && text[0]=='0')) throw std::invalid_argument("Expected canonical decimal integer");
    std::uint64_t value=0;
    for (char c:text) {
        if (c<'0' || c>'9' || unsigned(c-'0')>maximum || value>(maximum-unsigned(c-'0'))/10)
            throw std::invalid_argument("Invalid/out-of-range decimal integer");
        value=value*10+unsigned(c-'0');
    }
    return value;
}
std::string Text(const JsonValue &v) {
    if (v.type!=JsonValue::String) throw std::invalid_argument("Expected configuration string");
    return v.text;
}
void Digest(const std::string &s) {
    if (s.size()!=64 || s.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("Expected lowercase SHA-256 digest");
}
class Decoder {
    const JsonValue &object;
    std::set<std::string> fields;
public:
    explicit Decoder(const JsonValue &v):object(v) {
        if (v.type!=JsonValue::Object) throw std::invalid_argument("Expected experiment settings object");
    }
    template<class T> void Field(const char *name,T &s) { fields.insert(name); Value(object.At(name),s); }
    void Finish() const {
        if (object.members.size()!=fields.size()) throw std::invalid_argument("Unknown experiment settings key");
    }
    static void Value(const JsonValue &v,std::string &s) { s=Text(v); }
    static void Value(const JsonValue &v,bool &s) {
        if (v.type!=JsonValue::Boolean) throw std::invalid_argument("Expected JSON boolean");
        s=v.text=="true";
    }
    template<class T> static typename std::enable_if<std::is_integral<T>::value && !std::is_same<T,bool>::value>::type Value(const JsonValue &v,T &s) {
        const auto text=Text(v);
        const bool negative=!text.empty() && text[0]=='-';
        if (negative && (!std::is_signed<T>::value || text=="-0")) throw std::invalid_argument("Invalid negative integer");
        const auto maximum=std::uint64_t(std::numeric_limits<T>::max());
        const auto magnitude=Unsigned(negative ? text.substr(1) : text,maximum+(negative ? 1 : 0));
        s=negative ? static_cast<T>(-static_cast<std::int64_t>(magnitude-1)-1) : static_cast<T>(magnitude);
    }
    template<class T> static typename std::enable_if<std::is_enum<T>::value>::type Value(const JsonValue &v,T &s) {
        typename std::underlying_type<T>::type value; Value(v,value); s=static_cast<T>(value);
    }
    template<class T> static typename std::enable_if<std::is_floating_point<T>::value>::type Value(const JsonValue &v,T &s) {
        const auto text=Text(v);
        if (text.size()>128 || ParseStrictJson(text).type!=JsonValue::Number) throw std::invalid_argument("Expected decimal real string");
        std::istringstream stream(text); stream.imbue(std::locale::classic()); stream >> std::noskipws >> s;
        if (!stream || !stream.eof() || !std::isfinite(s)) throw std::invalid_argument("Invalid/non-finite experiment real");
    }
    template<class T> static void Value(const JsonValue &v,std::vector<T> &s) {
        if (v.type!=JsonValue::Array) throw std::invalid_argument("Expected experiment array");
        s.clear(); s.reserve(v.items.size()); for (const auto &item:v.items) { T value; Value(item,value); s.push_back(std::move(value)); }
    }
    template<class T,std::size_t N> static void Value(const JsonValue &v,std::array<T,N> &s) {
        if (v.type!=JsonValue::Array || v.items.size()!=N) throw std::invalid_argument("Incorrect fixed experiment array size");
        for (std::size_t i=0; i<N; ++i) Value(v.items[i],s[i]);
    }
    static void Key(const std::string &s,std::string &key) { key=s; }
    static void Key(const std::string &s,unsigned int &key) { key=static_cast<unsigned int>(Unsigned(s,std::numeric_limits<unsigned int>::max())); }
    template<class K,class V> static void Value(const JsonValue &v,std::map<K,V> &s) {
        if (v.type!=JsonValue::Object) throw std::invalid_argument("Expected experiment parameter map");
        s.clear(); for (const auto &entry:v.members) { K key; Key(entry.first,key); V value; Value(entry.second,value); s.emplace(std::move(key),std::move(value)); }
    }
    template<class T> static typename std::enable_if<!std::is_arithmetic<T>::value && !std::is_enum<T>::value>::type Value(const JsonValue &v,T &s) {
        Decoder decoder(v); ExperimentFields(decoder,s); decoder.Finish();
    }
};
void Sources(const SystemExperimentArtifact &a) {
    if (a.sources.empty() || a.sources.size()>256) throw std::invalid_argument("Artifact requires 1..256 source records");
    std::set<std::string> paths;
    for (const auto &s:a.sources) {
        if (s.path.empty() || s.path.size()>4096 || s.path.find('\0')!=std::string::npos || !paths.insert(s.path).second)
            throw std::invalid_argument("Invalid/duplicate experiment source path");
        ValidateJsonUtf8(s.path); Digest(s.sha256);
    }
    Digest(a.profileSha256);
}
}
std::string SystemExperimentDigest(const SystemExperimentSettings &s) {
    return ConfigurationSha256("getdaq.system-experiment-settings:1\n"+EncodeSystemExperimentSettings(s));
}
std::string EncodeSystemExperimentArtifact(const SystemExperimentArtifact &a) {
    Sources(a);
    const auto version=SystemExperimentDigest(a.settings);
    if (!a.configurationSha256.empty() && a.configurationSha256!=version) throw std::invalid_argument("Supplied experiment digest differs from settings");
    std::string json="{\"schema_version\":1,\"kind\":\"getdaq.system-experiment-settings\",\"configuration_sha256\":"+EncodeJsonString(version)+
        ",\"profile_sha256\":"+EncodeJsonString(a.profileSha256)+",\"sources\":[";
    for (const auto &s:a.sources) { if (json.back()!='[') json+=','; json+="{\"path\":"+EncodeJsonString(s.path)+",\"sha256\":"+EncodeJsonString(s.sha256)+"}"; }
    json+="],\"settings\":"+EncodeSystemExperimentSettings(a.settings)+"}\n";
    if (json.size()>64*1024*1024) throw std::invalid_argument("Experiment artifact exceeds 64 MiB");
    return json;
}
SystemExperimentArtifact DecodeSystemExperimentArtifact(const std::string &bytes) {
    const auto root=ParseStrictJson(bytes);
    if (root.type!=JsonValue::Object || root.members.size()!=6 || root.At("schema_version").type!=JsonValue::Number || root.At("schema_version").text!="1" ||
        Text(root.At("kind"))!="getdaq.system-experiment-settings") throw std::invalid_argument("Unsupported system experiment artifact");
    SystemExperimentArtifact a;
    a.configurationSha256=Text(root.At("configuration_sha256")); a.profileSha256=Text(root.At("profile_sha256"));
    const auto &sources=root.At("sources");
    if (sources.type!=JsonValue::Array) throw std::invalid_argument("Expected source records array");
    for (const auto &source:sources.items) {
        if (source.type!=JsonValue::Object || source.members.size()!=2) throw std::invalid_argument("Unexpected source record keys");
        ExperimentConfigurationSource s; s.path=Text(source.At("path")); s.sha256=Text(source.At("sha256")); a.sources.push_back(std::move(s));
    }
    Sources(a); Digest(a.configurationSha256); Decoder::Value(root.At("settings"),a.settings);
    if (SystemExperimentDigest(a.settings)!=a.configurationSha256) throw std::invalid_argument("Experiment configuration digest mismatch");
    a.artifactSha256=ConfigurationSha256(bytes); return a;
}
SystemExperimentArtifact LoadSystemExperimentArtifact(const std::string &path) { return DecodeSystemExperimentArtifact(ReadConfigurationFile(path)); }
}
