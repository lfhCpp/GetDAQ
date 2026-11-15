#include "hardwaredescription.h"
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace GetDAQ;
namespace {
void Require(bool condition,const char *message) {if (!condition) throw std::runtime_error(message);}
std::string Sample() {
    return "{\"schema_version\":1,\"kind\":\"getdaq.hardware-description\",\"node_id\":\"test\",\"sources\":[],\"warnings\":[],\"devices\":[{\"name\":\"dev\",\"access\":\"memory\",\"base_address\":\"18446744073709551615\",\"register_bytes\":8,\"registers\":[{\"name\":\"reg\",\"offset\":\"0\",\"read_only\":true,\"fields\":[{\"name\":\"all\",\"offset_bits\":0,\"width_bits\":64,\"read_only\":false}]}]}]}";
}
std::string Replace(std::string text,const std::string &from,const std::string &to) {
    const auto at=text.find(from);Require(at!=std::string::npos,"Fixture substring missing");text.replace(at,from.size(),to);return text;
}
void Reject(const std::string &bytes) {bool failed=false;try {DecodeHardwareDescription(bytes);} catch (const std::exception &) {failed=true;}Require(failed,"Malformed description accepted");}
}
int main(int argc,char **argv) {
    try {
        if (argc==2) {std::cout<<HardwarePolicyJson(LoadHardwareDescription(argv[1]))<<'\n';return 0;}
        const auto bytes=Sample();const auto value=DecodeHardwareDescription(bytes);
        Require(value.devices[0].baseAddress==std::numeric_limits<std::uint64_t>::max(),"64-bit address lost precision");
        Require(value.devices[0].registers[0].fields[0].width==64,"64-bit field rejected");
        Require(HardwarePolicyJson(value).find("18446744073709551615")!=std::string::npos,"Policy address changed");
        Reject(Replace(bytes,"\"schema_version\":1","\"schema_version\":\"1\""));
        Reject(Replace(bytes,"\"read_only\":true","\"read_only\":\"true\""));
        Reject(Replace(bytes,"\"register_bytes\":8","\"register_bytes\":8.0"));
        Reject(Replace(bytes,"\"offset\":\"0\"","\"offset\":\"1\""));
        Reject(Replace(bytes,"\"width_bits\":64","\"width_bits\":65"));
        Reject(Replace(bytes,"\"offset_bits\":0","\"offset_bits\":1"));
        Reject(Replace(bytes,"\"node_id\":\"test\"","\"node_id\":\"test\",\"node_id\":\"other\""));
        Reject(Replace(bytes,"\"warnings\":[]","\"extra\":1,\"warnings\":[]"));
        Reject(Replace(bytes,"\"base_address\":\"18446744073709551615\"","\"base_address\":18446744073709551615"));
        Reject(Replace(bytes,"\"base_address\":\"18446744073709551615\"","\"base_address\":\"18446744073709551616\""));
        Reject(bytes+"\n---\n{}\n");Reject("schema_version: 1\nkind: getdaq.hardware-description\n");
        Reject(Replace(bytes,"\"node_id\":\"test\"","\"node_id\":\"\\u0000\""));
        Reject(std::string(8*1024*1024+1,' '));
        auto malformed=bytes;malformed.insert(malformed.find("test"),1,char(0xff));Reject(malformed);
        std::cout<<"PASS hardware description pure C++11: exact uint64/policy, strict types, bounds, duplicate keys, overflow, UTF-8 and JSON-only\n";
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
