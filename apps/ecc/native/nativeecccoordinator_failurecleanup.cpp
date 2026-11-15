#include "nativeecccoordinator_p.h"

#include "nativeeccfilesystemproof_p.h"
#include "nativeecccoordinator_recovery_p.h"
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"

#include <stdexcept>
#include <sys/stat.h>
#include <vector>

namespace GetDAQ {
using namespace NativeEccFilesystemProof;
using namespace NativeEccCoordinatorRecoveryInternal;

void NativeEccCoordinator::UnprepareAfterFailure(const std::string &id)
{
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; failure unprepare not queued");
    ++p.observationEpoch;OperationId(id);p.CheckBinding();const auto status=Status();
    if(!status.outcomeUnknown || status.state!=EccState::Described || !p.preparation || p.acquisition ||
       p.preparation->GetState()!=SystemPreparationClient::State::Failed || p.failureUnprepareAttempted ||
       !p.productionStopProof || p.stoppedFailureOperation!=status.operationId ||
       p.context.command!=EccCommand::Prepare || p.context.id!=status.operationId)
        throw std::logic_error("Failure unprepare requires the same failed Prepare and confirmed production stop; no Run/cold adoption or replay");
    const auto original=p.journal->Operation(status.operationId);
    if(original.command!=int(EccCommand::Prepare) || original.phase!="unknown")
        throw std::logic_error("Failure unprepare original Prepare journal differs");
    p.CheckProductionStopProof();p.data.VerifyStopped();p.CheckUnusedPath(id);
    const auto path=p.options.auditRoot+"/operations/"+id;CreateHardwareAuditDirectory(path);Directory directory(path);
    HardwareAudit audit(path+"/operation.jsonl");
    const auto fields="\"operation_id\":"+JsonString(id)+",\"operation\":\"unprepare-after-failure\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"parent_ecc_unknown_preserved\":true,\"sender_changed\":false,\"receiver_sealed\":false";
    const auto intent="{\"phase\":\"intent\","+fields+"}";audit.Append(intent);ImmutableFile proof(path+"/operation.jsonl",intent+'\n');
    p.failureUnprepareAttempted=true;
    try {
        p.preparation->UnprepareAfterFailure(path+"/failure-unprepare",true);
        p.data.VerifyStopped();p.CheckBinding();p.CheckProductionStopProof();directory.Check();proof.Check();
        std::vector<std::unique_ptr<ImmutableFile>> cleanupFiles;
        for(const auto &entry:p.preparation->FailureCleanupAuditChains()) {
            struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot capture completed cleanup audit");
            cleanupFiles.emplace_back(new ImmutableFile(entry.first,ReadFailedDescriptionAudit(entry.first,identity,entry.second)));
        }
        const auto complete="{\"phase\":\"backend_complete\","+fields+",\"retained_preparation_unprepared\":true}";
        audit.Append(complete);
        p.failureCleanupDirectory.reset(new Directory(path));
        p.failureCleanupProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.failureCleanupFiles=std::move(cleanupFiles);p.failureCleanupOperation=id;
        p.CheckPreparationCleanupProof();
    } catch(...) {try{audit.Append("{\"phase\":\"failed\","+fields+",\"automatic_retry\":false}");}catch(...){}throw;}
}

void NativeEccCoordinator::BreakupAfterFailure(const std::string &id)
{
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; failure Breakup not queued");
    ++p.observationEpoch;OperationId(id);p.CheckBinding();const auto status=Status();
    if(!status.outcomeUnknown || !p.acquisition || !p.preparation || !p.productionStopProof ||
       p.stoppedFailureOperation!=status.operationId || p.failureBreakupAttempted)
        throw std::logic_error("Failure Breakup requires the same retained acquisition and confirmed original production stop; no replay or cold takeover");
    if (p.sourceFault) {
        if (!p.sourceRetirementProof || p.sourceRetirementOriginal!=status.operationId)
            throw std::logic_error("Failure Breakup cannot adopt an unknown sender; explicitly retire original failed sources first");
        p.CheckSourceRetirementProof();
    }
    p.CheckProductionStopProof();
    p.journal->CheckNewId(id,GetDAQ::ConfigurationSha256("getdaq.native-ecc-failure-breakup:1\n"+p.snapshotHash+'\n'+id));
    p.CheckUnusedPath(id);const auto path=p.options.auditRoot+"/operations/"+id;
    CreateHardwareAuditDirectory(path);Directory directory(path);HardwareAudit audit(path+"/operation.jsonl");
    const auto fields="\"operation_id\":"+JsonString(id)+",\"operation\":\"breakup-after-failure\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"parent_ecc_unknown_preserved\":true,\"receiver_lifecycle_changed\":false,\"receiver_sealed\":false,\"run_complete\":false";
    const auto intent="{\"phase\":\"intent\","+fields+"}";audit.Append(intent);ImmutableFile intentProof(path+"/operation.jsonl",intent+'\n');
    p.failureBreakupAttempted=true;
    try {
        p.acquisition->BreakupAfterFailure(path+"/acquisition-cleanup",true);
        p.CheckBinding();p.CheckProductionStopProof();directory.Check();intentProof.Check();
        std::vector<std::unique_ptr<ImmutableFile>> files;
        for(const auto &entry:p.acquisition->FailureBreakupAuditChains()) {
            struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind failure Breakup audit");
            files.emplace_back(new ImmutableFile(entry.first,ReadFailedDescriptionAudit(entry.first,identity,entry.second)));
        }
        const auto complete="{\"phase\":\"backend_complete\","+fields+",\"retained_acquisition_broken\":true,\"retained_preparation_prepared\":true}";
        audit.Append(complete);p.failureBreakupDirectory.reset(new Directory(path));
        p.failureBreakupProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.failureBreakupFiles=std::move(files);p.failureBreakupOperation=id;p.failureBreakupStopProof=p.productionStopProof;
    } catch(...) {try{audit.Append("{\"phase\":\"failed\","+fields+",\"automatic_retry\":false}");}catch(...){}throw;}
}

void NativeEccCoordinator::UnprepareAcquisitionAfterFailure(const std::string &id) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; acquisition unprepare was not queued");
    ++p.observationEpoch;OperationId(id);p.CheckBinding();const auto status=Status();
    if(!status.outcomeUnknown || !p.acquisition || !p.preparation || !p.productionStopProof ||
       p.stoppedFailureOperation!=status.operationId || p.acquisitionUnprepareAttempted ||
       p.preparation->GetState()!=SystemPreparationClient::State::Prepared)
        throw std::logic_error("Acquisition unprepare requires the same original failed acquisition, retired data and completed connection cleanup; no replay or cold takeover");
    p.CheckReceiverRetirementProof();p.data.VerifyStopped();p.CheckBinding();p.CheckReceiverRetirementProof();
    p.journal->CheckNewId(id,GetDAQ::ConfigurationSha256("getdaq.native-ecc-acquisition-unprepare:1\n"+p.snapshotHash+'\n'+id));
    p.CheckUnusedPath(id);const auto path=p.options.auditRoot+"/operations/"+id;
    CreateHardwareAuditDirectory(path);Directory directory(path);HardwareAudit audit(path+"/operation.jsonl");
    const auto fields="\"operation_id\":"+JsonString(id)+",\"operation\":\"unprepare-acquisition-after-failure\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"receiver_retirement_operation_id\":"+JsonString(p.receiverRetirementOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"parent_ecc_unknown_preserved\":true,\"receiver_sealed\":false,\"run_complete\":false,\"new_receivers_started\":false";
    const auto intent="{\"phase\":\"intent\","+fields+"}";audit.Append(intent);ImmutableFile intentProof(path+"/operation.jsonl",intent+'\n');
    p.acquisitionUnprepareAttempted=true;
    try {
        p.preparation->Unprepare(path+"/preparation-cleanup",true);
        std::vector<std::unique_ptr<ImmutableFile>> files;
        for(const auto &entry:p.preparation->UnprepareAuditChains()) {
            struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind acquisition Unprepare audit");
            const auto bytes=ReadFailedDescriptionAudit(entry.first,identity,entry.second);
            // The original private preparation progress journal is append-only
            // under its retained writer. Keep its original prefix and identity;
            // bind the new full chain instead of treating a known append as drift.
            for(const auto &old:p.failureBreakupFiles)if(old->IsPath(entry.first))old->AdvanceVerifiedAppend(bytes);
            files.emplace_back(new ImmutableFile(entry.first,bytes));
        }
        p.CheckBinding();p.CheckReceiverRetirementProof();directory.Check();intentProof.Check();
        p.preparation->VerifyUnpreparedAfterFailure(path+"/verification");
        for(const auto &entry:p.preparation->FailureVerificationAuditChains()) {
            struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind final acquisition unprepared observation");
            const auto bytes=ReadFailedDescriptionAudit(entry.first,identity,entry.second);
            // Final read-only verification records the confirmed Fresh
            // lifecycle in the retained preparation journal. Accept only
            // that append, after verifying the earlier audit prefix.
            for(const auto &old:p.failureBreakupFiles)if(old->IsPath(entry.first))old->AdvanceVerifiedAppend(bytes);
            for(const auto &old:files)if(old->IsPath(entry.first))old->AdvanceVerifiedAppend(bytes);
            files.emplace_back(new ImmutableFile(entry.first,bytes));
        }
        p.data.VerifyStopped();p.CheckBinding();p.CheckReceiverRetirementProof();directory.Check();intentProof.Check();
        for(const auto &file:files)file->Check();
        const auto complete="{\"phase\":\"backend_complete\","+fields+",\"retained_preparation_unprepared\":true,\"all_power_off_observed\":true,\"owned_subscriptions_closed\":true}";
        audit.Append(complete);p.acquisitionUnprepareDirectory.reset(new Directory(path));
        p.acquisitionUnprepareProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.acquisitionUnprepareFiles=std::move(files);p.acquisitionUnprepareOperation=id;
    } catch(...) {try{audit.Append("{\"phase\":\"failed\","+fields+",\"automatic_retry\":false}");}catch(...){}throw;}
}


} // namespace GetDAQ
