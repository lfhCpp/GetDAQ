#ifndef GETDAQ_SCANEXECUTIONPOLICY_H
#define GETDAQ_SCANEXECUTIONPOLICY_H

#include "systemscanplan.h"

#include <string>

namespace GetDAQ {

enum class ScanStopStrategy {
    EventCount
};

// Operator-supplied, finite event-count stopping conditions for one immutable
// scan plan. Threshold plans are deliberately ineligible until their trigger,
// pulser and result semantics have been reviewed. This metadata never starts,
// configures, or stops hardware.
class ScanExecutionPolicy {
public:
    static const unsigned int MaximumEventsPerPoint = 1000000000U;
    static const unsigned int MaximumEventRunDurationMs = 86400000U;

    static ScanExecutionPolicy ForEventCount(const std::string &planBytes,
                                              const SystemScanPlanSummary &plan,
                                              unsigned int eventsPerPoint,
                                              unsigned int maximumDurationMsPerPoint);
    static ScanExecutionPolicy Decode(const std::string &policyBytes,
                                      const std::string &planBytes,
                                      const SystemScanPlanSummary &validatedPlan);

    std::string Encode() const;
    const std::string &SourcePlanSha256() const { return fSourcePlanSha256; }
    const std::string &Kind() const { return fKind; }
    const std::string &TargetCoBo() const { return fTargetCoBo; }
    unsigned int PointCount() const { return fPointCount; }
    ScanStopStrategy Strategy() const { return fStrategy; }
    unsigned int EventsPerPoint() const { return fEventsPerPoint; }
    unsigned int MaximumDurationMsPerPoint() const { return fMaximumDurationMsPerPoint; }

private:
    ScanExecutionPolicy(const std::string &planBytes,
                        const SystemScanPlanSummary &plan,
                        unsigned int eventsPerPoint,
                        unsigned int maximumDurationMsPerPoint);

    std::string fSourcePlanSha256;
    std::string fKind;
    std::string fTargetCoBo;
    unsigned int fPointCount = 0;
    ScanStopStrategy fStrategy = ScanStopStrategy::EventCount;
    unsigned int fEventsPerPoint = 0;
    unsigned int fMaximumDurationMsPerPoint = 0;
};

}

#endif
