#include "sourceringreadout.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <thread>
using namespace GetDAQ;
namespace {
void Require(bool value,const char *text) {if(!value)throw std::runtime_error(text);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();}catch(...) {rejected=true;}Require(rejected,"Expected rejection");}
struct Driver {
    std::vector<std::uint8_t> memory=std::vector<std::uint8_t>(32);
    std::atomic<std::uint64_t> write{0},read{0};
    std::atomic<bool> idle{true},waitFault{false};
    std::atomic<unsigned int> starts{0},stops{0},faults{0};
    bool startFault=false,stopFault=false,lieIdle=false,continuous=false;
    std::mutex mutex;
    std::condition_variable changed;
    std::vector<std::uint8_t> sent;
    void Append(std::size_t count) {
        const auto w=write.load();Require(w-read+count<=memory.size(),"Test ring overflow");
        for(std::size_t i=0;i<count;++i)memory[std::size_t((w+i)%memory.size())]=std::uint8_t(w+i);
        write=w+count;changed.notify_all();
    }
    SourceRing Ring() {
        SourceRing r;r.memory=memory.data();r.capacity=memory.size();r.writeTotal=[this] {return write.load();};
        r.publishReadTotal=[this](std::uint64_t value) {read=value;if(continuous)Append(4);};return r;
    }
    SourceReadoutDriver Callbacks() {
        SourceReadoutDriver d;d.verifyIdle=[this] {Require(idle || lieIdle,"Device not confirmed idle");};
        d.start=[this] {++starts;idle=false;if(startFault)throw std::runtime_error("Start ACK lost");Append(19);};
        d.stop=[this] {++stops;idle=true;if(stopFault)throw std::runtime_error("Stop ACK lost after device stop");};
        d.wait=[this](std::chrono::milliseconds maximum) {if(waitFault)throw 9;std::unique_lock<std::mutex> lock(mutex);changed.wait_for(lock,maximum);};return d;
    }
    SourceRingReader::Send Sender() {return [this](const std::uint8_t *p,std::size_t n) {std::lock_guard<std::mutex> lock(mutex);sent.insert(sent.end(),p,p+n);};}
    std::function<void(const std::string &)> Fault() {return [this](const std::string &) {++faults;};}
};
void Wait(const std::function<bool()> &predicate) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(2);
    while(!predicate() && std::chrono::steady_clock::now()<end)std::this_thread::yield();
    Require(predicate(),"Readout observation timeout");
}
}
int main() {
    try {
        unsigned int cases=0;
        {
            Driver d;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);r.Configure(1,false);
            for(unsigned int i=0;i<3;++i) {r.Start();Wait([&] {return d.read>=d.write-7;});r.StopAndJoin();r.VerifyIdle();r.Drain();Require(d.read==d.write,"Readout tail remained");}
            const auto status=r.Status();Require(!status.unknown && !status.workerRunning && status.deviceStopAcknowledged && d.starts==3 && d.stops==3 && d.sent.size()==57,"Normal cycles/lifecycle");
            for(std::size_t i=0;i<d.sent.size();++i)Require(d.sent[i]==i,"Exact wrapped stream");
            ++cases;
        }
        {
            Driver d;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);
            r.VerifyIdle();r.StopAndJoin();r.Drain();Require(d.starts==0 && d.stops==0 && d.sent.empty(),"Empty readout changed hardware");++cases;
        }
        {
            Driver d;d.startFault=true;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);
            Reject([&] {r.Start();});Require(r.Status().unknown && !r.Status().workerRunning && d.starts==1,"Start fault lost state");
            Reject([&] {r.Start();});r.StopAndJoin();Require(d.stops==1 && r.Status().unknown,"Explicit stop after start uncertainty");Reject([&] {r.Drain();});++cases;
        }
        {
            Driver d;d.stopFault=true;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);r.Start();
            Reject([&] {r.StopAndJoin();});Require(r.Status().unknown && !r.Status().workerRunning,"Stop failure did not join local worker");
            Reject([&] {r.StopAndJoin();});Reject([&] {r.Drain();});Require(d.stops==1 && !r.Status().deviceStopAcknowledged,"Unknown device stop replayed");++cases;
        }
        {
            Driver d;d.waitFault=true;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);r.Start();
            Wait([&] {return r.Status().unknown;});r.StopAndJoin();Require(!r.Status().workerRunning && d.stops==1,"Wait failure not joinable");Reject([&] {r.Drain();});++cases;
        }
        {
            Driver d;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);r.Start();Wait([&] {return d.read==d.write;});
            Reject([&] {r.Configure(1,true);});r.StopAndJoin();Require(r.Status().unknown,"Running policy failed to latch");++cases;
        }
        {
            Driver d;d.continuous=true;d.Append(4);SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),4,8,4);
            Reject([&] {r.Drain();});Require(r.Status().unknown && d.sent.size()==32,"Drain not statically bounded with lying idle callback");++cases;
        }
        {
            Driver d;{SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);r.Start();Wait([&] {return d.read==d.write;});}
            Require(d.starts==1 && d.stops==0 && !d.idle,"Destructor must only cancel/join local worker");d.idle=true;++cases;
        }
        {
            Driver d;d.idle=false;SourceRingReadout r({d.Ring()},d.Callbacks(),d.Sender(),d.Fault(),7,8,11);
            Reject([&] {r.VerifyIdle();});Require(r.Status().unknown && d.starts==0,"Idle check ignored actual driver");++cases;
        }
        std::cout<<"PASS "<<cases<<" source readout worker/device-stop/drain/UNKNOWN cases; no payload files\n";return 0;
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
