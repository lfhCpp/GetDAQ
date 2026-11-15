#include "icesourceclient.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
namespace {
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
std::vector<std::uint8_t> Frame(unsigned int event,bool little) {
    std::vector<std::uint8_t> bytes(4096,std::uint8_t(event));bytes[0]=std::uint8_t(6|(little ? 128 : 0));bytes[4]=7;bytes[7]=4;
    const auto put=[&](unsigned int offset,std::uint64_t value,unsigned int count) {for(unsigned int i=0;i<count;++i) bytes[offset+i]=std::uint8_t(value>>(little ? 8*i : 8*(count-i-1)));};
    put(1,64,3);put(5,1,2);put(8,2,2);put(10,4,2);put(12,992,4);put(22,event,4);bytes[26]=bytes[27]=0;
    for(unsigned int i=128;i<136;++i) bytes[i]=i<132 ? 0 : 255;
    return bytes;
}
void Status(const GetDAQ::IceSourceSnapshot &s) {
    std::cout<<"{\"state\":"<<int(s.state)<<",\"issued_bytes\":"<<s.issuedBytes<<",\"locally_sent_bytes\":"<<s.locallySentBytes
        <<",\"issued_chunks\":"<<s.issuedChunks<<",\"finish_issued\":"<<(s.finishIssued ? "true" : "false")
        <<",\"source_end_acknowledged\":"<<(s.sourceEndAcknowledged ? "true" : "false")<<"}\n";
}
}
int main(int argc,char **argv) {
    try {
        Require(argc==5,"Pass PORT ENCODING MODE READY_FILE");GetDAQ::IceSourceOptions options;options.port=unsigned(std::stoul(argv[1]));options.encodingMinor=unsigned(std::stoul(argv[2]));options.timeoutMs=1000;options.chunkBytes=317;
        const std::string mode=argv[3];if(mode=="wrong-session") options.expectedSession=std::string(64,'b');
        else options.expectedSession=std::string(64,'d');
        if(mode=="discover") options.expectedSession.clear();
        GetDAQ::IceSourceClient client(options);
        if(mode=="wrong-session" || mode=="legacy-peer") {
            bool rejected=false;try {client.Connect();} catch(const std::exception &) {rejected=true;}
            Require(rejected && client.Status().state==GetDAQ::IceSourceSnapshot::Unknown && !client.Status().issuedBytes,"Source preflight sent bytes or accepted wrong session/legacy extension");
            bool repeat=false;try {client.Connect();} catch(const std::logic_error &) {repeat=true;}Require(repeat,"UNKNOWN source reconnected");Status(client.Status());return 0;
        }
        client.Connect();Require(client.Status().sessionId==std::string(64,'d'),"Source session discovery differs");
        if(mode!="empty") {const auto first=Frame(1,false),second=Frame(2,true);client.Send(first.data(),first.size());client.Send(second.data(),second.size());}
        const auto before=client.Status();Require(before.locallySentBytes==(mode=="empty" ? 0 : 8192),"Source local byte count differs");
        if(mode=="destroy") {Status(before);return 0;}
        if(mode=="send-loss" || mode=="finish-loss") {
            std::ofstream ready(argv[4]);ready<<"ready\n";ready.close();
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(6);
            while(!std::ifstream(std::string(argv[4])+".continue")) {
                Require(std::chrono::steady_clock::now()<deadline,"Source fault fixture has no replacement-ready signal");
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            bool failed=false;
            try {
                if(mode=="send-loss") {const auto next=Frame(3,false);client.Send(next.data(),next.size());}
                // A oneway return may only be local handoff before close is seen.
                client.Finish();
            } catch(const std::exception &) {failed=true;}
            Require(failed && client.Status().state==GetDAQ::IceSourceSnapshot::Unknown,"Failed source call reconnected or fabricated known outcome");
            bool send=false,end=false;try {client.Send(nullptr,0);} catch(const std::logic_error &) {send=true;}
            try {client.Finish();} catch(const std::logic_error &) {end=true;}Require(send && end,"UNKNOWN allowed retry/end");Status(client.Status());return 0;
        }
        Require(mode=="normal" || mode=="discover" || mode=="empty","Unknown source client test mode");
        client.Finish();const auto done=client.Status();Require(done.state==GetDAQ::IceSourceSnapshot::Finished && done.sourceEndAcknowledged && done.finishIssued,"Source did not record actual end ACK");
        bool repeat=false;try {client.Finish();} catch(const std::logic_error &) {repeat=true;}Require(repeat,"Completed source resent end");Status(done);return 0;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
