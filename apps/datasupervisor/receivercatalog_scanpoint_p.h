#ifndef GETDAQ_RECEIVERCATALOG_SCANPOINT_P_H
#define GETDAQ_RECEIVERCATALOG_SCANPOINT_P_H

#include "supervisorcommon.h"
#include "systemexperimentconfiguration.h"

#include <cstddef>
#include <string>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {

struct BoundScanPointInput {
    JsonValue binding;
    std::string catalogPath;
    SystemExperimentArtifact artifact;
    std::size_t selectedCoboIndex;
};

BoundScanPointInput LoadBoundScanPointInput(
    const std::string &scanPath, const std::string &bindingPath,
    unsigned int pointIndex, const JsonValue &point, const std::string &targetCobo,
    const std::string &outputPath, const std::string &analysisName,
    const std::string &missingCoboMessage);

}
}

#endif
