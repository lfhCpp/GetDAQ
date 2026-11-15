#ifndef GETDAQ_SOURCEMFMRING_H
#define GETDAQ_SOURCEMFMRING_H
#include "sourceringreader.h"
namespace GetDAQ {
struct SourceMfmRingStatus {
    bool unknown=false;
    std::string error;
    std::uint64_t rawWriteTotal=0,completeWriteTotal=0,publishedReadTotal=0,frames=0;
};
// Converts arbitrary CPU-visible DMA prefixes into checked whole MFM boundaries.
// Borrows the original memory; only up to 28 header bytes are copied per frame.
class SourceMfmRing {
public:
    SourceMfmRing(const SourceRing &raw,std::size_t maximumFrame,std::size_t scanFrames=1024);
    ~SourceMfmRing();
    SourceMfmRing(const SourceMfmRing &)=delete;
    SourceMfmRing &operator=(const SourceMfmRing &)=delete;
    SourceRing FramedView(); // Keep this object alive while the view/readout can call it.
    SourceMfmRingStatus Status() const;
    void VerifyDrained(); // Device idle; reject partial header/body or unconsumed complete bytes.
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
