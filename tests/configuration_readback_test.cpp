#include "configurationreadback.h"
#include "configurationsha256.h"
#include <functional>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace GetDAQ;
namespace {
void Require(bool ok,const char *message) {if (!ok) throw std::runtime_error(message);}
std::string Chain(const std::string &bytes) {
    auto result=ConfigurationSha256("getdaq.hardware-audit-chain:1\n");std::size_t begin=0;
    while (begin<bytes.size()) {const auto end=bytes.find('\n',begin);Require(end!=std::string::npos,"Test framing");result=ConfigurationSha256(result+'\n'+bytes.substr(begin,end-begin+1));begin=end+1;}
    return result;
}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();}catch(const std::exception&){rejected=true;}Require(rejected,"Invalid evidence accepted");}
std::string Replace(std::string body,const std::string &from,const std::string &to) {
    const auto at=body.find(from);Require(at!=std::string::npos,"Test replacement missing");body.replace(at,from.size(),to);return body;
}
}
int main() {
    try {
        const std::string target="\"device_proxy\":\"D:tcp -h 127.0.0.1 -p 1\",\"device\":\"ctrl\",\"register\":\"setting\",\"field\":\"\",\"value\":\"18446744073709551615\",\"mask\":\"18446744073709551615\",\"allowed_difference\":\"0\"";
        const std::string bytes="{\"schema_version\":1,\"phase\":\"intent\",\"node\":\"N:tcp -h 127.0.0.1 -p 1\",\"started_unix_ms\":\"9007199254740993\",\"steps\":[{\"operation\":\"write\","+target+"},{\"operation\":\"expect\","+target+"},{\"operation\":\"write\","+target+"},{\"operation\":\"delay\",\"milliseconds\":20},{\"operation\":\"pulser-stop-periodic\"}]}\n"
            "{\"step\":0,\"phase\":\"begin\"}\n{\"step\":0,\"phase\":\"acknowledged\",\"readback_verified\":false}\n"
            "{\"step\":1,\"phase\":\"begin\"}\n{\"step\":1,\"phase\":\"matched\",\"actual\":\"18446744073709551615\",\"difference\":\"0\"}\n"
            "{\"step\":2,\"phase\":\"begin\"}\n{\"step\":2,\"phase\":\"acknowledged\",\"readback_verified\":false}\n"
            "{\"step\":3,\"phase\":\"begin\"}\n{\"step\":3,\"phase\":\"completed\"}\n"
            "{\"step\":4,\"phase\":\"begin\"}\n{\"step\":4,\"phase\":\"acknowledged\",\"readback_verified\":false}\n"
            "{\"phase\":\"complete\",\"automatic_readback\":false}\n";
        const auto evidence=DecodeConfigurationReadbackAudit(bytes,Chain(bytes));
        Require(evidence.steps==5 && evidence.writesAcknowledged==2 && evidence.writesReadbackVerified==1 && evidence.checksMatched==1 &&
            evidence.managerCommandsAcknowledged==1 && evidence.delaysCompleted==1 && evidence.startedAtUnixMs==9007199254740993ull,"Counters/exact timestamp differ");
        Require(EncodeConfigurationReadbackEvidence(evidence).find("writes_without_full_readback\":1")!=std::string::npos,"Unverified write hidden");
        const std::string masked="{\"schema_version\":1,\"phase\":\"intent\",\"node\":\"N\",\"started_unix_ms\":\"1\",\"steps\":[{\"operation\":\"masked-update\",\"value\":\"1\",\"mask\":\"3\"},{\"operation\":\"read\"}]}\n"
            "{\"step\":0,\"phase\":\"begin\"}\n{\"step\":0,\"phase\":\"observed\",\"previous\":\"14\",\"effective_write_value\":\"13\"}\n"
            "{\"step\":0,\"phase\":\"write_begin\"}\n{\"step\":0,\"phase\":\"acknowledged\",\"readback_verified\":false}\n"
            "{\"step\":0,\"phase\":\"matched\",\"actual\":\"13\"}\n{\"step\":1,\"phase\":\"begin\"}\n{\"step\":1,\"phase\":\"observed\",\"actual\":\"13\"}\n"
            "{\"phase\":\"complete\",\"automatic_readback\":false}\n";
        const auto maskedEvidence=DecodeConfigurationReadbackAudit(masked,Chain(masked));
        Require(maskedEvidence.maskedUpdatesVerified==1 && maskedEvidence.writesReadbackVerified==1 && maskedEvidence.readOnlyObservations==1,"Masked update/read coverage missing");
        const auto badMasked=Replace(masked,"\"effective_write_value\":\"13\"","\"effective_write_value\":\"12\"");
        Reject([&] {DecodeConfigurationReadbackAudit(badMasked,Chain(badMasked));});
        Reject([&] {DecodeConfigurationReadbackAudit(Replace(bytes,"\"actual\":\"18446744073709551615\"","\"actual\":\"0\""),Chain(bytes));});
        for (const auto &changed:std::vector<std::string>{
            Replace(bytes,"\"actual\":\"18446744073709551615\"","\"actual\":\"0\""),
            Replace(bytes,"\"step\":1","\"step\":2"),
            Replace(bytes,"\"automatic_readback\":false","\"automatic_readback\":true"),
            Replace(bytes,"\"operation\":\"write\"","\"operation\":\"unknown\""),
            Replace(bytes,"\"actual\":\"18446744073709551615\"","\"actual\":\"18446744073709551616\"")})
            Reject([&] {DecodeConfigurationReadbackAudit(changed,Chain(changed));});
        Reject([&] {DecodeConfigurationReadbackAudit(bytes,std::string(64,'a'));});
        Reject([&] {DecodeConfigurationReadbackAudit(bytes.substr(0,bytes.size()-1),Chain(bytes));});
        std::cout<<"PASS strict completed Configure evidence: retained digest, full/partial coverage, ordered steps, exact64 and malformed/mismatch rejection; no hardware or DAQ\n";
        return 0;
    }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
}
