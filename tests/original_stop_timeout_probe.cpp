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
    std::atomic<int> calls{0},startCalls{0};bool failStart=false;std::atomic<int> *firmware=nullptr,*interrupts=nullptr,*daq=nullptr;std::mutex mutex;std::condition_variable done;bool release=false;
    void setDefaultPulserVoltage(Ice::Long,const Ice::Current &) override {}
    void triggerPulser(Ice::Long,const Ice::Current &) override {}
    void configureExternalPulser(bool,Ice::Long,const Ice::Current &) override {}
    void configurePeriodicPulser(Ice::Long,Ice::Long,const Ice::Current &) override {}
    void setRandomPulserEnabled(bool,const Ice::Current &) override {}
    void configureDoublePulserMode(Ice::Long,const Ice::Current &) override {}
    void startPeriodicPulser(const Ice::Current &) override {++startCalls;if(failStart)throw mdaq::utl::CmdException("injected periodic pulser start failure");}
    void stopPeriodicPulser(const Ice::Current &) override {++calls;if(firmware && *firmware==0 && *interrupts==0 && *daq>0)return;std::unique_lock<std::mutex> lock(mutex);done.wait(lock,[this]{return release;});}
    void Release(){std::lock_guard<std::mutex> lock(mutex);release=true;done.notify_all();}
};
struct Probe : get::rc::CoBoNode {
    explicit Probe(mdaq::rc::HardwareNode *node):CoBoNode("0",node){}
    using CoBoNode::preStop;
    using CoBoNode::impl_stop;
    using CoBoNode::postStart;
    void PeriodicMode(bool enabled){configData_.asadPulserPeriodicModeEnabled=enabled;}
    ~Probe(){hwNode_=nullptr;} // Never run board-destructor register writes.
};
struct Control : Ice::Blobject {
    std::atomic<int> firmware{1},interrupts{1},daq{0};bool failDaq=false,badReadback=false;
    bool ice_invoke(const Ice::ByteSeq &bytes,Ice::ByteSeq &response,const Ice::Current &current) override {
        auto input=Ice::createInputStream(current.adapter->getCommunicator(),bytes);input->startEncapsulation();
        auto output=Ice::createOutputStream(current.adapter->getCommunicator());output->startEncapsulation();
        if(current.operation=="writeField" || current.operation=="readField") {
            std::string reg,field;input->read(reg);input->read(field);auto &value=field=="firmwareEnable"?firmware:interrupts;
            if(current.operation=="writeField"){Ice::Long n;input->read(n);value=int(n);}
            else output->write(Ice::Long(badReadback && field=="firmwareEnable"?1:value.load()));
        }else if(current.operation=="daqStop"){++daq;if(failDaq)throw Ice::UnknownException(__FILE__,__LINE__,"injected sender stop failure");}
        else throw Ice::OperationNotExistException(__FILE__,__LINE__);
        input->endEncapsulation();output->endEncapsulation();output->finished(response);return true;
    }
};
int main(){
    auto properties=Ice::createProperties();properties->setProperty("Ice.RetryIntervals","-1");Ice::InitializationData init;init.properties=properties;auto communicator=Ice::initialize(init);
    IceUtil::Handle<BlockedPulser> servant=new BlockedPulser;auto adapter=communicator->createObjectAdapterWithEndpoints("BlockedStop","tcp -h 127.0.0.1 -p 0");
    auto proxy=adapter->addWithUUID(servant)->ice_collocationOptimized(false);adapter->activate();mdaq::rc::HardwareNode hardware;hardware.hwServerCtrlPrx=mdaq::hw::NodePrx::uncheckedCast(proxy);
    bool timedOut=false;const auto start=std::chrono::steady_clock::now();
    {Probe node(&hardware);try{node.preStop();}catch(const Ice::TimeoutException &){timedOut=true;}catch(const std::exception &e){std::cerr<<e.what()<<'\n';}}
    const auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-start).count();servant->Release();
    bool orderingPassed=true;
    for(int failure=0;failure<3;++failure){
        IceUtil::Handle<Control> control=new Control;control->failDaq=failure==1;control->badReadback=failure==2;
        auto ctrlProxy=adapter->addWithUUID(control)->ice_collocationOptimized(false);
        IceUtil::Handle<BlockedPulser> pulser=new BlockedPulser;pulser->firmware=&control->firmware;pulser->interrupts=&control->interrupts;pulser->daq=&control->daq;
        auto pulserProxy=adapter->addWithUUID(pulser)->ice_collocationOptimized(false);
        mdaq::rc::HardwareNode h;h.hwServerCtrlPrx=mdaq::hw::NodePrx::uncheckedCast(pulserProxy);h.dataSenderCtrlPrx=mdaq::DaqCtrlNodePrx::uncheckedCast(ctrlProxy);
        h.regProcMap.insert("ctrl",std::auto_ptr<mdaq::hw::RegisterProcessor>(new mdaq::hw::RemoteRegisterProcessor(mdaq::hw::DevicePrx::uncheckedCast(ctrlProxy),false)));
        bool failed=false;{Probe node(&h);try{node.impl_stop();}catch(const mdaq::utl::CmdException &){failed=true;}}
        pulser->Release();orderingPassed=orderingPassed && (failed==(failure!=0)) && control->firmware==0 && control->interrupts==0 && control->daq==1 && pulser->calls==1;
    }
    bool startPassed=true;
    for(int mode=0;mode<3;++mode){
        IceUtil::Handle<BlockedPulser> pulser=new BlockedPulser;pulser->failStart=mode==2;
        auto p=adapter->addWithUUID(pulser)->ice_collocationOptimized(false);
        mdaq::rc::HardwareNode h;h.hwServerCtrlPrx=mdaq::hw::NodePrx::uncheckedCast(p);
        bool failed=false;{Probe node(&h);node.PeriodicMode(mode!=0);try{node.postStart();}catch(const mdaq::utl::CmdException &e){failed=e.reason=="injected periodic pulser start failure";}}
        startPassed=startPassed && (failed==(mode==2)) && pulser->startCalls==(mode==0?0:1);
    }
    adapter->deactivate();adapter->waitForDeactivate();communicator->destroy();
    if(!startPassed || !orderingPassed || !timedOut || elapsed<29000 || elapsed>35000 || servant->calls!=1){std::cerr<<"FAIL start="<<startPassed<<" timeout="<<timedOut<<" ms="<<elapsed<<" calls="<<servant->calls<<'\n';return 1;}
    std::cout<<"PASS original pulser stop propagated TimeoutException after "<<elapsed<<" ms; one RPC, no retry, no fabricated success; Stop order, independent failures and pulser Start failure propagation passed\n";
}
