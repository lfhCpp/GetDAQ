#include "IceIngress.h"
#include <Ice/Ice.h>
#include <cstdint>
#include <algorithm>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <csignal>

namespace {
volatile std::sig_atomic_t legacyStop=0;
void StopLegacy(int) {legacyStop=1;}
class LegacyOnly : public mdaq::daq::DataProcessor {
public:
    std::atomic<unsigned int> calls{0};
    void processData(const std::pair<const Ice::Byte *,const Ice::Byte *> &,const Ice::Current &) override {++calls;}
};
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
std::vector<Ice::Byte> Frame(unsigned int event,bool little=false) {
    std::vector<Ice::Byte> bytes(4096,Ice::Byte(event));
    const Ice::Byte big[]={6,0,0,64,7,0,1,4,0,2,0,4},small[]={134,64,0,0,7,1,0,4,2,0,4,0};
    for(std::size_t i=0;i<12;++i) bytes[i]=little ? small[i] : big[i];
    for(unsigned int i=0;i<4;++i) bytes[12+i]=Ice::Byte(992u>>(little ? 8*i : 24-8*i));
    bytes[26]=bytes[27]=0;
    for(unsigned int i=0;i<4;++i) bytes[22+i]=Ice::Byte(event>>(little ? 8*i : 24-8*i));
    for(std::size_t i=128;i<136;++i) bytes[i]=i<132 ? 0 : 255; // Payload must remain unchanged.
    return bytes;
}
std::vector<Ice::Byte> RawBytes() {
    std::vector<Ice::Byte> bytes(65537);
    for (std::size_t i=0;i<bytes.size();++i) bytes[i]=Ice::Byte((i*37+11)&255);
    return bytes;
}
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc==6,"Pass IPv4 PORT SESSION MODE ENCODING");const std::string session=argv[3],mode=argv[4];const int minor=std::stoi(argv[5]);Require(minor==0 || minor==1,"Encoding must be 0/1");
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.RetryIntervals","-1");
        init.properties->setProperty("Ice.MessageSizeMax","4096");init.properties->setProperty("Ice.Override.Timeout","2000");
        communicator=Ice::initialize(init);Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=Ice::Byte(minor);
        if(mode=="legacy-server") {
            std::signal(SIGTERM,StopLegacy);std::signal(SIGINT,StopLegacy);
            auto adapter=communicator->createObjectAdapterWithEndpoints("LegacyOnly",std::string("tcp -h ")+argv[1]+" -p "+argv[2]);
            IceUtil::Handle<LegacyOnly> servant=new LegacyOnly;adapter->add(servant,communicator->stringToIdentity("DataProcessor"));adapter->activate();
            std::cout<<"{\"legacy_ready\":true}\n"<<std::flush;
            while(!legacyStop) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            adapter->deactivate();adapter->waitForDeactivate();std::cout<<"{\"data_calls\":"<<servant->calls<<"}\n";
            communicator->destroy();communicator=nullptr;return 0;
        }
        auto base=communicator->stringToProxy(std::string("DataProcessor:tcp -h ")+argv[1]+" -p "+argv[2])->ice_encodingVersion(encoding)->ice_collocationOptimized(false)->ice_connectionId("data-source");
        auto original=mdaq::daq::DataProcessorPrx::checkedCast(base);Require(bool(original),"Original DataProcessor checkedCast failed");
        auto data=mdaq::daq::DataProcessorPrx::uncheckedCast(original->ice_oneway());GetDAQ::DataIngressPrx end;
        if(mode!="legacy" && mode!="peer-close" && mode!="message-limit" && mode!="reconnect-count") {end=GetDAQ::DataIngressPrx::checkedCast(base);Require(bool(end),"New explicit-end checkedCast failed");}
        std::vector<Ice::Byte> payload=mode=="byte-storage" ? RawBytes() : Frame(1);const auto second=Frame(2,true);
        if(mode!="byte-storage") payload.insert(payload.end(),second.begin(),second.end());
        if(mode=="empty") payload.clear();
        else if(mode=="truncated") payload.insert(payload.end(),second.begin(),second.begin()+100);
        else if(mode=="oversized-frame") {payload=Frame(1);payload[1]=127;payload[2]=255;payload[3]=255;}
        else if(mode=="message-limit" || mode=="reconnect-count") payload.resize(2*1024*1024,0);
        else Require(mode=="byte-storage" || mode=="complete" || mode=="export" || mode=="legacy" || mode=="peer-close" || mode=="wrong-session" || mode=="wrong-connection" || mode=="wrong-count" || mode=="second-source","Unknown ICE ingress mode");
        const std::size_t chunk=(mode=="message-limit" || mode=="reconnect-count") ? payload.size() : 317;
        for(std::size_t offset=0;offset<payload.size();offset+=chunk) {
            const auto count=std::min(chunk,payload.size()-offset);try {data->processData(std::make_pair(payload.data()+offset,payload.data()+offset+count));} catch(const Ice::Exception &) {if(mode!="message-limit" && mode!="reconnect-count") throw;}
        }
        if(mode=="wrong-connection" || mode=="second-source" || mode=="peer-close" || mode=="export") {
            const Ice::Byte empty=0;original->processData(std::make_pair(&empty,&empty)); // same-connection twoway dispatch barrier
        }
        if(mode=="export") std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Test observation only; not a production completion rule.
        if(mode=="peer-close") base->ice_getConnection()->close(true);
        if(mode=="reconnect-count") {
            auto marker=GetDAQ::DataIngressPrx::uncheckedCast(base->ice_connectionId("reconnected-source"));
            bool refused=false;try {marker->finishStream(session,Ice::Long(payload.size()));} catch(const Ice::Exception &) {refused=true;}
            Require(refused,"Lost initial one-way payload became successful empty stream");communicator->destroy();communicator=nullptr;
            std::cout<<"PASS lost-message/reconnect count rejected\n";return 0;
        }
        if(mode=="legacy" || mode=="peer-close" || mode=="message-limit") {
            communicator->destroy();communicator=nullptr;std::cout<<"PASS original one-way data sent without stream end\n";return 0;
        }
        if(mode=="second-source") {
            auto other=mdaq::daq::DataProcessorPrx::uncheckedCast(base->ice_connectionId("other-source")->ice_twoway());
            bool refused=false;try {other->processData(std::make_pair(second.data(),second.data()+second.size()));} catch(const Ice::Exception &) {refused=true;}
            Require(refused,"Second data connection unexpectedly accepted");communicator->destroy();communicator=nullptr;std::cout<<"PASS second-source payload rejected\n";return 0;
        }
        if(mode=="wrong-session" || mode=="wrong-connection" || mode=="wrong-count") {
            auto marker=mode=="wrong-connection" ? GetDAQ::DataIngressPrx::uncheckedCast(base->ice_connectionId("other-control")) : end;
            bool refused=false;try {marker->finishStream(mode=="wrong-session" ? std::string(64,'b') : session,Ice::Long(payload.size()+(mode=="wrong-count" ? 1 : 0)));} catch(const Ice::Exception &) {refused=true;}
            Require(refused,"Invalid source end request accepted");
        }
        if(mode=="truncated" || mode=="oversized-frame") {
            try {end->finishStream(session,Ice::Long(payload.size()));} catch(const Ice::Exception &) {}
        } else end->finishStream(session,Ice::Long(payload.size()));
        communicator->destroy();communicator=nullptr;std::cout<<"PASS actual Ice encoding 1."<<minor<<" source mode "<<mode<<'\n';return 0;
    } catch(const std::exception &e) {if(communicator) try {communicator->destroy();} catch(...) {}std::cerr<<e.what()<<'\n';return 1;}
}
