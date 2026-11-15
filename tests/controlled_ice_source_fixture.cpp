#include "controlledicesource.h"
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
std::vector<std::uint8_t> Frame(unsigned int event,bool little=false) {
    std::vector<std::uint8_t> bytes(4096,std::uint8_t(event));bytes[0]=std::uint8_t(6|(little ? 128 : 0));bytes[4]=7;bytes[7]=4;
    const auto put=[&](unsigned int offset,std::uint64_t value,unsigned int count) {for(unsigned int i=0;i<count;++i) bytes[offset+i]=std::uint8_t(value>>(little ? 8*i : 8*(count-i-1)));};
    put(1,64,3);put(5,1,2);put(8,2,2);put(10,4,2);put(12,992,4);put(22,event,4);bytes[26]=bytes[27]=0;
    for(unsigned int i=128;i<136;++i)bytes[i]=i<132 ? 0 : 255;
    return bytes;
}
void Status(const GetDAQ::ControlledIceSourceSnapshot &status) {
    std::cout<<"{\"state\":"<<int(status.state)<<",\"issued_bytes\":"<<status.stream.issuedBytes<<",\"locally_sent_bytes\":"<<status.stream.locallySentBytes
        <<",\"start_issued\":"<<(status.startIssued ? "true" : "false")<<",\"stop_acknowledged\":"<<(status.stopAcknowledged ? "true" : "false")
        <<",\"drain_acknowledged\":"<<(status.drainAcknowledged ? "true" : "false")<<",\"finish_issued\":"<<(status.stream.finishIssued ? "true" : "false")
        <<",\"source_end_acknowledged\":"<<(status.stream.sourceEndAcknowledged ? "true" : "false")<<"}\n";
}
struct Producer {
    GetDAQ::ControlledIceSource *source=nullptr;
    std::thread worker;
    std::atomic<bool> running{false};
    bool failDrain=false,nonstandardDrain=false,idleFailure=false;
    unsigned int event=1,starts=0,stops=0,drains=0;
    ~Producer() {if(worker.joinable()) worker.join();}
    void Idle() {Require(!running && !worker.joinable() && !idleFailure,"Producer still has an unjoined active worker");}
    void Start() {
        Idle();++starts;running=true;const auto value=event++;
        worker=std::thread([this,value] {try {const auto frame=Frame(value,value%2==0);source->Feed(frame.data(),frame.size());} catch(...) {running=false;return;}running=false;});
    }
    void Stop() {++stops;if(worker.joinable()) worker.join();running=false;}
    void Drain() {
        ++drains;if(nonstandardDrain) throw 7;
        if(failDrain) throw std::runtime_error("Synthetic source tail drain failed");
        // A real source may retain a tail until flush. Exercise Feed from inside
        // drain: Stop must not hold a mutex that prevents this callback.
        if(starts && drains==1) {const auto frame=Frame(event++);source->Feed(frame.data(),frame.size());}
    }
};
}
int main(int argc,char **argv) {
    try {
        Require(argc==6,"Pass PORT ENCODING SESSION MODE READY_FILE");const std::string mode=argv[4];Producer producer;
        GetDAQ::IceSourceOptions options;options.port=unsigned(std::stoul(argv[1]));options.encodingMinor=unsigned(std::stoul(argv[2]));options.expectedSession=argv[3];options.timeoutMs=1000;options.chunkBytes=317;
        if(mode=="wrong-session") options.expectedSession=std::string(64,'b');
        GetDAQ::IceProducerCallbacks callbacks;callbacks.verifyIdle=[&] {producer.Idle();};callbacks.start=[&] {producer.Start();};callbacks.stop=[&] {producer.Stop();};callbacks.drain=[&] {producer.Drain();};
        if(mode=="missing-callback") {callbacks.stop={};bool rejected=false;try {GetDAQ::ControlledIceSource invalid(options,callbacks);} catch(const std::invalid_argument &) {rejected=true;}Require(rejected,"Missing callback accepted");std::cout<<"PASS missing callbacks rejected before connect\n";return 0;}
        GetDAQ::ControlledIceSource source(options,callbacks);producer.source=&source;
        if(mode=="wrong-session" || mode=="idle-connect") {
            producer.idleFailure=mode=="idle-connect";bool rejected=false;try {source.Connect();} catch(const std::exception &) {rejected=true;}
            Require(rejected && source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown && !source.Status().stream.issuedBytes,"Source connect preflight sent data or lost UNKNOWN");Status(source.Status());return 0;
        }
        source.Connect();const auto session=source.Status().stream.sessionId;Require(session==argv[3],"Source session differs");
        if(mode=="empty") {source.Disconnect();Require(!producer.starts && !source.Status().stream.locallySentBytes,"Empty source started producer");}
        else {
            source.Start();
            if(mode=="drain-fail") producer.failDrain=true;
            if(mode=="nonstd-drain") producer.nonstandardDrain=true;
            if(mode=="idle-stop") producer.idleFailure=true;
            if(mode=="drain-fail" || mode=="idle-stop" || mode=="nonstd-drain") {
                bool rejected=false;try {source.Stop();} catch(...) {rejected=true;}
                Require(rejected && source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown && !source.Status().stream.finishIssued,"Failed quiesce/drain issued End");
                rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}Require(rejected,"UNKNOWN source disconnected/replayed End");producer.idleFailure=false;source.StopProductionAfterFailure();Require(source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown && source.Status().failureStopIssued && source.Status().failureStopAcknowledged && !source.Status().stream.finishIssued,"Failure stop cleared UNKNOWN or sent End");
                rejected=false;try {source.StopProductionAfterFailure();} catch(const std::exception &) {rejected=true;}Require(rejected,"Failure stop allowed replay");Status(source.Status());return 0;
            }
            source.Stop();const auto paused=source.Status();Require(paused.state==GetDAQ::ControlledIceSourceSnapshot::Stopped && !paused.stream.finishIssued && paused.stream.locallySentBytes==8192,"Pause lost tail or ended stream");
            if(mode=="late-feed") {
                const auto frame=Frame(9);bool rejected=false;try {source.Feed(frame.data(),frame.size());} catch(const std::exception &) {rejected=true;}
                Require(rejected && source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown,"Late producer bytes did not latch UNKNOWN");
                rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}Require(rejected,"Late producer input allowed End");Status(source.Status());return 0;
            }
            if(mode=="destroy") {Status(paused);return 0;}
            if(mode=="end-loss") {
                std::ofstream ready(argv[5]);ready<<"ready\n";ready.close();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
                while(!std::ifstream(std::string(argv[5])+".continue")) {Require(std::chrono::steady_clock::now()<deadline,"Fault fixture has no replacement-ready marker");std::this_thread::sleep_for(std::chrono::milliseconds(10));}
                bool rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}
                Require(rejected && source.Status().state==GetDAQ::ControlledIceSourceSnapshot::Unknown && !source.Status().stream.sourceEndAcknowledged,"End loss manufactured completion");
                rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}Require(rejected,"End loss allowed replay");Status(source.Status());return 0;
            }
            Require(mode=="cycles","Unknown controlled source test mode");
            source.Start();source.Stop();source.Start();source.Stop();Require(source.Status().stream.sessionId==session && !source.Status().stream.finishIssued,"Run cycles replaced/ended stream");source.Disconnect();
            Require(producer.starts==3 && source.Status().stream.locallySentBytes==16384,"Retained source cycle counts differ");
        }
        const auto ended=source.Status();Require(ended.state==GetDAQ::ControlledIceSourceSnapshot::Disconnected && ended.stream.sourceEndAcknowledged && ended.stopAcknowledged && ended.drainAcknowledged,"Final source quiesce/drain/End differs");
        bool rejected=false;try {source.Disconnect();} catch(const std::exception &) {rejected=true;}Require(rejected,"Completed source replayed End");Status(ended);return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
