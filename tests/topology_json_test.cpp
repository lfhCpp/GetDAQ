#include "systemalignment.h"
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace GetDAQ;
namespace {
void Require(bool value,const char *message) {if (!value) throw std::runtime_error(message);}
std::string Replace(std::string bytes,const std::string &from,const std::string &to) {
    const auto at=bytes.find(from);Require(at!=std::string::npos,"Missing fixture token");bytes.replace(at,from.size(),to);return bytes;
}
}
int main() {
    try {
        const std::string bytes="{\"schema_version\":1,\"kind\":\"getdaq.alignment-topology\",\"encoding\":\"1.1\",\"timeout_ms\":3000,\"mutants\":[],\"cobos\":[{\"id\":\"0\",\"expected_node_name\":\"board\",\"node_proxy\":\"node:tcp -h 127.0.0.1 -p 1\",\"link_proxy\":\"link:tcp -h 127.0.0.1 -p 1\",\"enabled\":true,\"hardware_description_file\":\"cobo.json\"}]}";
        unsigned reads=0;
        const auto reader=[&](const std::string &path) {
            Require(path=="cobo.json","Description path changed");++reads;
            HardwareDescription d;d.nodeId="board";DeviceInfo device;device.name="dev";device.access="memory";device.registerBytes=4;d.devices.push_back(device);return d;
        };
        const auto result=DecodeSystemAlignmentOptions(bytes,reader,true);
        Require(result.encodingMinor==1 && result.timeoutMs==3000 && result.cobos.size()==1 && result.cobos[0].enabled && reads==1,"Topology changed valid input");
        const std::vector<std::string> invalid={
            Replace(bytes,"\"schema_version\":1","\"schema_version\":\"1\""),
            Replace(bytes,"\"encoding\":\"1.1\"","\"encoding\":1.1"),
            Replace(bytes,"\"timeout_ms\":3000","\"timeout_ms\":3000.0"),
            Replace(bytes,"\"timeout_ms\":3000","\"timeout_ms\":\"3000\""),
            Replace(bytes,"\"enabled\":true","\"enabled\":\"true\""),
            Replace(bytes,"\"id\":\"0\"","\"id\":0"),
            Replace(bytes,"\"mutants\":[]","\"mutants\":[],\"mutants\":[]"),
            Replace(bytes,"\"mutants\":[]","\"mutants\":[],\"unknown\":false"),
            bytes+"\n---\n{}", "schema_version: 1\nkind: getdaq.alignment-topology\n",
            Replace(bytes,"\"enabled\":true","\"enabled\":true /*comment*/")};
        for (const auto &bad:invalid) {
            const auto before=reads;bool rejected=false;
            try {DecodeSystemAlignmentOptions(bad,reader,true);}catch (const std::exception &) {rejected=true;}
            Require(rejected && reads==before,"Invalid topology reached description reader");
        }
        std::cout<<"PASS topology strict JSON: exact bindings/types, duplicate/unknown keys, JSON-only and rejected before description reads\n";
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
