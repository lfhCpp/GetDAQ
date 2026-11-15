#include "mfmstream.h"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>
using namespace GetDAQ;
namespace {
void Put(std::vector<uint8_t> &data,std::size_t at,std::uint64_t value,unsigned bytes) {
    for (unsigned i=0;i<bytes;++i) data[at+i]=uint8_t(value>>(8*(bytes-i-1)));
}
}
int main(int argc,char **argv) {
    try {
        if (argc!=4) throw std::invalid_argument("Pass total MiB, frame bytes and feed chunk bytes");
        const auto total=std::stoull(argv[1])*1024*1024,frame=std::stoull(argv[2]),chunk=std::stoull(argv[3]);
        if (!total || total>64ULL*1024*1024*1024 || frame<128 || frame>1024*1024 || frame%64 || !chunk || chunk>1024*1024 || total%frame)
            throw std::invalid_argument("Invalid bounded benchmark parameters");
        std::vector<uint8_t> bytes(8*1024*1024,0x5a);const auto usable=bytes.size()/frame*frame;
        for (std::size_t at=0;at<usable;at+=std::size_t(frame)) {
            bytes[at]=6;Put(bytes,at+1,frame/64,3);bytes[at+4]=1;Put(bytes,at+5,1,2);bytes[at+7]=4;
            Put(bytes,at+8,2,2);Put(bytes,at+10,4,2);Put(bytes,at+12,(frame-128)/4,4);Put(bytes,at+22,1,4);bytes[at+26]=0;bytes[at+27]=0;
        }
        std::uint64_t frames=0,borrowed=0,buffered=0,checksum=0;
        const std::uintptr_t begin=reinterpret_cast<std::uintptr_t>(bytes.data()),end=begin+std::uintptr_t(usable);
        MfmStream stream(1024*1024,[&](const uint8_t *p,const FrameInfo &info) {
            ++frames;checksum+=p[info.bytes-1]+info.event;
            const auto address=reinterpret_cast<std::uintptr_t>(p);
            if (address>=begin && address<end && info.bytes<=end-address) borrowed+=info.bytes;else buffered+=info.bytes;
        });
        const auto start=std::chrono::steady_clock::now();
        std::uint64_t fed=0;
        while (fed<total) {
            const auto amount=std::size_t(std::min<std::uint64_t>(usable,total-fed));
            for (std::size_t at=0;at<amount;at+=std::size_t(chunk)) stream.Feed(bytes.data()+at,std::min<std::size_t>(std::size_t(chunk),amount-at));
            fed+=amount;
        }
        stream.Finish();const auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
        if (frames!=total/frame || checksum!=frames*91 || borrowed+buffered!=total) throw std::runtime_error("Framer benchmark lost or altered frames");
        std::cout<<"{\"total_bytes\":"<<total<<",\"frame_bytes\":"<<frame<<",\"chunk_bytes\":"<<chunk<<",\"frames\":"<<frames
            <<",\"borrowed_frame_bytes\":"<<borrowed<<",\"buffered_frame_bytes\":"<<buffered<<",\"seconds\":"<<seconds
            <<",\"gib_per_second\":"<<(double(total)/1073741824.0/seconds)<<",\"scope\":\"memory-only parser; no network/disk/hardware claim\"}\n";
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
