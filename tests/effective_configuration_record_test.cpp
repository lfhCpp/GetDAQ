#include "effectiveconfigurationrecord.h"
#include "configurationrange.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "effective_record_fixture.h"
#include <iostream>
#include <stdexcept>
namespace {
using namespace GetDAQ;
void Require(bool value,const char *why) { if (!value) throw std::runtime_error(why); }
template<class F> void Rejects(F action) { try { action(); } catch (const std::exception &) { return; } throw std::runtime_error("Invalid effective record accepted"); }
JsonValue String(const std::string &s) { JsonValue v; v.type=JsonValue::String; v.text=s; return v; }
JsonValue Object() { JsonValue v; v.type=JsonValue::Object; return v; }
JsonValue Origin(const std::string &path) {
    auto v=Object(); v.members["file"]=String("root.xcfg"); v.members["path"]=String(path);
    JsonValue line; line.type=JsonValue::Number; line.text="1"; v.members["line"]=line; return v;
}
JsonValue Scalar(const std::string &path,const std::string &type,const std::string &value,const std::string &range="") {
    auto v=Object(); v.members["path"]=String(path); v.members["declared_type"]=String(type); v.members["type"]=String(type); v.members["value"]=String(value);
    v.members["normalized_value"]=String(NormalizeConfigurationValue(ParseConfigurationValue(value,type)));
    v.members["unit"]=String(""); v.members["range"]=String(range); v.members["value_source"]=Origin(path);
    v.members["unit_source"]=Object(); v.members["range_source"]=range.empty() ? Object() : Origin(path); return v;
}
void Seal(JsonValue &v) {
    const auto digests=CheckEffectiveConfigurationRecord(v,false);
    v.members["effective_sha256"]=String(digests.effectiveSha256); v.members["snapshot_sha256"]=String(digests.snapshotSha256);
}
std::string Encode(const JsonValue &v) {
    if (v.type==JsonValue::String) return EncodeJsonString(v.text);
    if (v.type==JsonValue::Number || v.type==JsonValue::Boolean) return v.text;
    if (v.type==JsonValue::Null) return "null";
    std::string out=v.type==JsonValue::Array ? "[" : "{";
    if (v.type==JsonValue::Array) for (const auto &item:v.items) { if (out.back()!='[') out+=','; out+=Encode(item); }
    else for (const auto &item:v.members) { if (out.back()!='{') out+=','; out+=EncodeJsonString(item.first)+':'+Encode(item.second); }
    return out+(v.type==JsonValue::Array ? ']' : '}');
}
JsonValue Base() {
    return ParseStrictJson(GetDAQTest::EmptyEffectiveSnapshot("root.xcfg",ConfigurationSha256("<Setup/>"),8));
}
}
int main() {
    try {
        using namespace GetDAQ;
        auto base=Base(); const auto empty=CheckEffectiveConfigurationRecord(base);
        Require(empty.effectiveSha256=="267026a64c0163a684cc15e33d47d9f094746fd5b1d5c6417f50cbd8bbd03b51","Independent empty effective digest differs");
        Require(empty.snapshotSha256=="dcd72ec790e335c67ef2703757784e9332e6ebc9273e8469e14eca07b2ca319f","Independent empty source digest differs");
        auto v=base;
        v.members["objects"].items.push_back(String("Setup/Board[0]"));
        v.members["scalars"].items={Scalar("Setup/Board[0]/gain","int","9007199254740993","[9007199254740992,9007199254740994]"),
            Scalar("Setup/Board[0]/mask","hex","FFFFFFFFFFFFFFFF","[0x0,0xffffffffffffffff]"),Scalar("Setup/Board[0]/zero","real","-0.0")};
        Seal(v); const auto valid=CheckEffectiveConfigurationRecord(v); Require(DecodeEffectiveConfigurationRecord(Encode(v)).At("effective_sha256").text==valid.effectiveSha256,"Strict record roundtrip");
        auto changed=v; changed.members["scalars"].items[0].members["value"]=String("09007199254740993"); changed.members["scalars"].items[0].members["range"]=String("[9007199254740992, 9007199254740994]"); Seal(changed);
        Require(CheckEffectiveConfigurationRecord(changed).effectiveSha256==valid.effectiveSha256 && CheckEffectiveConfigurationRecord(changed).snapshotSha256!=valid.snapshotSha256,"Equivalent syntax changed effective version or lost provenance");
        changed=v; changed.members["scalars"].items[0].members["normalized_value"]=String("9007199254740992"); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items[1].members["range"]=String("[0,255]"); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items[0].members["range_source"].members["file"]=String("unknown.xcfg"); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items[0].members["value_source"].members["line"].text="0"; Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["objects"].items.pop_back(); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items.push_back(changed.members["scalars"].items.back()); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items[0].members["extra"]=String("x"); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["scalars"].items[0].members["value"].text=std::string(1,char(0xff)); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=v; changed.members["effective_sha256"].text=std::string(64,'0'); Rejects([&] { CheckEffectiveConfigurationRecord(changed); });
        const std::string duplicate="{\"schema_version\":1,"+Encode(v).substr(1); Rejects([&] { DecodeEffectiveConfigurationRecord(duplicate); });
        auto unicode=base;
        const std::string supplementary="Setup/Item[\xf0\x90\x80\x80]",privateUse="Setup/Item[\xee\x80\x80]";
        unicode.members["objects"].items.push_back(String(supplementary)); unicode.members["objects"].items.push_back(String(privateUse)); Seal(unicode); CheckEffectiveConfigurationRecord(unicode);
        auto wrongOrder=unicode; std::swap(wrongOrder.members["objects"].items[1],wrongOrder.members["objects"].items[2]); Rejects([&] { CheckEffectiveConfigurationRecord(wrongOrder,false); });
        changed=v; changed.members["objects"].items.push_back(String("Setup/Board[*]")); Rejects([&] { CheckEffectiveConfigurationRecord(changed,false); });
        changed=base; changed.members["scalars"].items.push_back(Scalar("Setup/choice","string","a,b","{\"a,b\",\"a\\\"b\"}")); Seal(changed); CheckEffectiveConfigurationRecord(changed);
        changed=base; changed.members["scalars"].items.push_back(Scalar("Setup/enabled","bool","TRUE","{false,TRUE}")); Seal(changed); CheckEffectiveConfigurationRecord(changed);
        changed=base; changed.members["scalars"].items.push_back(Scalar("Setup/label","string","", "{\"\"}")); Seal(changed); CheckEffectiveConfigurationRecord(changed);
        std::cout<<"PASS pure effective record: independent digest vectors, exact types/ranges, provenance, Unicode UTF-16 ordering, strict shape and tampering\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
