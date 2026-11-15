#ifndef GETDAQ_SYSTEMSCANPLAN_H
#define GETDAQ_SYSTEMSCANPLAN_H

#include <functional>
#include <string>

namespace GetDAQ {

struct SystemScanPlanSummary {
    std::string kind;
    std::string targetCoBo;
    unsigned int pointCount = 0;
};

// Validates one immutable offline scan plan and every referenced complete
// system artifact. The loader receives only a validated leaf filename and
// must reject missing, non-regular, linked, or oversized files.
SystemScanPlanSummary ValidateSystemScanPlan(
    const std::string &manifestBytes,
    const std::function<std::string(const std::string &)> &loadArtifact);

}

#endif
