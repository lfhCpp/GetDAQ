#ifndef GETDAQ_PULSERAMPLITUDESCAN_H
#define GETDAQ_PULSERAMPLITUDESCAN_H

#include "systemexperimentconfiguration.h"
#include <vector>

namespace GetDAQ {

struct PulserAmplitudeScanPoint {
    unsigned int index = 0;
    int voltageMv = 0;
    SystemExperimentArtifact artifact;
};

// Builds complete, immutable offline configuration points. The interval is
// ascending, inclusive, and must land exactly on stopMv. No hardware or DAQ
// operation is performed.
std::vector<PulserAmplitudeScanPoint> BuildPulserAmplitudeScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    int startMv, int stopMv, int stepMv);

} // namespace GetDAQ

#endif
