#ifndef GETDAQ_MFMHEADER_H
#define GETDAQ_MFMHEADER_H
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
namespace GetDAQ {
struct FrameInfo {
    std::size_t bytes = 0;
    unsigned type = 0, revision = 0, source = 0;
    unsigned cobo = 0, asad = 0;
    uint32_t event = 0;
    bool hasEvent = false, littleEndian = false;
    bool coboFrame = false, mutantFrame = false;
};
inline std::uint64_t MfmHeaderInteger(const std::uint8_t *data,unsigned size,bool little) {
    std::uint64_t value=0;
    for(unsigned i=0;i<size;++i)value=(value<<8)|data[little ? size-i-1 : i];
    return value;
}
inline std::size_t MfmFrameBytes(const std::uint8_t *data,std::size_t available) {
    if(!data || available<8)throw std::runtime_error("Invalid MFM primary header/size");
    const auto length=MfmHeaderInteger(data+1,3,(data[0]&0x80)!=0)*(std::uint64_t(1)<<(data[0]&15));
    if(length>std::numeric_limits<std::size_t>::max())throw std::runtime_error("MFM frame size exceeds platform range");
    return std::size_t(length);
}
// Prefix may be a 28-byte circular-header copy. Payload bytes are never read.
// frameBytes is the whole frame length, not the size of the prefix buffer.
inline FrameInfo InspectMfmHeader(const std::uint8_t *p,std::size_t prefixBytes,std::size_t frameBytes) {
    if(prefixBytes<8 || MfmFrameBytes(p,prefixBytes)!=frameBytes)throw std::runtime_error("Invalid MFM primary header/size");
    FrameInfo info;info.bytes=frameBytes;info.littleEndian=(p[0]&0x80)!=0;
    info.type=unsigned(MfmHeaderInteger(p+5,2,info.littleEndian));info.revision=p[7];info.source=p[4];
    if((p[0]&0x40)==0) {
        if(frameBytes<16 || prefixBytes<16)throw std::runtime_error("Truncated MFM standard header");
        const auto header=MfmHeaderInteger(p+8,2,info.littleEndian)*(std::uint64_t(1)<<(p[0]&15));
        const auto item=MfmHeaderInteger(p+10,2,info.littleEndian),count=MfmHeaderInteger(p+12,4,info.littleEndian);
        if(header<16 || header>frameBytes || item*count>frameBytes-header)throw std::runtime_error("Invalid MFM header/item bounds");
        if((info.type==1 && info.revision<=5) || (info.type==2 && info.revision==5)) {
            if(header<28)throw std::runtime_error("CoBo header does not contain source/event fields");
            if(prefixBytes<28)throw std::runtime_error("Truncated CoBo source/event header");
            info.coboFrame=true;info.hasEvent=true;info.event=std::uint32_t(MfmHeaderInteger(p+22,4,info.littleEndian));info.cobo=p[26];info.asad=p[27];
            if(info.asad>3)throw std::runtime_error("Invalid AsAd identifier");
        }
    }
    if(info.type==8 && info.revision==0 && frameBytes>=18) {
        if(prefixBytes<18)throw std::runtime_error("Truncated MuTaNT event header");
        info.mutantFrame=true;info.hasEvent=true;info.event=std::uint32_t(MfmHeaderInteger(p+14,4,info.littleEndian));
    }
    return info;
}
}
#endif
