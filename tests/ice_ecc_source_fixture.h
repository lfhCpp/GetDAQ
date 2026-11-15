#ifndef GETDAQ_ICE_ECC_SOURCE_FIXTURE_H
#define GETDAQ_ICE_ECC_SOURCE_FIXTURE_H
#include "native_receiver_fixture.h"
#include "sourcecontrolservant.h"
#include "sourceringreadout.h"
#include "sourcemfmring.h"
#include <atomic>
#include <condition_variable>
namespace GetDAQReceiverFixture {
struct IceProducer {
    World &world;std::string id;
    GetDAQ::SourceControlServant *source=nullptr;
    std::thread device;
    std::mutex mutex,interruptMutex;
    std::condition_variable interrupt;
    std::atomic<std::uint64_t> write[2],read[2];
    std::vector<std::uint8_t> memory[2]={std::vector<std::uint8_t>(6144),std::vector<std::uint8_t>(6144)};
    unsigned int event=0;
    std::atomic<bool> ready{false};
    std::string expected;
    std::unique_ptr<GetDAQ::SourceRingReadout> readout;
    std::unique_ptr<GetDAQ::SourceMfmRing> framed[2];
    explicit IceProducer(World &w,const std::string &name):world(w),id(name) {
        std::vector<GetDAQ::SourceRing> rings;
        for(unsigned int i=0;i<2;++i) {
            write[i]=read[i]=0;
            GetDAQ::SourceRing ring;ring.id=i ? 3 : 0;ring.memory=memory[i].data();ring.capacity=memory[i].size();
            ring.writeTotal=[this,i] {return write[i].load();};
            ring.publishReadTotal=[this,i](std::uint64_t total) {read[i]=total;interrupt.notify_all();};
            framed[i].reset(new GetDAQ::SourceMfmRing(ring,4096));rings.push_back(framed[i]->FramedView());
        }
        GetDAQ::SourceReadoutDriver driver;driver.verifyIdle=[this] {DeviceIdle();};
        driver.start=[this] {DeviceStart();};driver.stop=[this] {DeviceStop();};
        driver.wait=[this](std::chrono::milliseconds maximum) {std::unique_lock<std::mutex> lock(interruptMutex);interrupt.wait_for(lock,maximum);};
        readout.reset(new GetDAQ::SourceRingReadout(rings,driver,
            [this](const std::uint8_t *p,std::size_t n) {source->Feed(p,n);},
            [this](const std::string &error) {source->ReportFailure(error);},317,1024,1537));
    }
    void DeviceIdle() {Require(!device.joinable(),"ICE fixture device thread not joined");}
    void DeviceStart() {
        DeviceIdle();world.Record(id+".sender-start");ready=false;
        device=std::thread([this] {
            for(unsigned int i=0;i<2;++i) {
                const auto data=Frame(++event);std::size_t offset=0;
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while(offset<data.size()) {
                    const auto w=write[i].load(),r=read[i].load();
                    const auto room=memory[i].size()-std::size_t(w-r);
                    const auto count=std::min(std::min(room,data.size()-offset),std::size_t(701));
                    if(!count) {
                        if(std::chrono::steady_clock::now()>deadline) {source->ReportFailure("Synthetic device backpressure timeout");return;}
                        std::unique_lock<std::mutex> lock(interruptMutex);interrupt.wait_for(lock,std::chrono::milliseconds(1));continue;
                    }
                    for(std::size_t j=0;j<count;++j)memory[i][std::size_t((w+j)%memory[i].size())]=std::uint8_t(data[offset+j]);
                    write[i]=w+count;offset+=count;
                }
                interrupt.notify_all();
                std::lock_guard<std::mutex> lock(mutex);expected+=data;
            }
            ready=true;interrupt.notify_all();
        });
        // Stage one whole frame per AsAd before exposing this synthetic interrupt batch.
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        std::unique_lock<std::mutex> lock(interruptMutex);
        while(!ready && std::chrono::steady_clock::now()<deadline)interrupt.wait_for(lock,std::chrono::milliseconds(1));
        Require(ready,"Synthetic whole-frame batch not ready");
    }
    void DeviceStop() {world.Record(id+".sender-stop");if(device.joinable())device.join();}
    void Idle() {readout->VerifyIdle();}
    void Start() {readout->Start();}
    void Stop() {readout->StopAndJoin();}
    void Drain() {readout->Drain();}
    void Policy(unsigned int mask,bool flush) {readout->Configure(mask,flush);}
    void Reset() {Idle();Require(read[0]==write[0] && read[1]==write[1],"Prior ICE fixture ring not drained");std::lock_guard<std::mutex> lock(mutex);event=0;expected.clear();}
    std::string Expected() {std::lock_guard<std::mutex> lock(mutex);return expected;}
    GetDAQ::IceProducerCallbacks Callbacks() {GetDAQ::IceProducerCallbacks c;c.verifyIdle=[this] {Idle();};c.start=[this] {Start();};c.stop=[this] {Stop();};c.drain=[this] {Drain();};return c;}
    ~IceProducer() {if(device.joinable())device.join();}
};
class EccIceSource : public GetDAQ::SourceControlServant {
    IceProducer &producer;
public:
    EccIceSource(IceProducer &p,const GetDAQ::SourceControlOptions &options):SourceControlServant(options,p.Callbacks(),[&p](unsigned int mask,bool flush) {
        p.Policy(mask,flush);p.world.Record(p.id+".sender-mask="+std::to_string(mask));p.world.Record(p.id+".sender-flush="+std::to_string(flush));}),producer(p) {p.source=this;}
    void connectIceStream(const std::string &receiver,const std::string &session,const std::string &id,const std::string &instance,const Ice::Current &current) override {
        producer.Reset();SourceControlServant::connectIceStream(receiver,session,id,instance,current);producer.world.Record(producer.id+".sender-connect");
    }
};
}
#endif
