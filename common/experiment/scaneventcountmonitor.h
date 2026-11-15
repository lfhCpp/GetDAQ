#ifndef GETDAQ_SCANEVENTCOUNTMONITOR_H
#define GETDAQ_SCANEVENTCOUNTMONITOR_H

#include <cstdint>
#include <string>

namespace GetDAQ {

// Immutable identity and cumulative-counter baseline captured for one Run.
// The monitor is a decision component only: it performs no I/O and never
// submits a hardware command.
struct ScanEventCountBaseline {
    std::string instanceId;
    std::string snapshotSha256;
    std::string dataBindingSha256;
    std::string agentId;
    std::string planSha256;
    std::string receiverInstanceId;
    std::string sourceId;
    std::string sessionId;
    int pid = 0;
    bool available = false;
    std::uint64_t acquisitionEvents = 0;
    std::uint64_t observedAtUnixMs = 0;
};

enum class ScanEventCountDecision {
    Continue,
    StopAtEventCount,
    StopAtMaximumDuration,
    InspectRequired
};

struct ScanEventCountResult {
    ScanEventCountDecision decision = ScanEventCountDecision::Continue;
    std::uint64_t eventsSinceBaseline = 0;
    std::string reason;
};

class ScanEventCountMonitor {
public:
    ScanEventCountMonitor(const ScanEventCountBaseline &baseline,
                          std::uint64_t eventsPerPoint,
                          std::uint64_t maximumDurationMs);

    ScanEventCountResult Observe(const ScanEventCountBaseline &observation,
                                 std::uint64_t elapsedMs);
    bool Finished() const { return fFinished; }

private:
    ScanEventCountBaseline fBaseline;
    std::uint64_t fEventsPerPoint;
    std::uint64_t fMaximumDurationMs;
    std::uint64_t fLastObservedAtUnixMs;
    bool fFinished;
};

}

#endif
