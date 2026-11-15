#ifndef GETDAQ_THRESHOLDSCAN_H
#define GETDAQ_THRESHOLDSCAN_H

#include "systemexperimentconfiguration.h"
#include <vector>

namespace GetDAQ {

struct ThresholdScanPoint {
    unsigned int index = 0;
    unsigned int thresholdCode = 0; // legacy packed code: 3-bit global + 4-bit channel threshold.
    SystemExperimentArtifact artifact;
};

// Builds immutable offline artifacts for one explicitly selected active AGET.
// Does not interpret hit patterns, collect events, or operate hardware.
std::vector<ThresholdScanPoint> BuildThresholdScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    unsigned int asadId, unsigned int agetId, unsigned int startCode,
    unsigned int stopCode, unsigned int stepCode);

} // namespace GetDAQ

#endif
