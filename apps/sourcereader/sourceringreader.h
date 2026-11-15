#ifndef GETDAQ_SOURCERINGREADER_H
#define GETDAQ_SOURCERINGREADER_H
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
// Memory belongs to the board/DMA adapter. It remains mapped until destruction.
// writeTotal includes complete, CPU-visible bytes, relative to memory[0].
// Unconsumed memory must never be overwritten while send borrows a span.
struct SourceRing {
    unsigned int id=0;
    const std::uint8_t *memory=nullptr;
    std::size_t capacity=0;
    std::uint64_t initialReadTotal=0;
    bool writeEndsAtMessageBoundary=false; // Required for >1 rings; each sampled writeTotal ends whole messages/frames.
    std::function<std::uint64_t()> writeTotal;
    std::function<void(std::uint64_t)> publishReadTotal;
    std::function<void()> verifyDrained; // Optional raw-tail validation after device idle, not just eligible complete bytes.
};
struct SourceRingProgress {
    unsigned int id=0;
    std::uint64_t observedWriteTotal=0,handedTotal=0,publishedTotal=0;
};
struct SourceRingStatus {
    bool unknown=false;
    std::string error;
    unsigned int enabledMask=0;
    bool alwaysFlush=true;
    std::vector<SourceRingProgress> rings;
};
class SourceRingReader {
public:
    using Send=std::function<void(const std::uint8_t *,std::size_t)>;
    SourceRingReader(const std::vector<SourceRing> &rings,const Send &send,
                     const std::function<void(const std::string &)> &fault,
                     std::size_t chunkBytes,std::size_t batchBytes);
    ~SourceRingReader(); // No implicit drain, retry, pointer write or source End.
    SourceRingReader(const SourceRingReader &)=delete;
    SourceRingReader &operator=(const SourceRingReader &)=delete;
    void Configure(unsigned int mask,bool alwaysFlush); // Caller must quiesce producers first.
    std::size_t Flush(std::size_t byteBudget,bool force=false); // Serialized single consumer; bounded work.
    bool Empty(); // Checks enabled rings; driver errors latch UNKNOWN.
    void VerifyDrained(); // Enabled rings empty plus optional raw-tail checks; faults latch UNKNOWN.
    SourceRingStatus Status() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
// For old hardware that exposes only an address. Caller must explicitly prove
// fewer than capacity bytes can be produced between observations, no full-ring
// ambiguity, and producer backpressure before overwriting unread data.
class LegacyRingWriteTracker {
public:
    LegacyRingWriteTracker(std::uintptr_t start,std::size_t capacity,
                           std::uintptr_t initialWrite,std::uint64_t initialTotal,
                           bool noFullLapGuaranteed);
    std::uint64_t Observe(std::uintptr_t address);
private:
    std::uintptr_t fStart;
    std::size_t fCapacity,fOffset;
    std::uint64_t fTotal;
    bool fUnknown=false;
};
}
#endif
