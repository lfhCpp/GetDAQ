#include "../common/experiment/scaneventcountmonitor.h"

#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char *message)
{
    if (!condition) throw std::runtime_error(message);
}

GetDAQ::ScanEventCountBaseline Baseline()
{
    GetDAQ::ScanEventCountBaseline value;
    value.instanceId = "ecc-instance";
    value.snapshotSha256 = "snapshot";
    value.dataBindingSha256 = "data-binding";
    value.agentId = "agent-0";
    value.planSha256 = "plan";
    value.receiverInstanceId = "receiver";
    value.sourceId = "C0";
    value.sessionId = "session";
    value.pid = 1234;
    value.available = true;
    value.acquisitionEvents = 500;
    value.observedAtUnixMs = 1000;
    return value;
}
}

int main()
{
    using namespace GetDAQ;
    try {
        ScanEventCountMonitor monitor(Baseline(), 10, 1000);
        auto sample = Baseline();
        sample.observedAtUnixMs = 1100;
        sample.acquisitionEvents = 504;
        auto result = monitor.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::Continue &&
                result.eventsSinceBaseline == 4,
                "Monitor did not continue below the event threshold");
        sample.observedAtUnixMs = 1200;
        sample.acquisitionEvents = 510;
        result = monitor.Observe(sample, 200);
        Require(result.decision == ScanEventCountDecision::StopAtEventCount &&
                result.eventsSinceBaseline == 10 && monitor.Finished(),
                "Monitor did not stop at the requested event count");

        ScanEventCountMonitor timed(Baseline(), 10, 1000);
        sample = Baseline();
        sample.observedAtUnixMs = 1100;
        result = timed.Observe(sample, 1000);
        Require(result.decision == ScanEventCountDecision::StopAtMaximumDuration &&
                result.reason.find("abort") != std::string::npos,
                "Timeout was not distinguished from successful point completion");

        ScanEventCountMonitor changedIdentity(Baseline(), 10, 1000);
        sample = Baseline();
        sample.observedAtUnixMs = 1100;
        sample.sessionId = "replacement-session";
        result = changedIdentity.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::InspectRequired,
                "Replacement receiver session was accepted");

        ScanEventCountMonitor regressed(Baseline(), 10, 1000);
        sample = Baseline();
        sample.observedAtUnixMs = 1100;
        sample.acquisitionEvents = 499;
        result = regressed.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::InspectRequired,
                "Regressing cumulative event counter was accepted");

        ScanEventCountMonitor stale(Baseline(), 10, 1000);
        sample = Baseline();
        result = stale.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::InspectRequired,
                "Stale event observation was accepted");

        ScanEventCountMonitor unavailable(Baseline(), 10, 1000);
        sample = Baseline();
        sample.observedAtUnixMs = 1100;
        sample.available = false;
        result = unavailable.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::InspectRequired,
                "Unavailable receiver counter was treated as zero events");

        ScanEventCountMonitor invalid(Baseline(), 10, 1000);
        sample = Baseline();
        sample.observedAtUnixMs = 1100;
        sample.acquisitionEvents = 510;
        result = invalid.Observe(sample, 100);
        Require(result.decision == ScanEventCountDecision::StopAtEventCount,
                "Monitor did not finish before rejecting later samples");
        sample.observedAtUnixMs = 1200;
        result = invalid.Observe(sample, 200);
        Require(result.decision == ScanEventCountDecision::InspectRequired,
                "A finished monitor accepted another observation");

        std::cout << "PASS event-count thresholds, timeout abort, identity binding, monotonicity and stale observations\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
