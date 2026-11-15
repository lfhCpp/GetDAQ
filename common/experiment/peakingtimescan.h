#ifndef GETDAQ_PEAKINGTIMESCAN_H
#define GETDAQ_PEAKINGTIMESCAN_H

#include "systemexperimentconfiguration.h"
#include <vector>

namespace GetDAQ {

struct PeakingTimeScanPoint {
    unsigned int index = 0;
    SystemExperimentArtifact artifact;
};

// Builds sixteen immutable offline configuration artifacts. No hardware access,
// acquisition control, or assumption about an event count is performed here.
std::vector<PeakingTimeScanPoint> BuildPeakingTimeScan(const SystemExperimentArtifact &baseline,
                                                      const std::string &coboId);
unsigned int PeakingTimeForScanIndex(unsigned int chipVersion,unsigned int index);

} // namespace GetDAQ

#endif
