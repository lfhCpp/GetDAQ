#include "sourceringreader.h"
#include <algorithm>
#include <limits>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
class SourceRingReader::Impl {
public:
    std::vector<SourceRing> rings;
    Send send;
    std::function<void(const std::string &)> fault;
    std::size_t chunk,batch,next=0,active=0;
    bool hasActive=false;
    std::uint64_t activeEnd=0;
    mutable std::mutex mutex;
    SourceRingStatus status;
    Impl(const std::vector<SourceRing> &input,const Send &sender,
         const std::function<void(const std::string &)> &report,std::size_t c,std::size_t b)
        :rings(input),send(sender),fault(report),chunk(c),batch(b) {
        if(rings.empty() || rings.size()>8 || !send || !fault || !chunk || !batch)
            throw std::invalid_argument("Source reader requires 1..8 rings, callbacks and nonzero chunk/batch sizes");
        for(const auto &ring:rings) {
            if(ring.id>=8 || (status.enabledMask&(1u<<ring.id)) || !ring.memory || !ring.capacity ||
               !ring.writeTotal || !ring.publishReadTotal || batch>ring.capacity ||
               (rings.size()>1 && !ring.writeEndsAtMessageBoundary))
                throw std::invalid_argument("Invalid source ring, batch/capacity, or missing whole-message write-boundary guarantee for multiple rings");
            status.enabledMask|=1u<<ring.id;
            SourceRingProgress p;p.id=ring.id;p.observedWriteTotal=p.handedTotal=p.publishedTotal=ring.initialReadTotal;
            status.rings.push_back(p);
        }
    }
    void Known() const {if(status.unknown) throw std::logic_error("Source ring outcome UNKNOWN; no retry or pointer replay");}
    void Fail(const std::string &message) {
        status.unknown=true;if(status.error.empty())status.error=message;
        // Report callback must only latch source failure; never call this reader.
        // Preserve the original failure even when reporting itself throws.
        try {fault(status.error);} catch(...) {}
    }
    std::uint64_t Observe(std::size_t index) {
        auto &p=status.rings[index];const auto &ring=rings[index];const auto write=ring.writeTotal();
        if(write<p.observedWriteTotal || write<p.handedTotal) throw std::runtime_error("Source producer counter regressed/reset");
        p.observedWriteTotal=write;
        if(write-p.handedTotal>ring.capacity) throw std::runtime_error("Source ring overrun: unread memory may have been overwritten");
        return write-p.handedTotal;
    }
};
SourceRingReader::SourceRingReader(const std::vector<SourceRing> &rings,const Send &send,
                                 const std::function<void(const std::string &)> &fault,std::size_t chunk,std::size_t batch)
    :fImpl(new Impl(rings,send,fault,chunk,batch)) {}
SourceRingReader::~SourceRingReader() {}
SourceRingStatus SourceRingReader::Status() const {std::lock_guard<std::mutex> lock(fImpl->mutex);return fImpl->status;}
void SourceRingReader::Configure(unsigned int mask,bool flush) {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.Known();unsigned int available=0;
    for(const auto &ring:p.rings)available|=1u<<ring.id;
    if(!mask || (mask&~available))throw std::invalid_argument("Source ring mask selects absent buffers");
    if(mask!=p.status.enabledMask) {
        try {for(std::size_t i=0;i<p.rings.size();++i) {
            if(p.Observe(i))throw std::runtime_error("Cannot change source mask while any ring has unread bytes");
            if(p.rings[i].verifyDrained)p.rings[i].verifyDrained();
        }}
        catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Nonstandard source ring policy failure");throw;}
    }
    p.status.enabledMask=mask;p.status.alwaysFlush=flush;
}
std::size_t SourceRingReader::Flush(std::size_t budget,bool force) {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.Known();if(!budget)return 0;
    std::size_t sent=0,skipped=0;
    try {
        while(sent<budget && skipped<p.rings.size()) {
            std::size_t index=p.active;
            if(!p.hasActive) {
                index=p.next;p.next=(p.next+1)%p.rings.size();const auto &candidate=p.rings[index];
                if(!(p.status.enabledMask&(1u<<candidate.id))) {++skipped;continue;}
                const auto available=p.Observe(index);
                if(!available || (!force && !p.status.alwaysFlush && available<p.batch)) {++skipped;continue;}
                p.active=index;p.activeEnd=p.status.rings[index].observedWriteTotal;p.hasActive=true;
            } else p.Observe(index); // Check overrun even while retaining an unfinished selected message batch.
            const auto &ring=p.rings[index];auto &progress=p.status.rings[index];
            const auto available=p.activeEnd-progress.handedTotal;
            const auto offset=std::size_t(progress.handedTotal%ring.capacity);
            const auto count=std::min(std::min(std::size_t(available),ring.capacity-offset),std::min(p.chunk,budget-sent));
            p.send(ring.memory+offset,count); // Synchronous borrowed span; only local handoff is proved.
            progress.handedTotal+=count; // If publish throws, never send this span again.
            ring.publishReadTotal(progress.handedTotal);
            progress.publishedTotal=progress.handedTotal;
            sent+=count;skipped=0;
            if(progress.handedTotal==p.activeEnd)p.hasActive=false; // Switch rings only at a sampled whole-message boundary.
        }
        return sent;
    } catch(const std::exception &e) {p.Fail(e.what());throw;}
    catch(...) {p.Fail("Nonstandard source ring read/send/publish failure");throw;}
}
bool SourceRingReader::Empty() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.Known();bool empty=true;
    try {for(std::size_t i=0;i<p.rings.size();++i)if(p.status.enabledMask&(1u<<p.rings[i].id))empty=p.Observe(i)==0 && empty;return empty;}
    catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Nonstandard source ring observation failure");throw;}
}
void SourceRingReader::VerifyDrained() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.Known();
    try {
        for(std::size_t i=0;i<p.rings.size();++i)if(p.status.enabledMask&(1u<<p.rings[i].id)) {
            if(p.Observe(i))throw std::runtime_error("Source ring has unconsumed complete bytes");
            if(p.rings[i].verifyDrained)p.rings[i].verifyDrained();
        }
    }catch(const std::exception &e) {p.Fail(e.what());throw;}catch(...) {p.Fail("Source drain validation raised a nonstandard exception");throw;}
}
namespace {
std::size_t RingOffset(std::uintptr_t start,std::size_t capacity,std::uintptr_t address) {
    if(address<start || address-start>capacity)throw std::invalid_argument("Legacy write address outside circular buffer");
    return address-start==capacity ? 0 : std::size_t(address-start);
}
}
LegacyRingWriteTracker::LegacyRingWriteTracker(std::uintptr_t start,std::size_t capacity,std::uintptr_t initial,
                                             std::uint64_t total,bool guaranteed)
    :fStart(start),fCapacity(capacity),fOffset(0),fTotal(total) {
    if(!capacity || !guaranteed || capacity>std::numeric_limits<std::uintptr_t>::max()-start)
        throw std::invalid_argument("Legacy address tracker requires non-wrapping range and explicit no-full-lap guarantee");
    fOffset=RingOffset(start,capacity,initial);
    if(total%capacity!=fOffset)throw std::invalid_argument("Legacy initial pointer and cumulative total disagree");
}
std::uint64_t LegacyRingWriteTracker::Observe(std::uintptr_t address) {
    if(fUnknown)throw std::logic_error("Legacy pointer tracker is UNKNOWN; no reset/replay");
    try {
        const auto offset=RingOffset(fStart,fCapacity,address);
        const auto delta=offset>=fOffset ? offset-fOffset : fCapacity-fOffset+offset;
        if(delta>std::numeric_limits<std::uint64_t>::max()-fTotal)throw std::overflow_error("Legacy producer counter overflow");
        fTotal+=delta;fOffset=offset;return fTotal;
    } catch(...) {fUnknown=true;throw;}
}
}
