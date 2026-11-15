// Host release steps adapted from received CEA CoBoNode::impl_unprepare.
// Original CeCILL license: third_party/licenses/getbench/COPYING.
#include "hardwareclient.h"
namespace GetDAQ {
namespace {
HardwareStep Step(HardwareStep::Kind kind,const char *device,const char *reg,const char *field,std::uint64_t value=0) {
    HardwareStep s; s.kind=kind; s.device=device; s.reg=reg; s.field=field; s.value=value; s.requireBoundDescription=true; return s;
}
}
std::vector<HardwareStep> CoBoUnprepareMonitoring() {
    std::vector<HardwareStep> steps{Step(HardwareStep::ReadValue,"ctrl","asadEnable","")};
    steps.back().annotation="actual powered AsAd mask before power-off; not configured activity";
    for (unsigned int i=0;i<4;++i) {
        const auto start=steps.size();
        steps.push_back(Step(HardwareStep::WriteValue,"ctrl","asadSelect","d",i));
        steps.push_back(Step(HardwareStep::ExpectValue,"ctrl","asadSelect","d",i));
        steps.push_back(Step(HardwareStep::ReadValue,"asad","interruptStatus1",""));
        steps.back().annotation="raw auto-protection status; nonzero is recorded, never clears protection";
        for (std::size_t n=start;n<steps.size();++n) {
            steps[n].conditionStep=0; steps[n].conditionMask=UINT64_C(1)<<i; steps[n].conditionValue=UINT64_C(1)<<i;
        }
    }
    return steps;
}
std::vector<HardwareStep> CoBoAsAdPowerOff() {
    return {Step(HardwareStep::WriteValue,"ctrl","asadEnable","",0),Step(HardwareStep::ExpectValue,"ctrl","asadEnable","",0)};
}
}
