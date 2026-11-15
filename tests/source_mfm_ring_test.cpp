#include "sourcemfmring.h"
#include "sourceringreadout.h"
#include "../apps/datareceiver/mfmheader.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace GetDAQ;
namespace {
void Require(bool value,const char *text) {if(!value)throw std::runtime_error(text);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();}catch(...) {rejected=true;}Require(rejected,"Expected rejection");}
void Put(std::vector<std::uint8_t> &p,std::size_t at,std::uint64_t value,unsigned count,bool little) {
    for(unsigned i=0;i<count;++i)p[at+(little?i:count-i-1)]=std::uint8_t(value>>(8*i));
}
std::vector<std::uint8_t> Frame(bool little,unsigned event,unsigned asad=0) {
    std::vector<std::uint8_t> p(128,std::uint8_t(event));p[0]=std::uint8_t(4|(little?0x80:0));Put(p,1,8,3,little);p[4]=7;Put(p,5,1,2,little);p[7]=4;
    Put(p,8,2,2,little);Put(p,10,4,2,little);Put(p,12,24,4,little);Put(p,22,event,4,little);p[26]=2;p[27]=std::uint8_t(asad);return p;
}
struct Raw {
    unsigned id;
    std::vector<std::uint8_t> memory;
    std::uint64_t write,read;
    bool publishFault=false;
    Raw(unsigned value,std::size_t capacity=384,std::uint64_t initial=0):id(value),memory(capacity),write(initial),read(initial) {}
    SourceRing View() {
        SourceRing r;r.id=id;r.memory=memory.data();r.capacity=memory.size();r.initialReadTotal=read;
        r.writeTotal=[this] {return write;};r.publishReadTotal=[this](std::uint64_t total) {read=total;if(publishFault)throw 5;};return r;
    }
    void Append(const std::vector<std::uint8_t> &data,std::size_t first,std::size_t count) {
        Require(write-read+count<=memory.size(),"Test ring overrun");
        for(std::size_t i=0;i<count;++i)memory[std::size_t((write+i)%memory.size())]=data[first+i];
        write+=count;
    }
};
SourceRingReader::Send Collect(std::vector<std::uint8_t> &data) {return [&](const std::uint8_t *p,std::size_t n) {data.insert(data.end(),p,p+n);};}
}
int main() {
    try {
        unsigned int cases=0;
        for(bool little:{false,true}) {
            const auto frame=Frame(little,0x12345678);
            for(std::size_t initial:{std::size_t(0),std::size_t(370)}) {
                Raw raw(0,384,initial);SourceMfmRing checked(raw.View(),256);std::vector<std::uint8_t> received;
                SourceRingReader reader({checked.FramedView()},Collect(received),[](const std::string &) {},17,64);
                for(std::size_t i=0;i<frame.size();++i) {
                    raw.Append(frame,i,1);reader.Flush(31,true);
                    if(i+1<frame.size())Require(raw.read==initial && received.empty(),"Partial frame became eligible");
                }
                while(!reader.Empty())reader.Flush(31,true);
                reader.VerifyDrained();Require(received==frame,"Wrapped header/body changed");
                Require(checked.Status().frames==1 && checked.Status().completeWriteTotal==initial+128,"Frame boundary count");++cases;
            }
        }
        {
            Raw a(0),b(3);SourceMfmRing ca(a.View(),256),cb(b.View(),256);const auto fa=Frame(false,1),fb=Frame(true,2,3);
            a.Append(fa,0,37);b.Append(fb,0,fb.size());std::vector<std::uint8_t> received;
            SourceRingReader reader({ca.FramedView(),cb.FramedView()},Collect(received),[](const std::string &) {},17,64);
            Require(reader.Flush(23)==23 && a.read==0,"Complete source may progress while another has a partial frame");
            a.Append(fa,37,fa.size()-37);while(!reader.Empty())reader.Flush(23,true);reader.VerifyDrained();
            auto expected=fb;expected.insert(expected.end(),fa.begin(),fa.end());Require(received==expected,"Partial source interleaved into complete source");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);const auto frame=Frame(false,1);raw.Append(frame,0,27);
            std::vector<std::uint8_t> received;SourceRingReader reader({checked.FramedView()},Collect(received),[](const std::string &) {},17,64);
            Require(reader.Empty(),"Partial header should not be an eligible frame");Reject([&] {reader.VerifyDrained();});Require(reader.Status().unknown && checked.Status().unknown && received.empty(),"Empty eligible set falsely sealed partial tail");++cases;
        }
        {
            Raw a(0),b(3);SourceMfmRing ca(a.View(),256),cb(b.View(),256);const auto frame=Frame(false,1);b.Append(frame,0,1);
            SourceRingReader reader({ca.FramedView(),cb.FramedView()},[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},17,64);
            Reject([&] {reader.Configure(1,true);});Require(reader.Status().unknown,"Mask change discarded raw partial bytes");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);auto bad=Frame(false,1);bad[27]=4;raw.Append(bad,0,28);
            Reject([&] {checked.FramedView().writeTotal();});Require(checked.Status().unknown,"Bad header must fail before payload complete");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);auto bad=Frame(false,1);Put(bad,1,24,3,false);raw.Append(bad,0,8);
            Reject([&] {checked.FramedView().writeTotal();});Require(checked.Status().unknown,"Oversized frame was not rejected from primary header");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);raw.write=385;Reject([&] {checked.FramedView().writeTotal();});Require(checked.Status().unknown,"Raw partial overrun ignored");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);const auto frame=Frame(false,1);raw.Append(frame,0,128);auto view=checked.FramedView();view.writeTotal();view.publishReadTotal(128);raw.write=127;
            Reject([&] {view.writeTotal();});Require(checked.Status().unknown,"Producer reset ignored");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);const auto frame=Frame(false,1);raw.Append(frame,0,128);raw.publishFault=true;auto view=checked.FramedView();view.writeTotal();
            Reject([&] {view.publishReadTotal(17);});Reject([&] {view.publishReadTotal(17);});Require(raw.read==17 && checked.Status().publishedReadTotal==0 && checked.Status().unknown,"Read-pointer ACK loss replayed");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256,1);const auto frame=Frame(false,1);raw.Append(frame,0,128);raw.Append(frame,0,128);auto view=checked.FramedView();
            Require(view.writeTotal()==128 && checked.Status().frames==1,"Scan budget not bounded");Require(view.writeTotal()==256 && checked.Status().frames==2,"Next scan lost complete prefix");++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);std::vector<std::uint8_t> blob(12,0);blob[0]=0x40;blob[3]=12;blob[6]=7;raw.Append(blob,0,12);
            auto view=checked.FramedView();Require(view.writeTotal()==12 && checked.Status().frames==1,"Old topology blob frame rejected");view.publishReadTotal(12);checked.VerifyDrained();++cases;
        }
        {
            Raw raw(0);SourceMfmRing checked(raw.View(),256);SourceReadoutDriver driver;driver.verifyIdle=[] {};driver.start=[] {};driver.stop=[] {};driver.wait=[](std::chrono::milliseconds) {};
            SourceRingReadout readout({checked.FramedView()},driver,[](const std::uint8_t *,std::size_t) {},[](const std::string &) {},17,64,31);const auto frame=Frame(false,1);raw.Append(frame,0,37);
            Reject([&] {readout.Start();});Require(readout.Status().unknown && !readout.Status().startIssued,"Start accepted leftover partial frame");++cases;
        }
        std::cout<<"PASS "<<cases<<" MFM ring prefix/header/wrap/mux/raw-tail/UNKNOWN cases; no payload files\n";return 0;
    }catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
