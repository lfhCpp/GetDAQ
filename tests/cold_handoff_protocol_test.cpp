#include "coldhandoffprotocol.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <functional>
#include <iostream>
#include <stdexcept>
using namespace GetDAQ;
namespace {
void Require(bool okay,const char *why){if(!okay)throw std::runtime_error(why);}
void Reject(const std::function<void()> &f){bool failed=false;try{f();}catch(const std::invalid_argument &){failed=true;}Require(failed,"Invalid recovery metadata accepted");}
}
int main(){try {
    ColdHandoffRequest r;r.taskId=r.attemptId=std::string(128,'a');r.instanceId=std::string(64,'1');r.policySha256=std::string(64,'2');r.action="submit";
    ColdHandoffOperation o;o.request=r;o.requestKey=ColdHandoffRequestKey(r);o.phase="complete";
    o.receipt="{\"schema\":1,\"kind\":\"getdaq.remote-plan-retirement\",\"test_metadata\":"+EncodeJsonString(std::string(350*1024,'\\'),350*1024)+"}\n";
    o.receiptSha256=ConfigurationSha256(o.receipt);const auto bytes=EncodeColdHandoffOperation(o);const auto decoded=DecodeColdHandoffOperation(bytes);
    Require(decoded.receipt==o.receipt && decoded.receiptSha256==o.receiptSha256,"Metadata transport changed a receipt larger than ordinary scalar limits");
    auto wrong=o;wrong.receiptSha256=std::string(64,'3');Reject([&]{EncodeColdHandoffOperation(wrong);});
    wrong=o;wrong.phase="unknown";Reject([&]{EncodeColdHandoffOperation(wrong);});
    auto request=r;request.action="prepare";Reject([&]{EncodeColdHandoffRequest(request);});
    request=r;request.attemptId="../foreign";Reject([&]{EncodeColdHandoffRequest(request);});
    request=r;request.attemptId="other";Reject([&]{EncodeColdHandoffRequest(request);});
    Reject([&]{DecodeColdHandoffRequest("{\"schema\":1,\"schema\":1}");});
    auto encoded=EncodeColdHandoffRequest(r);encoded.insert(1,"\"executable\":\"/tmp/other\",");Reject([&]{DecodeColdHandoffRequest(encoded);});
    o.receipt=std::string(1024*1024+1,'x');o.receiptSha256=ConfigurationSha256(o.receipt);Reject([&]{EncodeColdHandoffOperation(o);});
    std::cout<<"PASS cold protocol large exact-byte receipt, digest/phase binding, full operation ID and arbitrary command/path rejection\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
