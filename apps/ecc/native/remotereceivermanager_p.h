#ifndef GETDAQ_REMOTERECEIVERMANAGER_P_H
#define GETDAQ_REMOTERECEIVERMANAGER_P_H

#include "remotereceivermanager.h"
#include "remoteoperationcodec.h"
#include "remoteendpointidentity_p.h"
#include "remoteobservations_p.h"
#include "eccsuccessorplan.h"
#include "eccjournal.h"
#include "remotehandoffevidence.h"
#include "receivercatalog.h"
#include "eccprotocol.h"
#include "configurationsha256.h"
#include "hardwareaudit.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <chrono>
#include <fcntl.h>
#include <sys/file.h>
#include <functional>
#include <exception>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <sstream>
#include <iomanip>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
namespace RemoteReceiverManagerInternal {

using namespace SupervisorIO;
using namespace RemoteOperationCodec;
using Clock=std::chrono::steady_clock;
std::string EmbeddedJsonObject(const std::string &encoded);
class RemoteBusy : public std::runtime_error { public: RemoteBusy() : std::runtime_error("Remote supervisor operation busy; not queued") {} };
JsonValue Response(const SupervisorHttpResponse &response,unsigned int expected);
void BoundFile(const std::string &path,const struct stat &identity,const std::string &bytes,std::size_t limit);
} // namespace RemoteReceiverManagerInternal
using namespace RemoteReceiverManagerInternal;

class RemoteReceiverManager::Impl {
public:
    struct Peer {
        RemoteReceiverEndpoint endpoint;
        SupervisorHttpClient client;
        std::string instance,catalogJson;
        SupervisorPlanDocument plan;
        DataSupervisorStatus observed,cycle;
        bool hasPlan=false,hasCycle=false,retiredCycle=false,serviceNeedsReconciliation=false;
        explicit Peer(const RemoteReceiverEndpoint &p) : endpoint(p),client(p.control) {}
    };
    RemoteReceiverOptions options;
    std::shared_ptr<const EccSuccessorPlan> successor;
    const std::string binding,epoch;
    std::string scope,rootPath,rootBytes,configHash,artifactHash;
    bool validated=false,failedAbortAttempted=false,bindingFault=false;
    bool failedAbortComplete=false,retirementAttempted=false,retirementBlocked=false;
    std::string failedAbortParent;
    bool handoffAttempted=false;
    std::string handoffCompleteJson;
    std::string predecessorHandoffJson;
    JsonValue predecessorHandoff;
    bool predecessorChangedPlans=false;
    std::set<std::string> predecessorVerifiedAgents;
    struct stat predecessorHandoffIdentity;
    Fd predecessorHandoffFd;
    std::unique_ptr<Directory> root,operations;
    struct stat snapshotIdentity;
    Fd snapshotFd;
    std::vector<std::unique_ptr<Peer>> peers;
    struct AuditProof {
        std::string path,intent,journal,handoffContext;
        std::unique_ptr<Directory> directory;
        std::unique_ptr<HardwareAudit> audit;
        struct stat intentIdentity,journalIdentity,handoffContextIdentity;
        void Check() const;
    };
    std::vector<std::unique_ptr<AuditProof>> proofs;
    AuditProof *handoffProof=nullptr;
    std::vector<SupervisorControlRequest> handoffRequests;
    // 1 means never submitted; 409/412 are explicit rejection replies. Zero
    // includes uncertain delivery and does not authorize another POST on 404.
    std::vector<unsigned int> handoffRejections;
    std::unique_ptr<RemoteHandoffEvidence> coldEvidence;
    std::unique_ptr<RemoteCatalogPublishEvidence> coldCatalogEvidence;
    std::function<void()> coldBoundary;
    std::string coldAuditPath;
    mutable std::mutex operationMutex,stateMutex;
    RemoteReceiverStatus status;
    explicit Impl(RemoteReceiverOptions o,std::shared_ptr<const EccSuccessorPlan> next,const std::string &originalEpoch="");
    void Latch(const std::string &message);
    void Gate(bool allowPendingHandoff=false,bool allowCatalogRecovery=false) const;
    void CheckRoot();
    void Publish(std::size_t index);
    void Deadline(const Clock::time_point &deadline) const;
    void Identity(std::size_t index,const Clock::time_point &deadline);
    void Observe(std::size_t index,const Clock::time_point &deadline,bool failureCleanup=false,
                 const std::string &successorScope="",const std::string &successorEpoch="");
    void AdoptPredecessorCycle(std::size_t index,const Clock::time_point &deadline);
    void VerifyPlanPredecessor(std::size_t index,const Clock::time_point &deadline);
    void Refresh(const Clock::time_point &deadline,bool allowCatalogRecovery=false);
    void Match(const std::vector<DataSenderOptions> &senders,const std::string &configuration,const std::string &artifact) const;
    SupervisorControlRequest Request(std::size_t index,SupervisorCommand command,const std::string &reference,const std::string &id,const std::string &experiment="",unsigned int run=0,bool replaceCompletedRun=false,const std::string &catalogPhase="single",const std::string &replacementId="") const;
    void Ready(Peer &peer,const DataSupervisorStatus &data,bool newCycle);
    void SameCycle(const Peer &peer,const DataSupervisorStatus &data,bool sealed) const;
    void Stopped(const Peer &peer,const DataSupervisorStatus &data) const;
    OperationResult WaitOperation(std::size_t index,const SupervisorControlRequest &request,OperationResult result,
                                  const Clock::time_point &deadline);
    OperationResult ObserveCompletedRetirement(std::size_t index,const SupervisorControlRequest &original,
        const Clock::time_point &deadline,const std::function<void(const std::string &)> &record);
    OperationResult ObserveCompletedHandoff(std::size_t index,const SupervisorControlRequest &original,
        const Clock::time_point &deadline,const std::function<void(const std::string &)> &record);
    OperationResult Execute(std::size_t index,const SupervisorControlRequest &request,const Clock::time_point &deadline,
                            const std::function<void(const std::string &)> &record={},unsigned int *rejection=nullptr);
    void ReadOnly(SupervisorCommand command,const Clock::time_point &deadline);
    void Mutation(SupervisorCommand command,const std::string &path,const std::vector<DataSenderOptions> *senders,const std::string &experiment="",unsigned int run=0,const std::vector<std::string> &unusedSourceIds={},const std::string &parentFailure="",
                  const std::string &successorScope="",const std::string &successorEpoch="",const std::vector<EccReceiverPlanTarget> &targets={},bool replaceCompletedRun=false,
                  const std::string &catalogPhase="single",const std::string &replacementId="",bool allowCatalogRecovery=false);
    std::unique_ptr<AuditProof> PersistMutationIntent(const std::string &path,bool handoff,
        const std::vector<SupervisorControlRequest> &requests);
    void ExecuteMutation(SupervisorCommand command,const std::string &path,
        const std::string &experiment,unsigned int run,const std::set<std::string> &unused,
        const std::vector<SupervisorControlRequest> &requests,const Clock::time_point &deadline,
        bool handoff,bool cleanup,bool retirement,bool changedPlans,const std::string &parentFailure,
        const std::string &successorScope,const std::string &successorEpoch,
        const std::vector<bool> &alreadySealed,AuditProof &proof);
    void ResumeCatalogReplacement(const std::string &newPath,const std::string &originalPath,const std::string &experiment,unsigned int run);
};

} // namespace GetDAQ
#endif
