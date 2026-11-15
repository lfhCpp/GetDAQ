#include "receivercatalog_scanwaveformmetrics_p.h"

#include <stdexcept>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {
namespace {
void Require(bool condition,const char *message) {
    if (!condition) throw std::runtime_error(message);
}
}

BoundScanWaveformMetrics::BoundScanWaveformMetrics()
    : frames(0),channelFrames(0),missing(0),invalid(0),valid(0),
      meanPeak(0.0),meanPeaking(0.0),meanFwhm(0.0) {}

BoundScanWaveformAccumulator::BoundScanWaveformAccumulator(unsigned int coboNumber,unsigned int asad,
                                                           unsigned int aget,unsigned int channel)
    : fCoboNumber(coboNumber),fAsad(asad),fAget(aget),fChannel(channel) {}

void BoundScanWaveformAccumulator::ProcessFrame(const std::uint8_t *frame,const FrameInfo &info) {
    ProcessFrameAtDepth(frame,info,0);
}

const BoundScanWaveformMetrics &BoundScanWaveformAccumulator::Metrics() const {
    return fMetrics;
}

void BoundScanWaveformAccumulator::ProcessFrameAtDepth(const std::uint8_t *frame,const FrameInfo &info,
                                                        unsigned int depth) {
    if (info.type==0xff01u) {
        Require(info.revision==1,"Bound AutoGain analysis does not support this layered MFM revision");
        Require(depth<8 && (frame[0]&0x40u)==0,"Layered MFM nesting/header is outside supported bounds");
        const unsigned exponent=unsigned(frame[0]&15u);
        const std::uint64_t blockBytes=std::uint64_t(1)<<exponent;
        const std::uint64_t header=MfmHeaderInteger(frame+8,2,info.littleEndian)*blockBytes;
        const std::uint64_t itemBytes=MfmHeaderInteger(frame+10,2,info.littleEndian);
        const std::uint64_t itemCount=MfmHeaderInteger(frame+12,4,info.littleEndian);
        Require(header>=16 && header<=info.bytes && itemBytes==0 && itemCount<=2000000,
            "Layered MFM header/item layout is invalid");
        std::size_t child=static_cast<std::size_t>(header);
        for (std::uint64_t childIndex=0;childIndex<itemCount;++childIndex) {
            Require(child<=info.bytes && info.bytes-child>=8,"Layered MFM child header is truncated");
            const std::size_t childBytes=MfmFrameBytes(frame+child,info.bytes-child);
            Require(childBytes>=8 && childBytes<=info.bytes-child,"Layered MFM child frame exceeds its parent");
            const auto childInfo=InspectMfmHeader(frame+child,childBytes,childBytes);
            ProcessFrameAtDepth(frame+child,childInfo,depth+1);
            child+=childBytes;
        }
        Require(child<=info.bytes && info.bytes-child<blockBytes,
            "Layered MFM payload length differs from its child count");
        return;
    }
    Require(info.type!=1 || info.revision<=5,
        "Bound AutoGain waveform analysis does not support this CoBo type-1 revision");
    if (!info.coboFrame || info.cobo!=fCoboNumber || info.asad!=fAsad) return;
    Require((frame[0]&0x40u)==0,"Bound AutoGain analysis requires standard MFM headers");
    const unsigned exponent=unsigned(frame[0]&15u);
    const std::uint64_t header=MfmHeaderInteger(frame+8,2,info.littleEndian)*(std::uint64_t(1)<<exponent);
    const std::uint64_t itemBytes=MfmHeaderInteger(frame+10,2,info.littleEndian);
    const std::uint64_t itemCount=MfmHeaderInteger(frame+12,4,info.littleEndian);
    Require(header>=28 && header<=info.bytes,"Bound AutoGain CoBo header layout is invalid");
    ++fMetrics.frames;
    bool found[512]={}; unsigned short values[512]={}; unsigned samples=0;
    const auto *item=frame+header;
    if (info.type==1 && info.revision<=5) {
        Require(itemBytes==4 && itemCount<=(info.bytes-header)/4,
            "Bound AutoGain CoBo type-1 sample layout is invalid");
        for (std::uint64_t n=0;n<itemCount;++n,item+=4) {
            const std::uint32_t word=(std::uint32_t(item[0])<<24)|(std::uint32_t(item[1])<<16)|
                (std::uint32_t(item[2])<<8)|item[3];
            const unsigned itemAget=(word>>30)&3u,itemChannel=(word>>23)&0x7fu,bucket=(word>>14)&0x1ffu;
            Require(itemChannel<68,"Bound AutoGain waveform contains invalid channel index");
            if (itemAget!=fAget || itemChannel!=fChannel) continue;
            Require(!found[bucket],"Bound AutoGain waveform contains duplicate bucket");
            found[bucket]=true; values[bucket]=static_cast<unsigned short>(word&0xfffu); ++samples;
        }
    } else if (info.type==2 && info.revision==5) {
        const std::uint64_t expectedItems=std::uint64_t(4)*68*512;
        Require(itemBytes==2 && itemCount==expectedItems && itemCount<=(info.bytes-header)/2,
            "Bound AutoGain compact CoBo type-2 frame is incomplete or has an invalid layout");
        unsigned channelIndex[4]={},bucketIndex[4]={}; std::uint64_t counts[4]={};
        for (std::uint64_t n=0;n<itemCount;++n,item+=2) {
            const std::uint16_t word=static_cast<std::uint16_t>((std::uint16_t(item[0])<<8)|item[1]);
            const unsigned itemAget=(word>>14)&3u;
            if (channelIndex[itemAget]>=68) { channelIndex[itemAget]=0; ++bucketIndex[itemAget]; }
            Require(bucketIndex[itemAget]<512,"Bound AutoGain compact CoBo frame exceeds 512 buckets");
            if (itemAget==fAget && channelIndex[itemAget]==fChannel) {
                const unsigned bucket=bucketIndex[itemAget];
                Require(!found[bucket],"Bound AutoGain compact waveform repeats a sample position");
                found[bucket]=true; values[bucket]=static_cast<unsigned short>(word&0xfffu); ++samples;
            }
            ++counts[itemAget]; ++channelIndex[itemAget];
        }
        for (unsigned itemAget=0;itemAget<4;++itemAget)
            Require(counts[itemAget]==std::uint64_t(68)*512,
                "Bound AutoGain compact frame does not contain four complete AGET readouts");
        Require(samples==512,"Bound AutoGain compact frame does not contain a complete selected channel");
    } else {
        throw std::runtime_error(
            "Bound AutoGain waveform analysis supports CoBo type-1 revision 0..5 and compact type-2 revision 5 frames");
    }
    if (!samples) { ++fMetrics.missing; return; }
    ++fMetrics.channelFrames;
    unsigned last=512,peakBucket=512; unsigned short peak=0;
    for (unsigned b=0;b<512;++b) if (found[b]) {
        if (peakBucket==512 || values[b]>peak) { peak=values[b]; peakBucket=b; }
        last=b;
    }
    unsigned peaking=0; bool hasPeaking=false,hasHalfLeft=false,hasHalfRight=false; unsigned left=512,right=512;
    const unsigned peakLevel=unsigned(0.05*double(peak)),halfLevel=unsigned(0.5*double(peak));
    for (unsigned b=0;b<peakBucket;++b) if (found[b]) {
        if (!hasPeaking && values[b]>peakLevel) { peaking=peakBucket-b; hasPeaking=true; }
        if (!hasHalfLeft && values[b]>=halfLevel) { left=b; hasHalfLeft=true; }
    }
    for (unsigned b=peakBucket+1;b<=last;++b) if (found[b] && values[b]<halfLevel) {
        if (hasHalfLeft) { right=b; hasHalfRight=true; }
        break;
    }
    if (!peak || !hasPeaking || !hasHalfLeft || !hasHalfRight) { ++fMetrics.invalid; return; }
    ++fMetrics.valid; fMetrics.meanPeak+=peak; fMetrics.meanPeaking+=peaking; fMetrics.meanFwhm+=right-left;
}

}
}
