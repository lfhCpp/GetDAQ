#include "sourceringreader.h"
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
using namespace GetDAQ;
namespace {
void Require(bool value,const char *text) {if(!value)throw std::runtime_error(text);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();}catch(...) {rejected=true;}Require(rejected,"Expected rejection");}
struct Ring {
    unsigned int id;
    std::vector<std::uint8_t> memory;
    std::uint64_t write=0,read=0;
    bool publishFault=false,observeFault=false;
    explicit Ring(unsigned int i,std::size_t capacity=16):id(i),memory(capacity) {}
    void Append(const std::string &data) {
        Require(write-read+data.size()<=memory.size(),"Fixture overrun");
        for(const unsigned char value:data)memory[std::size_t(write++%memory.size())]=value;
    }
    SourceRing View() {
        SourceRing ring;ring.id=id;ring.memory=memory.data();ring.capacity=memory.size();ring.initialReadTotal=read;ring.writeEndsAtMessageBoundary=true;
        ring.writeTotal=[this] {if(observeFault)throw 42;return write;};
        ring.publishReadTotal=[this](std::uint64_t value) {read=value;if(publishFault)throw std::runtime_error("Publish ACK lost after register write");};return ring;
    }
};
}
int main() {
    try {
        unsigned int cases=0;
        {
            Ring ring(0);std::string received;unsigned int faults=0;
            SourceRingReader reader({ring.View()},[&](const std::uint8_t *p,std::size_t n) {received.append(reinterpret_cast<const char *>(p),n);},[&](const std::string &) {++faults;},5,8);
            ring.Append("abcdefghijkl");Require(reader.Flush(9)==9 && ring.read==9,"Budget/publish");
            ring.Append("mnopqrstuvwx");Require(reader.Flush(64)==15,"Wrap chunks");
            Require(received=="abcdefghijklmnopqrstuvwx" && reader.Empty() && !faults,"Exact borrowed stream");++cases;
        }
        {
            Ring ring(0);std::string received;
            SourceRingReader reader({ring.View()},[&](const std::uint8_t *p,std::size_t n) {received.append(reinterpret_cast<const char *>(p),n);},[](const std::string &) {},5,8);
            reader.Configure(1,false);ring.Append("abc");Require(reader.Flush(64)==0 && !reader.Empty(),"Deferred tail");
            Require(reader.Flush(64,true)==3 && reader.Empty() && received=="abc","Forced drain");++cases;
        }
        {
            Ring a(0),b(3);std::string received;
            SourceRingReader reader({a.View(),b.View()},[&](const std::uint8_t *p,std::size_t n) {received.append(reinterpret_cast<const char *>(p),n);},[](const std::string &) {},2,4);
            a.Append("AAAA");b.Append("BBBB");Require(reader.Flush(3)==3 && received=="AAA","Budget may split a message");
            Require(reader.Flush(5)==5 && received=="AAAABBBB","Ring switch may not interleave partial messages across calls");
            reader.Configure(8,true);a.Append("cc");b.Append("dd");reader.Flush(8);Require(a.read==4 && b.read==6,"Mask preserves disabled data");
            Reject([&] {reader.Configure(1,true);});Require(reader.Status().unknown,"Mask may not discard unread ring");++cases;
        }
        {
            Ring ring(0);ring.Append("abcdef");unsigned int sends=0,faults=0;
            SourceRingReader reader({ring.View()},[&](const std::uint8_t *,std::size_t) {++sends;throw std::runtime_error("Unknown partial send");},[&](const std::string &) {++faults;},4,8);
            Reject([&] {reader.Flush(16);});Reject([&] {reader.Flush(16,true);});Reject([&] {reader.Empty();});Reject([&] {reader.Configure(1,true);});
            Require(reader.Status().unknown && ring.read==0 && sends==1 && faults==1,"Failed send publishes/replays nothing");++cases;
        }
        {
            Ring ring(0);ring.Append("abcdef");ring.publishFault=true;unsigned int sends=0;
            SourceRingReader reader({ring.View()},[&](const std::uint8_t *,std::size_t) {++sends;},[](const std::string &) {},4,8);
            Reject([&] {reader.Flush(16);});const auto progress=reader.Status().rings.front();Reject([&] {reader.Flush(16);});
            Require(reader.Status().unknown && ring.read==4 && progress.handedTotal==4 && progress.publishedTotal==0 && sends==1,"Lost register ACK cannot replay sent bytes");++cases;
        }
        {
            Ring ring(0);unsigned int sends=0;SourceRingReader reader({ring.View()},[&](const std::uint8_t *,std::size_t) {++sends;},[](const std::string &) {},4,8);
            ring.write=17;Reject([&] {reader.Flush(16);});Require(reader.Status().unknown && sends==0 && ring.read==0,"Overrun before send");++cases;
        }
        {
            Ring ring(0);SourceRingReader reader({ring.View()},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},4,8);
            ring.Append("abcd");reader.Flush(16);ring.write=3;Reject([&] {reader.Empty();});Require(reader.Status().unknown,"Counter reset latched");++cases;
        }
        {
            Ring ring(0);SourceRingReader reader({ring.View()},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {throw 7;},4,8);
            ring.observeFault=true;Reject([&] {reader.Empty();});Require(reader.Status().unknown && !reader.Status().error.empty(),"Nonstandard callback remains unknown");++cases;
        }
        {
            Ring ring(0);ring.Append("abcdefghijklmnop");std::size_t bytes=0;
            SourceRingReader reader({ring.View()},[&](const std::uint8_t *,std::size_t n) {bytes+=n;},[](const std::string &) {},32,16);
            Require(reader.Flush(16)==16 && bytes==16 && reader.Empty(),"Full ring determined by cumulative counters");++cases;
        }
        {
            Ring ring(0);Reject([&] {SourceRingReader r({ring.View(),ring.View()},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},1,1);});
            SourceRingReader reader({ring.View()},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},1,1);
            Reject([&] {reader.Configure(2,true);});Require(!reader.Status().unknown && reader.Flush(0)==0,"Invalid policy changes no pointer/state");++cases;
        }
        {
            Ring a(0),b(3);auto unbounded=b.View();unbounded.writeEndsAtMessageBoundary=false;
            Reject([&] {SourceRingReader reader({a.View(),unbounded},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},2,4);});++cases;
        }
        {
            Reject([] {LegacyRingWriteTracker t(1000,16,1000,0,false);});LegacyRingWriteTracker tracker(1000,16,1012,12,true);
            Require(tracker.Observe(1016)==16 && tracker.Observe(1003)==19 && tracker.Observe(1003)==19,"Legacy wrap/end normalization");
            Reject([&] {tracker.Observe(1017);});Reject([&] {tracker.Observe(1004);});++cases;
        }
        {
            const auto max=std::numeric_limits<std::uint64_t>::max();LegacyRingWriteTracker tracker(1000,16,1014,max-1,true);
            Require(tracker.Observe(1015)==max,"Legacy counter maximum");Reject([&] {tracker.Observe(1000);});++cases;
        }
        std::cout<<"PASS "<<cases<<" source ring pointer/budget/wrap/policy/failure cases; no payload files\n";return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
