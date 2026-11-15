#ifndef GETDAQ_SYSTEMACQUISITION_PREFLIGHT_H
#define GETDAQ_SYSTEMACQUISITION_PREFLIGHT_H

#include "acquisitionplans.h"
#include "datalinksetclient.h"
#include "systempreparation.h"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace GetDAQ {

using SystemAcquisitionPreflightExecutor = std::function<void(
    HardwareClient &, const std::vector<HardwareStep> &, const std::string &, const char *)>;
using SystemAcquisitionPreflightRecorder = std::function<void(const std::string &)>;

void RunSystemAcquisitionPreflight(
    const SystemPreparationOptions &options,
    const std::vector<CoBoAcquisitionPlan> &plans,
    const std::vector<std::unique_ptr<HardwareClient>> &cobos,
    const std::vector<std::unique_ptr<HardwareClient>> &mutants,
    DataLinkSetClient &links,
    const SystemAcquisitionPreflightExecutor &execute,
    const SystemAcquisitionPreflightRecorder &record);

}

#endif
