#include "sourcemfmring.h"
#include "../datareceiver/mfmheader.h"
#include <algorithm>
#include <array>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
class SourceMfmRing::Impl {
public:
    SourceRing raw;
    std::size_t maximum,budget;
    mutable std::mutex mutex;
    SourceMfmRingStatus status;
    Impl(const SourceRing &ring,std::size_t frame,std::size_t count):raw(ring),maximum(frame),budget(count) {
        if(raw.id>=8 || !raw.memory || raw.capacity<8 || !raw.writeTotal || !raw.publishReadTotal ||
           maximum<8 || maximum>raw.capacity || maximum>1024u*1024u*1024u || !budget || budget>65536)
            throw std::invalid_argument("MFM source ring requires valid memory/callbacks, 8B..min(capacity,1GiB) frame bound and 1..65536 scan budget");
        status.rawWriteTotal=status.completeWriteTotal=status.publishedReadTotal=raw.initialReadTotal;
    }
    void Known() const {if(status.unknown)throw std::logic_error("MFM source ring UNKNOWN; no reset/replay");}
    void Fail(const std::string &error) {status.unknown=true;if(status.error.empty())status.error=error;}
    void Copy(std::uint64_t total,std::uint8_t *target,std::size_t bytes) const {
        auto offset=std::size_t(total%raw.capacity);
        for(std::size_t i=0;i<bytes;++i) {target[i]=raw.memory[offset];if(++offset==raw.capacity)offset=0;}
    }
    void Scan() {
        Known();const auto write=raw.writeTotal();
        if(write<status.rawWriteTotal || write<status.completeWriteTotal || write<status.publishedReadTotal)
            throw std::runtime_error("MFM source producer counter regressed/reset");
        status.rawWriteTotal=write;
        if(write-status.publishedReadTotal>raw.capacity)throw std::runtime_error("MFM source ring raw overrun; partial bytes cannot be ignored");
        std::array<std::uint8_t,28> header;
        for(std::size_t frame=0;frame<budget;++frame) {
            const auto available=write-status.completeWriteTotal;if(available<8)return;
            Copy(status.completeWriteTotal,header.data(),8);const auto bytes=MfmFrameBytes(header.data(),8);
            if(bytes<8 || bytes>maximum)throw std::runtime_error("MFM source frame exceeds configured ring/size bounds");
            const auto prefix=std::min(bytes,header.size());if(available<prefix)return;
            Copy(status.completeWriteTotal,header.data(),prefix);InspectMfmHeader(header.data(),prefix,bytes);
            if(available<bytes)return;
            status.completeWriteTotal+=bytes;++status.frames;
        }
    }
    std::uint64_t Observe() {
        std::lock_guard<std::mutex> lock(mutex);
        try {Scan();return status.completeWriteTotal;}
        catch(const std::exception &e) {Fail(e.what());throw;}catch(...) {Fail("MFM ring observation raised a nonstandard exception");throw;}
    }
    void Publish(std::uint64_t total) {
        std::lock_guard<std::mutex> lock(mutex);Known();
        try {
            if(total<status.publishedReadTotal || total>status.completeWriteTotal)throw std::logic_error("MFM ring publish outside checked complete range");
            raw.publishReadTotal(total);status.publishedReadTotal=total;
        }catch(const std::exception &e) {Fail(e.what());throw;}catch(...) {Fail("MFM ring publish raised a nonstandard exception");throw;}
    }
};
SourceMfmRing::SourceMfmRing(const SourceRing &raw,std::size_t maximum,std::size_t budget):fImpl(new Impl(raw,maximum,budget)) {}
SourceMfmRing::~SourceMfmRing() {}
SourceRing SourceMfmRing::FramedView() {
    auto result=fImpl->raw;result.writeEndsAtMessageBoundary=true;
    result.writeTotal=[this] {return fImpl->Observe();};result.publishReadTotal=[this](std::uint64_t total) {fImpl->Publish(total);};
    result.verifyDrained=[this] {VerifyDrained();};return result;
}
SourceMfmRingStatus SourceMfmRing::Status() const {std::lock_guard<std::mutex> lock(fImpl->mutex);return fImpl->status;}
void SourceMfmRing::VerifyDrained() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);
    try {
        p.Scan();
        if(p.status.rawWriteTotal!=p.status.completeWriteTotal)throw std::runtime_error("MFM source has unvalidated or partial frame bytes; source End is forbidden");
        if(p.status.publishedReadTotal!=p.status.completeWriteTotal)throw std::runtime_error("MFM source complete frames are not drained/published");
        if(p.raw.verifyDrained)p.raw.verifyDrained();
    }catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("MFM drain check raised a nonstandard exception");throw;}
}
}
