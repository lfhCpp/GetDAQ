#ifndef GETDAQ_SOURCERINGREADOUT_H
#define GETDAQ_SOURCERINGREADOUT_H
#include "sourceringreader.h"
#include <chrono>
namespace GetDAQ {
struct SourceReadoutDriver {
    std::function<void()> verifyIdle,start,stop;
    // Read-only interrupt wait; must return within the supplied maximum even
    // when no interrupt arrives. No control, reset, or implicit hardware action.
    std::function<void(std::chrono::milliseconds)> wait;
};
struct SourceReadoutStatus {
    bool unknown=false,workerRunning=false,startIssued=false,stopIssued=false;
    bool deviceStopAcknowledged=false;
    std::string error;
    SourceRingStatus buffers;
};
// Source process only. Driver callbacks implement actual hardware lifecycle.
// SourceRingReader supplies the transport-neutral borrowed data path.
class SourceRingReadout {
public:
    SourceRingReadout(const std::vector<SourceRing> &,const SourceReadoutDriver &,
                     const SourceRingReader::Send &,const std::function<void(const std::string &)> &fault,
                     std::size_t chunkBytes,std::size_t batchBytes,std::size_t workBytes);
    ~SourceRingReadout(); // Cancel/join only local worker; no device command/drain/End.
    SourceRingReadout(const SourceRingReadout &)=delete;
    SourceRingReadout &operator=(const SourceRingReadout &)=delete;
    void VerifyIdle(); // Confirm local worker joined and actual driver idle.
    void Configure(unsigned int mask,bool alwaysFlush);
    void Start();
    void StopAndJoin(); // Device stop first, then join; one attempt per Start, no uncertain retry.
    void Drain(); // Device/worker idle; bounded forced drain, never produces source End itself.
    SourceReadoutStatus Status() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
