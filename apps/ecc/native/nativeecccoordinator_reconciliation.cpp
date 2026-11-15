#include "nativeecccoordinator_p.h"
#include "nativeecccoordinator_recovery_p.h"

#include "nativeeccfilesystemproof_p.h"
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"

#include <algorithm>
#include <cerrno>
#include <set>
#include <stdexcept>
#include <sys/stat.h>

namespace GetDAQ {
using namespace NativeEccFilesystemProof;
using namespace NativeEccCoordinatorRecoveryInternal;

void NativeEccCoordinator::Reconcile(const std::string &id,EccState s,const std::string &evidence) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Native ECC command already in progress; recovery was not queued");
    ++p.observationEpoch;
    OperationId(id); p.CheckBinding(); p.machine->Reconcile(id,s,evidence); // Only internally bound observation proofs can satisfy the native verifier.
}
void NativeEccCoordinator::HoldReceiverFailure(const std::string &expectedOperationId) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Native ECC command busy; receiver failure was not rebound");
    ++p.observationEpoch;
    p.CheckBinding(); const auto status=Status();
    if (status.transition || status.operationId!=expectedOperationId || !p.acquisition ||
        (status.state!=EccState::Ready && status.state!=EccState::Running && status.state!=EccState::Paused))
        throw std::logic_error("Receiver failure requires the same retained Ready/Running/Paused acquisition");
    p.receiverFault=true;
    p.machine->HoldForReconciliation(expectedOperationId,"{\"kind\":\"getdaq.bound-receiver-failure-hold\",\"snapshot_sha256\":"+
        JsonString(p.snapshotHash)+",\"original_operation_id\":"+JsonString(expectedOperationId)+"}");
}
void NativeEccCoordinator::HoldControlInputFailure(const std::string &expectedOperationId) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; input fault was not rebound");
    ++p.observationEpoch;p.CheckBinding();const auto status=Status();
    if(status.transition || status.operationId!=expectedOperationId || (!p.acquisition && !p.preparation))
        throw std::logic_error("Input fault stop requires the same retained preparation/acquisition and operation");
    p.controlInputFault=true;
}
void NativeEccCoordinator::ReconcileDescription(const std::string &id,const std::string &failedId) {
    ReconcileOriginal(id,failedId,false);
}
void NativeEccCoordinator::ReconcilePreparation(const std::string &id,const std::string &failedId) {
    ReconcileOriginal(id,failedId,true);
}
void NativeEccCoordinator::ReconcileAcquisition(const std::string &id,const std::string &failedId) {
    ReconcileOriginal(id,failedId,false,true);
}
void NativeEccCoordinator::ReconcileAcquisitionWithSources(const std::string &id,const std::string &failedId) {ReconcileOriginal(id,failedId,false,true,true);}
void NativeEccCoordinator::ReconcileOriginal(const std::string &id,const std::string &failedId,bool prepareFailure,bool acquisitionFailure,bool sourceFailure)
{
    auto &p=*fImpl;
    std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; recovery was not queued");
    ++p.observationEpoch;
    OperationId(id);OperationId(failedId);p.CheckBinding();
    const auto previous=p.machine->Status();
    const auto command=acquisitionFailure ? p.context.command : prepareFailure ? EccCommand::Prepare : EccCommand::Describe;
    const bool originalValid=acquisitionFailure ?
        (p.description && p.description->GetState()==SystemDescriptionClient::State::Described && p.preparation &&
         p.preparation->GetState()==SystemPreparationClient::State::Unprepared && p.acquisition && p.acquisitionUnprepareProof &&
         p.productionStopProof && p.stoppedFailureOperation==failedId) : prepareFailure ?
        (previous.state==EccState::Described && p.description && p.description->GetState()==SystemDescriptionClient::State::Described &&
         p.preparation && p.preparation->GetState()==SystemPreparationClient::State::Unprepared && !p.acquisition &&
         p.failureUnprepareAttempted && p.productionStopProof && p.stoppedFailureOperation==failedId) :
        (previous.state==EccState::Idle && p.description && p.description->GetState()==SystemDescriptionClient::State::Failed && !p.preparation && !p.acquisition);
    if(!originalValid || !previous.outcomeUnknown || previous.operationId!=failedId ||
       p.controlInputFault || (p.sourceFault && !sourceFailure) || (p.receiverFault && !acquisitionFailure) || p.context.id!=failedId || p.context.command!=command || p.data.NeedsReconciliation())
        throw std::logic_error(acquisitionFailure ? "Acquisition recovery requires the same original stopped/retired/unprepared acquisition; no cold takeover" : prepareFailure ? "Recovery requires the same live failed Prepare and confirmed cleanup; no Run or cold takeover" : "Description recovery requires the same live failed Describe; no Run or cold takeover");
    if(sourceFailure){if(!p.sourceFault || !acquisitionFailure || p.sourceRetirementOriginal!=failedId)throw std::logic_error("Source recovery requires the same failed acquisition and explicit source retirement");
        p.CheckSourceRetirementProof();}
    const auto original=p.journal->Operation(failedId);
    if(original.command!=int(command) || (acquisitionFailure ? original.phase!="complete" && original.phase!="unknown" : original.phase!="unknown"))throw std::logic_error("Recovery original journal does not match");
    if(acquisitionFailure)p.CheckAcquisitionUnprepareProof();
    else if(prepareFailure)p.CheckPreparationCleanupProof();
    else {
        if(!p.failedDescriptionDirectory || !p.failedDescriptionAudit)throw std::logic_error("No retained failed Describe audit proof");
        try {p.failedDescriptionDirectory->Check();p.failedDescriptionAudit->Check();}catch(...) {p.storageFault=true;throw;}
    }
    p.CheckUnusedPath(id);
    p.data.VerifyStopped();
    const auto path=p.options.auditRoot+"/operations/"+id;
    CreateHardwareAuditDirectory(path);
    Directory directory(path);
    auto options=p.options.description;
    for(auto &node:options.nodes) {node.policy=DescriptionPolicy::UseExisting;node.currentDescription=HardwareDescription();}
    p.verifiedDescription.reset(new SystemDescriptionClient(options));
    p.descriptionRecoveryOriginal=failedId;p.preparationRecovery=prepareFailure;p.acquisitionRecovery=acquisitionFailure;p.sourceRecovery=sourceFailure;
    auto evidencePrefix="{\"schema_version\":1,\"kind\":"+JsonString(acquisitionFailure ? "getdaq.acquisition-reconciliation" : prepareFailure ? "getdaq.preparation-reconciliation" : "getdaq.description-reconciliation")+",\"operation_id\":"+JsonString(id)+
        ",\"failed_operation_id\":"+JsonString(failedId)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"read_only\":true,\"target_state\":2,\"actual_loaded_configuration_verified\":false,\"control_lock_verified\":false}";
    if(prepareFailure)evidencePrefix=evidencePrefix.substr(0,evidencePrefix.size()-1)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+
        ",\"failure_cleanup_operation_id\":"+JsonString(p.failureCleanupOperation)+
        ",\"retained_cleanup_audits_verified\":true,\"all_cobo_power_off_observed\":true,\"owned_subscriptions_closed\":true}";
    if(acquisitionFailure)evidencePrefix=evidencePrefix.substr(0,evidencePrefix.size()-1)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+
        ",\"receiver_retirement_operation_id\":"+JsonString(p.receiverRetirementOperation)+
        ",\"acquisition_unprepare_operation_id\":"+JsonString(p.acquisitionUnprepareOperation)+
        ",\"original_raw_retained\":true,\"original_run_complete\":false,\"fresh_prepare_required\":true}";
    if(sourceFailure)evidencePrefix=evidencePrefix.substr(0,evidencePrefix.size()-1)+",\"source_retirement_operation_id\":"+JsonString(p.sourceRetirementOperation)+",\"original_source_unknown_retained\":true}";
    WriteNewFile(path+"/recovery-intent.json",evidencePrefix);
    ImmutableFile recoveryIntent(path+"/recovery-intent.json",evidencePrefix);
    try {
        p.verifiedDescription->VerifyExistingDescription(path+"/description-observation");
        if(prepareFailure || acquisitionFailure)p.preparation->VerifyUnpreparedAfterFailure(path+"/preparation-observation");
        p.descriptionVerificationFiles.clear();
        if(sourceFailure){p.sourceRetirementObservation=p.acquisition->VerifyRetiredIceSources();
            WriteNewFile(path+"/source-retirement-observation.json",p.sourceRetirementObservation);
            p.descriptionVerificationFiles.emplace_back(new ImmutableFile(path+"/source-retirement-observation.json",p.sourceRetirementObservation));}
        std::string observed="[";
        auto chains=p.verifiedDescription->VerificationAuditChains();
        if(prepareFailure || acquisitionFailure) {const auto extra=p.preparation->FailureVerificationAuditChains();chains.insert(chains.end(),extra.begin(),extra.end());}
        for(const auto &entry:chains) {
            struct stat identity;
            if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind completed description observation audit");
            const auto bytes=ReadFailedDescriptionAudit(entry.first,identity,entry.second);
            p.descriptionVerificationFiles.emplace_back(new ImmutableFile(entry.first,bytes));
            if(observed.back()!='[')observed+=',';
            observed+="{\"file\":"+JsonString(entry.first.substr(path.size()+1))+",\"sha256\":"+JsonString(GetDAQ::ConfigurationSha256(bytes))+",\"writer_chain_sha256\":"+JsonString(entry.second)+"}";
        }
        observed+=']';
        const auto manifest="{\"schema_version\":1,\"operation_id\":"+JsonString(id)+
            ",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+",\"observation_audits\":"+observed+"}";
        if(manifest.size()>1024*1024)throw std::runtime_error("Recovery observation manifest exceeds 1 MiB");
        WriteNewFile(path+"/observation-manifest.json",manifest);
        p.descriptionVerificationFiles.emplace_back(new ImmutableFile(path+"/observation-manifest.json",manifest));
        auto evidence=evidencePrefix.substr(0,evidencePrefix.size()-1)+
            ",\"observation_manifest_file\":\"observation-manifest.json\",\"observation_manifest_sha256\":"+
            JsonString(GetDAQ::ConfigurationSha256(manifest))+",\"observation_audit_count\":"+std::to_string(chains.size())+"}";
        if(sourceFailure)evidence=evidence.substr(0,evidence.size()-1)+",\"source_retirement_observation_file\":\"source-retirement-observation.json\",\"source_retirement_observation_sha256\":"+JsonString(GetDAQ::ConfigurationSha256(p.sourceRetirementObservation))+"}";
        if(evidence.size()>4096)throw std::runtime_error("Recovery proof exceeds the ECC journal evidence budget");
        p.data.VerifyStopped();p.CheckBinding();directory.Check();recoveryIntent.Check();
        if(acquisitionFailure)p.CheckAcquisitionUnprepareProof();
        else if(prepareFailure)p.CheckPreparationCleanupProof();
        WriteNewFile(path+"/recovery-proof.json",evidence);
        p.descriptionRecoveryProof.reset(new ImmutableFile(path+"/recovery-proof.json",evidence));
        p.descriptionRecoveryEvidence=evidence;
        p.machine->Reconcile(id,EccState::Described,evidence);
        p.description.swap(p.verifiedDescription);
        if(acquisitionFailure) {
            if(sourceFailure){p.sourceFault=false;p.sourceRetirementFiles.clear();p.sourceRetirementProof.reset();p.sourceRetirementDirectory.reset();
                p.sourceRetirementStopProof.reset();p.sourceRetirementOperation.clear();p.sourceRetirementOriginal.clear();p.sourceRetirementObservation.clear();p.sourceRetirementAttempted=false;}
            p.receiverFault=false;p.acquisition.reset();p.preparation.reset();p.prepareDirectory.clear();
            p.productionStopProof.reset();p.receiverAbortStopProof.reset();p.failureBreakupStopProof.reset();
            p.stoppedFailureOperation.clear();p.productionStopOperation.clear();
            p.receiverAbortProof.reset();p.receiverAbortDirectory.reset();p.receiverAbortOperation.clear();p.receiverAbortAttempted=false;
            p.receiverRetirementProof.reset();p.receiverRetirementDirectory.reset();p.receiverRetirementOperation.clear();p.receiverRetirementAttempted=false;
            p.failureBreakupFiles.clear();p.failureBreakupProof.reset();p.failureBreakupDirectory.reset();p.failureBreakupOperation.clear();p.failureBreakupAttempted=false;
            p.acquisitionUnprepareFiles.clear();p.acquisitionUnprepareProof.reset();p.acquisitionUnprepareDirectory.reset();p.acquisitionUnprepareOperation.clear();p.acquisitionUnprepareAttempted=false;
        }

        if(prepareFailure) {
            p.preparation.reset();p.prepareDirectory.clear();p.productionStopProof.reset();p.stoppedFailureOperation.clear();p.productionStopOperation.clear();
            p.failureCleanupFiles.clear();p.failureCleanupProof.reset();p.failureCleanupDirectory.reset();p.failureCleanupOperation.clear();
            p.failedPreparationAudit.reset();p.failedPreparationDirectory.reset();p.failureUnprepareAttempted=false;
        }
        p.preparationRecovery=false;p.acquisitionRecovery=false;p.sourceRecovery=false;
        p.verifiedDescription.reset();p.descriptionRecoveryProof.reset();p.descriptionVerificationFiles.clear();
        p.descriptionRecoveryEvidence.clear();p.descriptionRecoveryOriginal.clear();
    } catch(...) {
        p.preparationRecovery=false;p.acquisitionRecovery=false;p.sourceRecovery=false;
        p.verifiedDescription.reset();p.descriptionRecoveryProof.reset();p.descriptionVerificationFiles.clear();
        p.descriptionRecoveryEvidence.clear();p.descriptionRecoveryOriginal.clear();
        throw;
    }
}

} // namespace GetDAQ
