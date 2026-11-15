#include "controlledicesource.h"
#include "IceIngress.h"
#include <Ice/Ice.h>
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
class DelayedEnd : public GetDAQ::DataIngress {
public:
    std::atomic<bool> entered{false},release{false};
    std::atomic<unsigned int> finishes{0};
    std::string streamSession(const Ice::Current &) override {return std::string(64,'d');}
    void processData(const std::pair<const Ice::Byte *,const Ice::Byte *> &,const Ice::Current &) override {throw std::runtime_error("Fault fixture must remain empty");}
    void finishStream(const std::string &session,Ice::Long count,const Ice::Current &) override {
        Require(session==std::string(64,'d') && count==0,"Fault test end binding differs");entered=true;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!release) {Require(std::chrono::steady_clock::now()<deadline,"Fault test has no release");std::this_thread::sleep_for(std::chrono::milliseconds(1));}++finishes;
    }
};
}
int main() {
    Ice::CommunicatorPtr communicator;
    try {
        communicator=Ice::initialize();auto adapter=communicator->createObjectAdapterWithEndpoints("SourceFault","tcp -h 127.0.0.1 -p 0");IceUtil::Handle<DelayedEnd> receiver=new DelayedEnd;
        const auto proxy=adapter->add(receiver,communicator->stringToIdentity("DataProcessor"));adapter->activate();
        GetDAQ::IceSourceOptions options;options.port=unsigned(Ice::TCPEndpointInfoPtr::dynamicCast(proxy->ice_getEndpoints()[0]->getInfo())->port);options.expectedSession=std::string(64,'d');options.timeoutMs=3000;
        GetDAQ::IceProducerCallbacks callbacks;callbacks.verifyIdle=[] {};callbacks.start=[] {};callbacks.stop=[] {};callbacks.drain=[] {};
        GetDAQ::ControlledIceSource source(options,callbacks);source.Connect();
        auto ending=std::async(std::launch::async,[&] {try {source.Disconnect();return false;} catch(const std::exception &) {return true;}});
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!receiver->entered) {Require(std::chrono::steady_clock::now()<deadline,"Source did not issue actual End RPC");std::this_thread::sleep_for(std::chrono::milliseconds(1));}
        source.ReportFailure("Independent source device fault while final End waits");receiver->release=true;
        Require(ending.get() && receiver->finishes==1,"Late source state commit cleared fault or replayed End");
        const auto status=source.Status();Require(status.state==GetDAQ::ControlledIceSourceSnapshot::Unknown && status.stream.sourceEndAcknowledged,"Data End ACK must remain separate from source UNKNOWN");
        bool rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}Require(rejected && receiver->finishes==1,"Source UNKNOWN replayed finish");
        source.StopProductionAfterFailure();Require(source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown && receiver->finishes==1,"Explicit failure stop cleared fault or repeated End");
        adapter->deactivate();adapter->waitForDeactivate();communicator->destroy();communicator=nullptr;std::cout<<"PASS actual delayed End RPC + independent source fault: ACK cannot clear UNKNOWN, no End replay\n";return 0;
    } catch(const std::exception &e) {if(communicator)try {communicator->destroy();} catch(...) {}std::cerr<<e.what()<<'\n';return 1;}
}
