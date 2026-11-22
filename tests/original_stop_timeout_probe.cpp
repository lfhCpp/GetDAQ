// Fault injection through the actual patched original CoBoNode::preStop().
// No physical board, configuration, register or DAQ command is involved.
#include "get/rc/CoBoNode.h"
#include "get/cobo/CtrlNode.h"
#include <Ice/Ice.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
struct BlockedPulser : get::cobo::AsAdPulserMgr {
    std::atomic<int> calls{0};std::mutex mutex;std::condition_variable done;bool release=false;
    void setDefaultPulserVoltage(Ice::Long,const Ice::Current &) override {}
    void triggerPulser(Ice::Long,const Ice::Current &) override {}
    void configureExternalPulser(bool,Ice::Long,const Ice::Current &) override {}
    void configurePeriodicPulser(Ice::Long,Ice::Long,const Ice::Current &) override {}
    void setRandomPulserEnabled(bool,const Ice::Current &) override {}
    void configureDoublePulserMode(Ice::Long,const Ice::Current &) override {}
    void startPeriodicPulser(const Ice::Current &) override {}
    void stopPeriodicPulser(const Ice::Current &) override {++calls;std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[this]{return release;});}
    void Release(){std::lock_guard<std::mutex> lock(mutex);release=true;done.notify_all();}
};
struct Probe : get::rc::CoBoNode {
    explicit Probe(mdaq::rc::HardwareNode *node):CoBoNode("0",node){}
    using CoBoNode::preStop;
    ~Probe(){hwNode_=nullptr;} // Never run board-destructor register writes.
};
int main(){
    auto properties=Ice::createProperties();properties->setProperty("Ice.RetryIntervals","-1");Ice::InitializationData init;init.properties=properties;auto communicator=Ice::initialize(init);
    IceUtil::Handle<BlockedPulser> servant=new BlockedPulser;auto adapter=communicator->createObjectAdapterWithEndpoints("BlockedStop","tcp -h 127.0.0.1 -p 0");
    auto proxy=adapter->addWithUUID(servant)->ice_collocationOptimized(false);adapter->activate();mdaq::rc::HardwareNode hardware;hardware.hwServerCtrlPrx=mdaq::hw::NodePrx::uncheckedCast(proxy);
    bool timedOut=false;const auto start=std::chrono::steady_clock::now();
    {Probe node(&hardware);try{node.preStop();}catch(const Ice::TimeoutException &){timedOut=true;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';}}
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();servant->Release();adapter->deactivate();adapter->waitForDeactivate();communicator->destroy();
    if(!timedOut || elapsed<29000 || elapsed>35000 || servant->calls!=1){std::cerr<<"FAIL timeout="<<timedOut<<" ms="<<elapsed<<" calls="<<servant->calls<<'\n';return 1;}
    std::cout<<"PASS original pulser stop propagated TimeoutException after "<<elapsed<<" ms; one RPC, no retry, no fabricated success\n";
}
