#ifndef GETDAQ_SYSTEMPREPARATION_PLAN_P_H
#define GETDAQ_SYSTEMPREPARATION_PLAN_P_H

#include "systempreparation.h"
#include "acquisitionplans.h"

namespace GetDAQ {
namespace SystemPreparationInternal {

struct CompiledSystemPreparationPlan {
    SystemPreparationOptions options;
    std::vector<std::vector<HardwareStep> > configurePlans;
    std::vector<std::vector<HardwareStep> > mutantPreparePlans;
    std::vector<std::pair<std::string, bool> > coboTopology;
    SystemAlignmentOptions alignmentOptions;
    std::size_t master = 0;
    bool level2 = false;
};

// Normalizes the immutable configuration copy, validates cross-device
// constraints and compiles plans without connecting to hardware.
CompiledSystemPreparationPlan CompileSystemPreparationPlan(const SystemPreparationOptions &options);

} // namespace SystemPreparationInternal
} // namespace GetDAQ

#endif
