#ifndef GETDAQ_ECCCONTROLJOBS_P_H
#define GETDAQ_ECCCONTROLJOBS_P_H

#include "ecccontroljobs.h"
#include "eccrequestproof_p.h"
#include "eccsuccessorplan.h"
#include "cobolifecycleprojection.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include "../../../common/text/utf8.h"
#include <cerrno>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <fcntl.h>
#include <map>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <unistd.h>
#include <sys/stat.h>
#include <vector>

namespace GetDAQ {
using namespace SupervisorIO;
namespace EccControlJobsInternal {
using namespace SupervisorIO;
bool NormalSealedData(const RemoteReceiverStatus &status);
RemoteReceiverOptions RuntimeReceiverOptions(FixedEccConfiguration &configuration);
NativeEccOptions StartupOptions(FixedEccConfiguration &configuration,const RemoteReceiverManager &data,
                                const std::shared_ptr<const EccSuccessorPlan> &successor);
EccCommand Command(const std::string &name);
void Validate(const EccControlRequest &request);
std::string AlarmObservation(const NativeEccAlarms &observation);

}
using namespace EccControlJobsInternal;

class EccControlJobs::Impl {
public:
    FixedEccConfiguration &configuration;
    std::shared_ptr<const EccSuccessorPlan> successor;
    RemoteReceiverManager data;
    NativeEccCoordinator controller;
    const std::string instance,snapshot;
    std::string directory,policy,journalBytes,ownerEpoch;
    std::unique_ptr<Directory> root,operations;
    Fd journal;
    struct stat policyIdentity,journalIdentity;
    std::uint64_t sequence=0;
    std::map<std::string,EccControlOperation> jobs;
    std::vector<std::string> operationOrder;
    std::map<std::string,std::shared_ptr<EccControlRequestProof>> requestProofs;
    mutable std::mutex stateMutex;
    std::mutex operationMutex;
    std::thread worker;
    std::thread alarmWorker;
    std::thread receiverWorker;
    std::atomic<bool> alarmShutdown{false};
    std::mutex alarmWaitMutex;
    std::condition_variable alarmWait;
    bool alarmHandled=false;
    std::string alarmOperationId,alarmPhase="disabled",alarmObservation="null";
    bool receiverHandled=false;
    std::string receiverOperationId,receiverPhase="disabled",receiverEvidence="null";
    bool active=false,needsReconciliation=false,storageFault=false;
    // Process-local by design: restart loses the retained remote Publish proof.
    std::string catalogRecoveryOriginalId;
    bool catalogRecoveryInProgress=false;
    bool handoffFenced=false;
    std::string handoffOperationId;
    std::string handoffCompletionOperationId;
    std::atomic<bool> inputFault{false};
    bool inputFaultRecorded=false;
    std::string activeId,message;
    void CheckHandoffCompletion(const EccControlRequest &r,bool replay=false) const;
    std::string CheckAlarmRecoveryBinding(const EccControlRequest &r,bool replay=false) const;
    std::string CheckStoppedAlarmBinding(const EccControlRequest &r,bool replay=false) const;
    explicit Impl(FixedEccConfiguration &config,std::shared_ptr<const EccSuccessorPlan> next,const std::string &receipt);
    ~Impl();
    EccControlOperation AcceptLocked(const EccControlRequest &request,bool automatic);
    void MonitorAlarms();
    void MonitorReceivers();
    std::string StatusJsonForPolicy() const;
    void Check(bool allowInputFault=false);
    void Record(const std::string &event,const std::string &fields="",bool allowInputFault=false);
    std::string ControllerJson() const;
    std::vector<const EccControlOperation *> OrderedOperations() const;
    EccControlControllerStatus ProjectionControllerStatus() const;
    void ProjectTarget(EccControlMaintenanceTarget &target) const;
    void ProjectTargets(std::vector<EccControlMaintenanceTarget> &targets) const;
    std::string ProjectedTargetLifecycle(const std::string &id) const;
    void Run(const EccControlRequest &r,const std::shared_ptr<EccControlRequestProof> &proof);
};

} // namespace GetDAQ

#endif
