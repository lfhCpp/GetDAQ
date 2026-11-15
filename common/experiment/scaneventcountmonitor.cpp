#include "scaneventcountmonitor.h"

#include <stdexcept>

namespace GetDAQ {
namespace {

void Require(bool condition, const char *message)
{
    if (!condition) throw std::invalid_argument(message);
}

bool SameIdentity(const ScanEventCountBaseline &left,
                  const ScanEventCountBaseline &right)
{
    return left.instanceId == right.instanceId &&
           left.snapshotSha256 == right.snapshotSha256 &&
           left.dataBindingSha256 == right.dataBindingSha256 &&
           left.agentId == right.agentId && left.planSha256 == right.planSha256 &&
           left.receiverInstanceId == right.receiverInstanceId &&
           left.sourceId == right.sourceId && left.sessionId == right.sessionId &&
           left.pid == right.pid;
}

}

ScanEventCountMonitor::ScanEventCountMonitor(
    const ScanEventCountBaseline &baseline, std::uint64_t eventsPerPoint,
    std::uint64_t maximumDurationMs)
    : fBaseline(baseline), fEventsPerPoint(eventsPerPoint),
      fMaximumDurationMs(maximumDurationMs),
      fLastObservedAtUnixMs(baseline.observedAtUnixMs), fFinished(false)
{
    Require(!baseline.instanceId.empty() && !baseline.snapshotSha256.empty() &&
            !baseline.dataBindingSha256.empty() && !baseline.agentId.empty() &&
            !baseline.planSha256.empty() && !baseline.receiverInstanceId.empty() &&
            !baseline.sourceId.empty() && !baseline.sessionId.empty() &&
            baseline.pid > 0 && baseline.available && baseline.observedAtUnixMs > 0,
            "Event-count baseline requires a complete receiver identity");
    Require(eventsPerPoint > 0 && eventsPerPoint <= 1000000000ULL,
            "Events per point must be within 1..1000000000");
    Require(maximumDurationMs > 0 && maximumDurationMs <= 86400000ULL,
            "Maximum event-run duration must be within 1..86400000 milliseconds");
}

ScanEventCountResult ScanEventCountMonitor::Observe(
    const ScanEventCountBaseline &observation, std::uint64_t elapsedMs)
{
    ScanEventCountResult result;
    if (fFinished) {
        result.decision = ScanEventCountDecision::InspectRequired;
        result.reason = "Event-count monitor is already finished";
        return result;
    }
    if (!SameIdentity(fBaseline, observation)) {
        fFinished = true;
        result.decision = ScanEventCountDecision::InspectRequired;
        result.reason = "Receiver/CoBo identity changed during the Run";
        return result;
    }
    if (!observation.available) {
        fFinished = true;
        result.decision = ScanEventCountDecision::InspectRequired;
        result.reason = "Receiver event counter is unavailable";
        return result;
    }
    if (!observation.observedAtUnixMs ||
        observation.observedAtUnixMs <= fLastObservedAtUnixMs) {
        fFinished = true;
        result.decision = ScanEventCountDecision::InspectRequired;
        result.reason = "Receiver event observation is stale or out of order";
        return result;
    }
    fLastObservedAtUnixMs = observation.observedAtUnixMs;
    if (observation.acquisitionEvents < fBaseline.acquisitionEvents) {
        fFinished = true;
        result.decision = ScanEventCountDecision::InspectRequired;
        result.reason = "Receiver cumulative event counter regressed";
        return result;
    }

    result.eventsSinceBaseline = observation.acquisitionEvents - fBaseline.acquisitionEvents;
    if (result.eventsSinceBaseline >= fEventsPerPoint) {
        fFinished = true;
        result.decision = ScanEventCountDecision::StopAtEventCount;
        result.reason = "Configured event count reached";
        return result;
    }
    if (elapsedMs >= fMaximumDurationMs) {
        fFinished = true;
        result.decision = ScanEventCountDecision::StopAtMaximumDuration;
        result.reason = "Maximum Run duration reached; this is an abort, not a completed point";
        return result;
    }
    return result;
}

}
