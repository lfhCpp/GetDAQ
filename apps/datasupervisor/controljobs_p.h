#ifndef GETDAQ_SUPERVISORCONTROLJOBS_P_H
#define GETDAQ_SUPERVISORCONTROLJOBS_P_H

#include "controljobs.h"
#include "controlproofs_p.h"
#include "controljobs_journal_p.h"
#include "receiverplantransition.h"
#include "supervisorio.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

namespace GetDAQ {
class SupervisorControl::Impl {
public:
    SupervisorControlOptions options;
    std::shared_ptr<const SupervisorPlanPredecessor> predecessor;
    DataSupervisor data;
    const std::string instance,planHash;
    std::string directory,ownerSnapshot,ownerEpoch,replayInstance;
    std::unique_ptr<SupervisorControlJournal> journal;
    std::uint64_t sequence=0;
    mutable std::mutex stateMutex;
    std::mutex operationMutex;
    std::map<std::string,SupervisorControlOperation> jobs;
    std::map<std::string,std::shared_ptr<SupervisorHandoffProof>> handoffProofs;
    bool active=false,needsReconciliation=false,storageFault=false,startFailed=false;
    bool independentHold=false,retirementEligible=false;
    bool catalogRecoveryAllowed=false,catalogRecoveryInProgress=false;
    std::string catalogRecoveryReplacementId,catalogRecoverySession,catalogRecoveryExperiment;
    std::string catalogRecoveryPhase;
    unsigned int catalogRecoveryRun=0;
    bool planRetired=false,retirementCompletionUncertain=false;
    bool liveHandoffCompletionAllowed=false;
    std::string planRetirementOperation,planRetirementCompletionOperation;
    std::string currentOperation,message;
    std::thread worker;
    explicit Impl(const SupervisorControlOptions &o,std::shared_ptr<const SupervisorPlanPredecessor> previous);
    ~Impl();
    void CheckRetirementCompletion(const SupervisorControlRequest &r) const;
    void CheckControlHandoffCompletion(const SupervisorControlRequest &r,bool requireLive) const;
    void Replay(const JsonValue &v);
    void CheckStore();
    void Record(const std::string &event,const std::string &fields="");
    void Dispatch(const SupervisorControlRequest &r,std::shared_ptr<SupervisorNextPlan> next,
                  std::string &result,bool &retirement,bool &handoff);
    void Run(const SupervisorControlRequest &r,std::shared_ptr<SupervisorNextPlan> next={});
};
}
#endif
