#ifndef GETDAQ_SYSTEMACQUISITION_P_H
#define GETDAQ_SYSTEMACQUISITION_P_H

#include "systemacquisition.h"
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
class SystemAcquisitionClient::Impl {
public:
    SystemPreparationClient &prepared;
    SystemPreparationOptions options;
    std::unique_ptr<DataLinkSetClient> links;
    std::vector<DataSenderOptions> senderOptions;
    std::vector<CoBoAcquisitionPlan> plans;
    std::vector<std::unique_ptr<HardwareClient>> cobos, mutants;
    std::atomic<State> state{State::Configured};
    std::mutex operation;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory,linkDirectory;
    unsigned int sequence = 0;
    std::size_t master = 0;
    std::shared_ptr<CoBoLifecycleLedger> coboLifecycle;
    // 本次采集边界内，每个独立停止阶段只尝试一次。false为未确认，不能靠重发修复。
    std::map<std::pair<std::string,std::string>,bool> productionStops;
    bool brokenStopContext=false,brokenStopVerificationFailed=false;
    bool failureBreakupAttempted=false,failureBreakupComplete=false,captureFailureHardware=false;
    std::vector<std::pair<std::string,std::string>> failureBreakupChains,sourceRetirementChains;
    bool sourceRetirementAttempted=false,sourceRetirementComplete=false;
    explicit Impl(SystemPreparationClient &p,std::shared_ptr<CoBoLifecycleLedger> lifecycle);
    void Lifecycle(std::size_t index,CoBoLifecycleState next);
    void NegativeLifecycle(std::size_t index,CoBoLifecycleState next);
    void LifecycleAll(CoBoLifecycleState next);
    void RecordLifecycleMirror(const std::vector<CoBoLifecycleState> &before);
    void Check() const;
    void Record(const std::string &row);
    void Begin(const std::string &path,const char *action);
    void Complete(const char *stage,State target);
    void Fail();
    std::string File(const std::string &id,const char *stage);
    void Execute(HardwareClient &hw,const std::vector<HardwareStep> &steps,const std::string &id,const char *stage,bool interlock = true);
    void Hardware();
    void Name(HardwareClient &hw,const std::string &expected);
    void Preflight(bool start);
    void StartNodes(bool resume,bool restart=false);
    bool StopProducers();
    void VerifyBrokenProductionStopped();
    void Quiesce(CoBoLifecycleState terminalLifecycle=CoBoLifecycleState::Stopped);
};
}

#endif
