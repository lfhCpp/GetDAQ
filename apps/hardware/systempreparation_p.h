#ifndef GETDAQ_SYSTEMPREPARATION_P_H
#define GETDAQ_SYSTEMPREPARATION_P_H

// Host sequence adapted from CEA GET SystemManager (received 2021 release).
// Original CeCILL license retained in third_party/licenses/getbench/COPYING.
#include "systempreparation.h"
#include "cobolifecycle.h"
#include "acquisitionplans.h"
#include "hardwareaudit.h"
#include <atomic>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

namespace GetDAQ {
namespace SystemPreparationInternal {
void Text(const std::string &value,std::size_t maximum);
HardwareStep Field(HardwareStep::Kind kind,const char *device,const char *reg,const char *field,std::uint64_t value);
std::vector<HardwareStep> CoBoBit(const char *name,bool enabled);
std::vector<HardwareStep> CoBoStopped();
void Inventory(HardwareClient &hardware,const HardwareDescription &policy,const std::string &name);
}
using namespace SystemPreparationInternal;
class SystemPreparationClient::Impl {
public:
    SystemPreparationOptions options;
    std::atomic<State> state{State::Fresh};
    std::mutex operation;
    std::vector<std::unique_ptr<HardwareClient>> cobos, mutants;
    // 与正向Prepare的部分客户端集合分开保存，仍只绑定本对象的不可变节点。
    std::vector<std::unique_ptr<HardwareClient>> stopCobos, stopMutants;
    std::map<std::pair<std::string,std::string>,bool> productionStops;
    std::map<std::pair<std::string,std::string>,bool> failureCleanupSteps;
    bool failureCleanupComplete=false;
    bool unprepareComplete=false;
    std::vector<std::pair<std::string,std::string>> unprepareChains;
    std::vector<std::pair<std::string,std::string>> failureCleanupChains,failureVerificationChains;
    std::vector<std::pair<std::string,std::string>> invalidationChains;
    std::vector<std::unique_ptr<CoBoPrepareClient>> preparations;
    // Per-CoBo command stage, retained even when a later board/global phase fails.
    std::shared_ptr<CoBoLifecycleLedger> coboLifecycle;
    std::vector<std::vector<HardwareStep>> configurePlans, mutantPreparePlans;
    std::unique_ptr<SystemAlignmentClient> alignment;
    SystemAlignmentResult result;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory;
    unsigned int sequence = 0;
    std::vector<std::string> configureReadbacks;
    std::size_t master = 0;
    bool level2 = false;
    void Record(const std::string &row);
    void Check() const;
    void Execute(HardwareClient &hw,const std::vector<HardwareStep> &steps,const std::string &id,const char *stage);
    void Fail();
    void SetCoBoLifecycle(std::size_t index,const CoBoLifecycleState next);
    void MarkCoBoUnknown(std::size_t index,const std::string &id);
    void ValidateIdentities() const;
    void Preflight();
    void ValidateConfigure();
    void ReleasePreflight(bool powerOff);
};

} // namespace GetDAQ

#endif
