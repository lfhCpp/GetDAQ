#ifndef GETDAQ_NATIVEECCCOORDINATOR_P_H
#define GETDAQ_NATIVEECCCOORDINATOR_P_H

#include "nativeecccoordinator.h"
#include "nativeeccplancompiler_p.h"
#include "nativeeccpreflight.h"
#include "nativeeccfilesystemproof_p.h"
#include "hostcontrolownership.h"
#include "../../../common/text/utf8.h"
#include <Ice/Ice.h>
#include "systemacquisition.h"
#include <chrono>
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <map>
#include <mutex>
#include <set>
#include <stdexcept>
#include <sys/stat.h>

namespace GetDAQ {
using namespace NativeEccFilesystemProof;

class NativeEccCoordinator::Impl : public EccBackend {
public:
    // Declared first so the reservation outlives every hardware client on destruction.
    std::unique_ptr<HostControlOwnership> ownership;
    NativeEccOptions options;
    EccDataLifecycle &data;
    SystemPreparationOptions bound;
    std::vector<DataSenderOptions> senders;
    std::string snapshot,snapshotHash,prepareDirectory,dataBinding;
    std::unique_ptr<Directory> root,operations;
    std::unique_ptr<ImmutableFile> artifactFile,snapshotFile;
    std::unique_ptr<EccJournal> journal;
    std::unique_ptr<EccStateMachine> machine;
    std::unique_ptr<SystemDescriptionClient> description;
    std::unique_ptr<Directory> failedDescriptionDirectory;
    std::unique_ptr<ImmutableFile> failedDescriptionAudit;
    std::unique_ptr<SystemDescriptionClient> verifiedDescription;
    std::unique_ptr<ImmutableFile> descriptionRecoveryProof;
    std::vector<std::unique_ptr<ImmutableFile>> descriptionVerificationFiles;
    std::string descriptionRecoveryEvidence,descriptionRecoveryOriginal;
    std::unique_ptr<SystemPreparationClient> preparation;
    std::unique_ptr<SystemAcquisitionClient> acquisition;
    std::shared_ptr<ImmutableFile> productionStopProof;
    std::shared_ptr<ImmutableFile> receiverAbortStopProof,failureBreakupStopProof;
    std::string stoppedFailureOperation,productionStopOperation;
    bool receiverAbortAttempted=false;
    bool receiverRetirementAttempted=false;
    std::unique_ptr<Directory> receiverAbortDirectory,receiverRetirementDirectory;
    std::unique_ptr<ImmutableFile> receiverAbortProof,receiverRetirementProof;
    std::string receiverAbortOperation,receiverRetirementOperation;
    bool acquisitionUnprepareAttempted=false;
    std::unique_ptr<Directory> acquisitionUnprepareDirectory;
    std::unique_ptr<ImmutableFile> acquisitionUnprepareProof;
    std::vector<std::unique_ptr<ImmutableFile>> acquisitionUnprepareFiles;
    std::string acquisitionUnprepareOperation;
    bool failureUnprepareAttempted=false;
    bool failureBreakupAttempted=false;
    std::unique_ptr<Directory> failureBreakupDirectory;
    std::unique_ptr<ImmutableFile> failureBreakupProof;
    std::vector<std::unique_ptr<ImmutableFile>> failureBreakupFiles;
    std::string failureBreakupOperation;
    std::shared_ptr<ImmutableFile> knownOperationAudit,releaseOriginalAudit;
    std::shared_ptr<Directory> knownOperationDirectory,releaseOriginalDirectory;
    std::string knownOperationId;
    std::unique_ptr<Directory> sourceRetirementDirectory;
    std::unique_ptr<ImmutableFile> sourceRetirementProof;
    std::vector<std::unique_ptr<ImmutableFile>> sourceRetirementFiles;
    std::shared_ptr<ImmutableFile> sourceRetirementStopProof;
    std::string sourceRetirementOperation,sourceRetirementOriginal,sourceRetirementObservation;
    bool sourceRetirementAttempted=false,sourceRecovery=false;
    void CheckSourceRetirementProof();
    bool preparationRecovery=false;
    bool acquisitionRecovery=false;
    std::unique_ptr<Directory> failedPreparationDirectory,failureCleanupDirectory;
    std::unique_ptr<ImmutableFile> failedPreparationAudit,failureCleanupProof;
    std::vector<std::unique_ptr<ImmutableFile>> failureCleanupFiles;
    std::string failureCleanupOperation;
    void CheckPreparationCleanupProof();
    void CheckProductionStopProof();
    void CheckFailureConnectionsProof();
    void CheckReceiverAbortProof();
    void CheckReceiverRetirementProof();
    void CheckAcquisitionUnprepareProof();
    std::mutex command;
    mutable std::mutex readbackMutex;
    std::string readbackOperation,readbackPhase="not-observed",readbackEntries="[]";
    std::atomic<bool> storageFault{false},sourceFault{false},receiverFault{false};
    std::atomic<bool> controlInputFault{false};
    std::atomic<std::uint64_t> observationEpoch{0};
    struct Context {
        EccCommand command=EccCommand::Describe;
        EccState from=EccState::Idle,to=EccState::Described;
        std::string id,path,key,maintenanceNodeId;
        unsigned int maintenanceAsadId=0,maintenanceLedType=0,maintenanceLedState=0,maintenanceLedFlipId=0,maintenanceLedPulseId=0,maintenanceLedPulsePeriodMs=0;
        int pulserVoltageMv=0;
        bool verifyAlarmRelease=false;
    } context;
    Impl(const NativeEccOptions &o,EccDataLifecycle &d);
    void CheckBinding();
    void Match(EccCommand c,EccState from,EccState to);
    void CheckUnusedPath(const std::string &id) const;
    void Validate(EccCommand c,EccState from,EccState to) override;
    void VerifyRecovery(EccState target,const EccStatus &previous,const std::string &evidence) override;
    void Apply(EccCommand c,EccState from,EccState to) override;
};

} // namespace GetDAQ

#endif
