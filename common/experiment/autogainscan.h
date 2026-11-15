#ifndef GETDAQ_AUTOGAINSCAN_H
#define GETDAQ_AUTOGAINSCAN_H

#include "systemexperimentconfiguration.h"
#include <vector>

namespace GetDAQ {

struct AutoGainScanPoint {
    unsigned int index = 0;
    unsigned int channelId = 0;
    int voltageMv = 0;
    SystemExperimentArtifact artifact;
};

// Builds ordered channel x Pulser-amplitude points for the legacy AutoGain
// configuration sweep. Every point is a complete offline system artifact;
// this function never contacts ECC or performs acquisition.
std::vector<AutoGainScanPoint> BuildAutoGainScan(
    const SystemExperimentArtifact &baseline, const std::string &coboId,
    unsigned int asadId, unsigned int agetId,
    unsigned int firstChannel, unsigned int lastChannel,
    int startMv, int stopMv, int stepMv);

} // namespace GetDAQ

#endif
