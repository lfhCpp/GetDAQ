#include "sourceringreadout.h"
#include <algorithm>
#include <atomic>
#include <exception>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <thread>
namespace GetDAQ {
class SourceRingReadout::Impl {
public:
    SourceReadoutDriver driver;
    std::function<void(const std::string &)> fault;
    SourceRingReader reader;
    std::size_t work;
    std::uint64_t maximumDrain=0;
    std::thread worker;
    std::atomic<bool> cancel{false};
    mutable std::mutex operationMutex,statusMutex;
    SourceReadoutStatus status;
    Impl(const std::vector<SourceRing> &rings,const SourceReadoutDriver &d,const SourceRingReader::Send &send,
         const std::function<void(const std::string &)> &report,std::size_t chunk,std::size_t batch,std::size_t budget)
        :driver(d),fault(report),reader(rings,send,[this](const std::string &error) {Fail(error);},chunk,batch),work(budget) {
        if(!driver.verifyIdle || !driver.start || !driver.stop || !driver.wait || !work)
            throw std::invalid_argument("Source readout requires idle/start/stop/bounded-wait callbacks and work budget");
        for(const auto &ring:rings) {
            if(ring.capacity>std::numeric_limits<std::uint64_t>::max()-maximumDrain)throw std::overflow_error("Source drain capacity overflow");
            maximumDrain+=ring.capacity;
        }
    }
    void Fail(const std::string &error) {
        cancel=true;
        std::string message;
        {std::lock_guard<std::mutex> lock(statusMutex);status.unknown=true;if(status.error.empty())status.error=error;message=status.error;}
        try {fault(message);}catch(...) {} // Caller must latch source failure without entering this readout.
    }
    void Known() const {std::lock_guard<std::mutex> lock(statusMutex);if(status.unknown)throw std::logic_error("Source readout UNKNOWN; no normal control replay");}
    void Idle() {if(worker.joinable())throw std::logic_error("Source readout worker is not joined");driver.verifyIdle();}
    void Join() {cancel=true;if(worker.joinable())worker.join();}
    void Run() {
        {std::lock_guard<std::mutex> lock(statusMutex);status.workerRunning=true;}
        try {while(!cancel) {if(!reader.Flush(work))driver.wait(std::chrono::milliseconds(50));}}
        catch(const std::exception &e) {Fail(e.what());cancel=true;}
        catch(...) {Fail("Source readout worker raised a nonstandard exception");cancel=true;}
        {std::lock_guard<std::mutex> lock(statusMutex);status.workerRunning=false;}
    }
};
SourceRingReadout::SourceRingReadout(const std::vector<SourceRing> &rings,const SourceReadoutDriver &driver,
                                   const SourceRingReader::Send &send,const std::function<void(const std::string &)> &fault,
                                   std::size_t chunk,std::size_t batch,std::size_t work)
    :fImpl(new Impl(rings,driver,send,fault,chunk,batch,work)) {}
SourceRingReadout::~SourceRingReadout() {fImpl->Join();}
SourceReadoutStatus SourceRingReadout::Status() const {
    auto &p=*fImpl;SourceReadoutStatus result;
    {std::lock_guard<std::mutex> lock(p.statusMutex);result=p.status;}
    result.buffers=p.reader.Status();return result;
}
void SourceRingReadout::VerifyIdle() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operationMutex);
    try {p.Idle();}catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Source idle check raised a nonstandard exception");throw;}
}
void SourceRingReadout::Configure(unsigned int mask,bool flush) {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operationMutex);p.Known();
    try {p.Idle();p.reader.Configure(mask,flush);}catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Source configuration raised a nonstandard exception");throw;}
}
void SourceRingReadout::Start() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operationMutex);p.Known();
    try {
        p.Idle();if(!p.reader.Empty())throw std::logic_error("Source start requires prior buffered data drained");
        p.reader.VerifyDrained();
        {std::lock_guard<std::mutex> status(p.statusMutex);p.status.startIssued=true;p.status.stopIssued=false;p.status.deviceStopAcknowledged=false;}
        p.cancel=false;p.driver.start();p.Known();p.worker=std::thread([&p] {p.Run();});
    }catch(const std::exception &e) {p.Fail(e.what());p.Join();throw;}catch(...) {p.Fail("Source start raised a nonstandard exception");p.Join();throw;}
}
void SourceRingReadout::StopAndJoin() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operationMutex);bool shouldStop=false,repeatUnknown=false;
    {std::lock_guard<std::mutex> status(p.statusMutex);
        shouldStop=p.status.startIssued && !p.status.stopIssued;
        repeatUnknown=p.status.stopIssued && !p.status.deviceStopAcknowledged;
        if(shouldStop)p.status.stopIssued=true;
    }
    std::exception_ptr error;
    if(shouldStop) {
        try {p.driver.stop();std::lock_guard<std::mutex> status(p.statusMutex);p.status.deviceStopAcknowledged=true;}
        catch(const std::exception &e) {p.Fail(e.what());error=std::current_exception();}
        catch(...) {p.Fail("Source device stop raised a nonstandard exception");error=std::current_exception();}
    }
    p.Join(); // Still join the local consumer when device stop is uncertain; never drain then.
    if(error)std::rethrow_exception(error);
    if(repeatUnknown)throw std::logic_error("Device stop outcome UNKNOWN; no repeated device command");
    try {p.Idle();}catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Source stop idle check raised a nonstandard exception");throw;}
}
void SourceRingReadout::Drain() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operationMutex);p.Known();
    try {
        p.Idle();std::uint64_t drained=0;
        while(!p.reader.Empty()) {
            if(drained==p.maximumDrain)throw std::runtime_error("Source drain exceeded static ring capacity; producer idle promise violated");
            const auto count=p.reader.Flush(std::size_t(std::min<std::uint64_t>(p.work,p.maximumDrain-drained)),true);
            if(!count || count>p.maximumDrain-drained)throw std::runtime_error("Source drain exceeded static ring capacity; producer idle promise violated");
            drained+=count;
        }
        p.reader.VerifyDrained();p.driver.verifyIdle();p.Known();
    }catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Source drain raised a nonstandard exception");throw;}
}
}
