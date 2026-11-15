#include "controlledicesource.h"
#include "sourceringreader.h"
#include "sourcemfmring.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace GetDAQ;
namespace {
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();}catch(...) {rejected=true;}Require(rejected,"Expected rejection");}
std::vector<std::uint8_t> Frame(unsigned int event) {
    std::vector<std::uint8_t> bytes(4096,std::uint8_t(event));const unsigned char header[]={6,0,0,64,7,0,1,4,0,2,0,4,0,0,3,224};
    for(unsigned int i=0;i<sizeof(header);++i)bytes[i]=header[i];
    for(unsigned int i=0;i<4;++i)bytes[22+i]=std::uint8_t(event>>(24-8*i));
    bytes[26]=bytes[27]=0;return bytes;
}
struct Producer {
    std::vector<std::uint8_t> memory=std::vector<std::uint8_t>(6144);
    std::atomic<std::uint64_t> write{0},read{0};
    std::atomic<bool> stop{false},done{true},publishFault{false};
    std::thread device,consumer;
    unsigned int event=0;
    ControlledIceSource *source=nullptr;
    std::unique_ptr<SourceRingReader> reader;
    std::unique_ptr<SourceMfmRing> framed;
    std::string mode;
    explicit Producer(const std::string &selected):mode(selected) {
        publishFault=mode=="publish-fault";
        SourceRing ring;ring.memory=memory.data();ring.capacity=memory.size();
        ring.writeTotal=[this] {return write.load();};
        ring.publishReadTotal=[this](std::uint64_t total) {read.store(total);if(publishFault)throw std::runtime_error("Read register ACK lost after local send");};
        framed.reset(new SourceMfmRing(ring,4096));
        reader.reset(new SourceRingReader({framed->FramedView()},[this](const std::uint8_t *p,std::size_t n) {source->Feed(p,n);},
            [this](const std::string &error) {source->ReportFailure(error);},317,1024));
        reader->Configure(1,false);
    }
    void Idle() {Require(!device.joinable() && !consumer.joinable(),"Ring producer has not joined");}
    void Start() {
        Idle();stop=false;done=false;
        device=std::thread([this] {
            for(unsigned int frame=0;frame<3 && !stop;++frame) {
                const auto bytes=Frame(++event);std::size_t offset=0;
                const auto target=frame==2 && mode=="partial-header" ? std::size_t(7) : frame==2 && mode=="partial-body" ? std::size_t(2048) : bytes.size();
                while(offset<target && !stop) {
                    const auto w=write.load(),r=read.load();const auto room=memory.size()-std::size_t(w-r);
                    const auto count=std::min(std::min(room,target-offset),std::size_t(701));
                    if(!count) {std::this_thread::yield();continue;}
                    for(std::size_t i=0;i<count;++i)memory[std::size_t((w+i)%memory.size())]=bytes[offset+i];
                    write.store(w+count);offset+=count;
                }
            }
            done=true;
        });
        consumer=std::thread([this] {
            try {while(!stop) {reader->Flush(1537);std::this_thread::yield();}}
            catch(...) {stop=true;} // Reader already latched/report failure; never retry.
        });
    }
    void Stop() {stop=true;if(device.joinable())device.join();if(consumer.joinable())consumer.join();}
    void Drain() {Idle();while(!reader->Empty())Require(reader->Flush(1537,true)>0,"Drain made no progress");reader->VerifyDrained();}
    IceProducerCallbacks Callbacks() {IceProducerCallbacks c;c.verifyIdle=[this] {Idle();};c.start=[this] {Start();};c.stop=[this] {Stop();};c.drain=[this] {Drain();};return c;}
    ~Producer() {Stop();}
};
}
int main(int argc,char **argv) {
    try {
        Require(argc==6,"Pass DATA_PORT SESSION ENCODING MODE RESULT");const std::string mode=argv[4];
        Producer producer(mode);IceSourceOptions options;options.port=unsigned(std::stoul(argv[1]));options.expectedSession=argv[2];options.encodingMinor=unsigned(std::stoul(argv[3]));options.chunkBytes=317;options.timeoutMs=1000;
        ControlledIceSource source(options,producer.Callbacks());producer.source=&source;source.Connect();
        if(mode=="cycles") {
            for(unsigned int cycle=0;cycle<3;++cycle) {
                source.Start();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                while(!producer.done && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
                Require(producer.done,"Device fixture timeout");source.Stop();Require(producer.reader->Empty() && producer.read==producer.write,"Tail not drained");
            }
            source.Disconnect();Require(source.Status().stream.sourceEndAcknowledged,"No source End ACK");
        } else if(mode=="empty") {source.Disconnect();}
        else if(mode=="partial-header" || mode=="partial-body") {
            source.Start();const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while(!producer.done && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
            Require(producer.done,"Partial producer fixture timeout");Reject([&] {source.Stop();});
            Require(source.Status().state==ControlledIceSourceSnapshot::Unknown && producer.framed->Status().unknown,"Raw partial tail was treated as drained");
            source.StopProductionAfterFailure();Reject([&] {source.Disconnect();});
        }
        else if(mode=="publish-fault") {
            try {source.Start();}catch(...) {}
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
            while(!producer.reader->Status().unknown && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
            Require(producer.reader->Status().unknown && source.Status().state==ControlledIceSourceSnapshot::Unknown,"Pointer fault was not propagated");
            source.StopProductionAfterFailure();Reject([&] {source.Disconnect();});Reject([&] {producer.reader->Flush(1537,true);});
            Require(!source.Status().stream.sourceEndAcknowledged,"Pointer uncertainty fabricated End");
        } else throw std::invalid_argument("Unknown fixture mode");
        producer.Stop();const auto rings=producer.reader->Status();const auto raw=producer.framed->Status();const auto status=source.Status();
        std::ofstream result(argv[5]);result<<"{\"passed\":true,\"ring_unknown\":"<<(rings.unknown?"true":"false")<<",\"source_unknown\":"<<(status.state==ControlledIceSourceSnapshot::Unknown?"true":"false")
            <<",\"handed_total\":"<<rings.rings[0].handedTotal<<",\"published_total\":"<<rings.rings[0].publishedTotal<<",\"raw_write_total\":"<<raw.rawWriteTotal<<",\"complete_write_total\":"<<raw.completeWriteTotal<<",\"raw_unknown\":"<<(raw.unknown?"true":"false")<<",\"end_acknowledged\":"<<(status.stream.sourceEndAcknowledged?"true":"false")<<"}\n";
        Require(bool(result),"Cannot write result");std::cout<<"PASS ring source "<<mode<<'\n';return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
