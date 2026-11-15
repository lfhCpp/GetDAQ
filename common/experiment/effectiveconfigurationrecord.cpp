#include "effectiveconfigurationrecord.h"
#include "configurationsha256.h"
#include "../config/configurationrange.h"
#include <cmath>
#include <limits>
#include <locale>
#include <set>
#include <sstream>
#include <stdexcept>
#include <cstdint>
namespace GetDAQ {
namespace {
const std::size_t MaxBytes=64*1024*1024;
void Require(bool condition,const std::string &why) { if (!condition) throw std::invalid_argument(why); }
void Keys(const JsonValue &v,std::initializer_list<const char *> keys) {
    Require(v.type==JsonValue::Object && v.members.size()==keys.size(),"Unexpected or missing snapshot fields");
    for (const char *key:keys) v.At(key);
}
const std::string &Text(const JsonValue &v,std::size_t maximum=1024*1024) {
    Require(v.type==JsonValue::String && v.text.size()<=maximum && v.text.find('\0')==std::string::npos,"Invalid snapshot string");
    ValidateJsonUtf8(v.text); return v.text;
}
const std::vector<JsonValue> &Array(const JsonValue &v,std::size_t maximum) {
    Require(v.type==JsonValue::Array && v.items.size()<=maximum,"Snapshot array type/limit differs"); return v.items;
}
void Digest(const std::string &s) { Require(s.size()==64 && s.find_first_not_of("0123456789abcdef")==std::string::npos,"Invalid snapshot SHA-256"); }
std::uint64_t Decimal(const JsonValue &v,std::uint64_t maximum) {
    const auto &s=Text(v,20); Require(!s.empty() && (s.size()==1 || s[0]!='0'),"Invalid snapshot canonical decimal"); std::uint64_t result=0;
    for (char c:s) { Require(c>='0' && c<='9' && std::uint64_t(c-'0')<=maximum && result<=(maximum-std::uint64_t(c-'0'))/10,"Snapshot decimal overflow"); result=result*10+std::uint64_t(c-'0'); }
    return result;
}
double Number(const JsonValue &v) {
    Require(v.type==JsonValue::Number,"Snapshot field must be a number");
    std::istringstream input(v.text); input.imbue(std::locale::classic()); double result;
    input>>std::noskipws>>result; Require(bool(input) && input.eof() && std::isfinite(result),"Invalid snapshot number"); return result;
}
int Line(const JsonValue &v) {
    const auto n=Number(v); Require(n>=1 && n<=std::numeric_limits<int>::max() && std::floor(n)==n,"Invalid snapshot source line"); return static_cast<int>(n);
}
std::u16string Utf16(const std::string &s) {
    // schema1沿用QString排序；UTF-8字节序在补充平面/私用区之间与UTF-16不同。
    std::u16string result;
    for (std::size_t i=0;i<s.size();) {
        std::uint32_t c=static_cast<unsigned char>(s[i++]); unsigned count=0;
        if (c>=0xf0) { count=3; c&=7; } else if (c>=0xe0) { count=2; c&=15; } else if (c>=0xc0) { count=1; c&=31; }
        while (count--) c=(c<<6)|(static_cast<unsigned char>(s[i++])&63);
        if (c<=0xffff) result+=static_cast<char16_t>(c);
        else { c-=0x10000; result+=static_cast<char16_t>(0xd800+(c>>10)); result+=static_cast<char16_t>(0xdc00+(c&1023)); }
    }
    return result;
}
void Ordered(const std::string &previous,const std::string &current) {
    Require(previous.empty() || Utf16(previous)<Utf16(current),"Unsorted/duplicate snapshot path");
}
void Field(std::string *encoded,const std::string &s) {
    Require(s.size()<=1024*1024 && encoded->size()+s.size()+32<=MaxBytes,"Snapshot digest byte limit exceeded");
    *encoded+=std::to_string(s.size())+':'+s;
}
void Origin(const JsonValue &v,const std::set<std::string> &sources,bool required,std::string *record) {
    Require(v.type==JsonValue::Object,"Snapshot origin must be an object");
    if (v.members.empty() && !required) { Field(record,""); Field(record,""); Field(record,"0"); return; }
    Keys(v,{"file","path","line"}); const auto &file=Text(v.At("file"),4096),&path=Text(v.At("path"),65536);
    Require(sources.count(file) && !path.empty(),"Unknown/empty snapshot origin");
    Field(record,file); Field(record,path); Field(record,std::to_string(Line(v.At("line"))));
}
void ConcretePath(const std::string &path,const std::string &root) {
    Require(!path.empty() && path.back()!='/' && (path==root || (path.size()>root.size() && path.compare(0,root.size()+1,root+'/')==0)),"Invalid concrete snapshot path");
    std::size_t begin=0,segments=0;
    while (begin<path.size()) {
        auto end=path.find('/',begin); if (end==std::string::npos) end=path.size();
        const auto part=path.substr(begin,end-begin); const auto bracket=part.find('[');
        Require(!part.empty() && ++segments<=128,"Snapshot path depth/segment limit exceeded");
        if (bracket==std::string::npos) Require(part.find(']')==std::string::npos,"Ambiguous snapshot path segment");
        else {
            Require(bracket>0 && part.back()==']' && part.size()>bracket+2 && part.find(']',0)==part.size()-1 && part.find('[',bracket+1)==std::string::npos,
                    "Ambiguous snapshot path segment");
            Require(part.substr(bracket+1,part.size()-bracket-2).find('*')==std::string::npos,"Snapshot path requires a concrete ID");
        }
        begin=end+1;
    }
}
}
EffectiveConfigurationDigests CheckEffectiveConfigurationRecord(const JsonValue &v,bool checkDigests) {
    Keys(v,{"schema_version","kind","root_file","root_path","sources","objects","scalars","effective_sha256","snapshot_sha256"});
    Require(Number(v.At("schema_version"))==1 && Text(v.At("kind"))=="getdaq.effective-configuration-snapshot","Unsupported effective snapshot schema");
    const auto &rootFile=Text(v.At("root_file"),4096),&rootPath=Text(v.At("root_path"),65536); Require(!rootPath.empty(),"Empty snapshot root path");
    std::string effective="getdaq.effective-configuration:1\n",record="getdaq.effective-configuration-snapshot:1\n";
    Field(&effective,rootPath); Field(&record,rootFile); Field(&record,rootPath);
    const auto &objects=Array(v.At("objects"),200000),&scalars=Array(v.At("scalars"),200000),&sources=Array(v.At("sources"),256);
    Require(objects.size()+scalars.size()<=200000,"Effective snapshot node limit exceeded");
    Field(&effective,std::to_string(objects.size())); Field(&record,std::to_string(objects.size()));
    std::set<std::string> concrete,objectPaths,sourcePaths; std::string previous; std::size_t pathBytes=0;
    const auto path=[&](const std::string &value) {
        ConcretePath(value,rootPath); Require(concrete.insert(value).second,"Duplicate snapshot path");
        pathBytes+=value.size(); Require(pathBytes<=MaxBytes,"Effective snapshot path byte limit exceeded");
    };
    for (const auto &item:objects) {
        const auto &name=Text(item,65536); Ordered(previous,name); previous=name; path(name); objectPaths.insert(name);
        Field(&effective,name); Field(&record,name);
    }
    Field(&record,std::to_string(sources.size())); previous.clear(); std::uint64_t sourceBytes=0;
    for (const auto &source:sources) {
        Keys(source,{"path","sha256","bytes"}); const auto &name=Text(source.At("path"),4096),&sha=Text(source.At("sha256"));
        Require(!name.empty(),"Empty snapshot source"); Ordered(previous,name); previous=name; sourcePaths.insert(name); Digest(sha);
        sourceBytes+=Decimal(source.At("bytes"),8*1024*1024); Require(sourceBytes<=MaxBytes,"Snapshot source total byte limit exceeded");
        Field(&record,name); Field(&record,sha); Field(&record,Text(source.At("bytes"),20));
    }
    Require(sourcePaths.count(rootFile),"Snapshot root is absent from sources");
    Field(&effective,std::to_string(scalars.size())); Field(&record,std::to_string(scalars.size())); previous.clear();
    for (const auto &scalar:scalars) {
        Keys(scalar,{"path","value","declared_type","type","normalized_value","unit","range","value_source","unit_source","range_source"});
        const auto &name=Text(scalar.At("path"),65536); Ordered(previous,name); previous=name; path(name);
        const auto &text=Text(scalar.At("value")),&declared=Text(scalar.At("declared_type"),16),&type=Text(scalar.At("type"),16),&normalized=Text(scalar.At("normalized_value"));
        const auto parsed=ParseConfigurationValue(text,declared);
        Require(type==ConfigurationValueType(parsed) && normalized==NormalizeConfigurationValue(parsed),"Snapshot type/normalized value differs");
        const auto &unit=Text(scalar.At("unit")),&range=Text(scalar.At("range"));
        if (!range.empty()) Require(ParseConfigurationRange(range).Contains(parsed),"Snapshot value outside range: "+name);
        Field(&effective,name); Field(&effective,type); Field(&effective,normalized); Field(&effective,unit); Field(&effective,EncodeConfigurationRange(range));
        for (const char *key:{"path","value","declared_type","type","normalized_value","unit","range"}) Field(&record,Text(scalar.At(key)));
        Origin(scalar.At("value_source"),sourcePaths,true,&record); Origin(scalar.At("unit_source"),sourcePaths,!unit.empty(),&record); Origin(scalar.At("range_source"),sourcePaths,!range.empty(),&record);
    }
    Require(concrete.count(rootPath),"Snapshot root path is absent from effective tree");
    for (const auto &name:concrete) if (name!=rootPath) Require(objectPaths.count(name.substr(0,name.find_last_of('/'))),"Snapshot path has no object parent");
    EffectiveConfigurationDigests result; result.effectiveSha256=ConfigurationSha256(effective); result.snapshotSha256=ConfigurationSha256(record);
    if (checkDigests) {
        const auto &a=Text(v.At("effective_sha256")),&b=Text(v.At("snapshot_sha256")); Digest(a); Digest(b);
        Require(a==result.effectiveSha256 && b==result.snapshotSha256,"Snapshot digest mismatch");
    }
    return result;
}
JsonValue DecodeEffectiveConfigurationRecord(const std::string &bytes) {
    JsonLimits limits; limits.stringBytes=1024*1024; limits.arrayItems=200000;
    auto record=ParseStrictJson(bytes,limits); CheckEffectiveConfigurationRecord(record); return record;
}
}
