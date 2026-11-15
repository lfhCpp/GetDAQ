#include "nativeecccoordinator_p.h"

#include "nativeeccfilesystemproof_p.h"
#include "nativeecccoordinator_recovery_p.h"
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

void NativeEccCoordinator::Impl::VerifyRecovery(EccState target,const EccStatus &previous,const std::string &evidence) {
    CheckBinding();
    const bool originalValid=acquisitionRecovery ?
        (description && description->GetState()==SystemDescriptionClient::State::Described && preparation &&
         preparation->GetState()==SystemPreparationClient::State::Unprepared && acquisition && acquisitionUnprepareProof) : preparationRecovery ?
        (previous.state==EccState::Described && description && description->GetState()==SystemDescriptionClient::State::Described &&
         preparation && preparation->GetState()==SystemPreparationClient::State::Unprepared && !acquisition) :
        (previous.state==EccState::Idle && description && description->GetState()==SystemDescriptionClient::State::Failed && !preparation && !acquisition);
    if(target!=EccState::Described || !originalValid || !previous.outcomeUnknown ||
       !verifiedDescription || verifiedDescription->GetState()!=SystemDescriptionClient::State::Described ||
       !descriptionRecoveryProof || evidence!=descriptionRecoveryEvidence ||
       previous.operationId!=descriptionRecoveryOriginal || controlInputFault ||
       (sourceFault && !sourceRecovery) || (receiverFault && !acquisitionRecovery) || data.NeedsReconciliation())
        throw std::logic_error("Recovery requires the same original failure and complete observation proof");
    const auto command=acquisitionRecovery ? context.command : preparationRecovery ? EccCommand::Prepare : EccCommand::Describe;
    const auto original=journal->Operation(previous.operationId);
    if(original.command!=int(command) || (acquisitionRecovery ? original.phase!="complete" && original.phase!="unknown" : original.phase!="unknown") || context.id!=previous.operationId || context.command!=command)
        throw std::logic_error("Recovery cannot adopt a cold or unrelated operation");
    const auto checkProof=[&] {
        try {
            if(acquisitionRecovery)CheckAcquisitionUnprepareProof();
            else if(preparationRecovery)CheckPreparationCleanupProof();
            else {if(!failedDescriptionDirectory || !failedDescriptionAudit)throw std::logic_error("Missing original Describe audit");
                failedDescriptionDirectory->Check();failedDescriptionAudit->Check();}
            if(sourceRecovery)CheckSourceRetirementProof();
            descriptionRecoveryProof->Check();for(const auto &file:descriptionVerificationFiles)file->Check();
        } catch(...) {storageFault=true;throw;}
        if(sourceRecovery && (!acquisition || acquisition->VerifyRetiredIceSources()!=sourceRetirementObservation))
            throw std::logic_error("Original retired source observation changed during recovery");
    };
    checkProof();data.VerifyStopped();CheckBinding();checkProof();
}

void NativeEccCoordinator::StopProductionAfterFailure(const std::string &id) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if (!lock.owns_lock()) throw std::runtime_error("Native ECC command already in progress; negative request was not queued");
    ++p.observationEpoch;
    OperationId(id); p.CheckBinding();
    if(!p.machine->Status().outcomeUnknown && p.acquisition && p.acquisition->CheckIceSourceFailures())p.sourceFault=true;
    if(p.sourceFault && !p.machine->Status().outcomeUnknown){const auto original=p.machine->Status().operationId;
        p.machine->HoldForReconciliation(original,"{\"kind\":\"getdaq.bound-source-failure-hold\",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+",\"original_operation_id\":"+JsonString(original)+"}");}
    const auto status=Status();
    if (!status.outcomeUnknown || (!p.acquisition && !p.preparation))
        throw std::logic_error("Native ECC failure production stop requires UNKNOWN and the same retained preparation/run object");
    if (p.acquisition) {
        const auto run=p.acquisition->GetState();
        if (run!=SystemAcquisitionClient::State::Failed && run!=SystemAcquisitionClient::State::Configured && run!=SystemAcquisitionClient::State::Ready &&
            run!=SystemAcquisitionClient::State::Running && run!=SystemAcquisitionClient::State::Paused && run!=SystemAcquisitionClient::State::Stopped &&
            run!=SystemAcquisitionClient::State::Broken)
            throw std::logic_error("Native ECC negative stop requires retained acquisition; completed Breakup permits only stopped readback verification");
    } else {
        const auto prepared=p.preparation->GetState();
        if (prepared!=SystemPreparationClient::State::Failed && prepared!=SystemPreparationClient::State::Prepared && prepared!=SystemPreparationClient::State::Configured)
            throw std::logic_error("Native ECC negative stop requires retained Failed/Prepared/Configured preparation");
    }
    const auto path=p.options.auditRoot+"/operations/"+id;
    const auto key=GetDAQ::ConfigurationSha256("getdaq.native-ecc-negative-operation:1\n"+p.snapshotHash+'\n'+id);
    if(p.productionStopProof)p.CheckProductionStopProof(); // A corrupt earlier audit cannot be replaced by a fresh ID.
    p.journal->CheckNewId(id,key); p.CheckUnusedPath(id); CreateHardwareAuditDirectory(path);
    Directory operationDirectory(path);
    HardwareAudit audit(path+"/operation.jsonl");
    const auto binding="\"operation_id\":"+JsonString(id)+",\"operation\":\"stop-production-after-failure\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+
        ",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+",\"ecc_unknown_preserved\":true,\"sender_changed\":"+
        (p.acquisition && std::any_of(p.senders.begin(),p.senders.end(),[](const DataSenderOptions &s) {return s.protocol=="ICE";}) ? "true" : "false")+",\"retained_object\":"+
        JsonString(p.acquisition ? "acquisition" : "preparation")+",\"retained_preparation_directory\":"+JsonString(p.prepareDirectory);
    const auto intent="{\"schema_version\":1,\"phase\":\"intent\","+binding+"}";
    audit.Append(intent);
    // 到子流程结束之前父文件只能包含这一规范意图；核对inode、权限和原字节。
    ImmutableFile intentProof(path+"/operation.jsonl",intent+'\n');
    // A new explicit check supersedes the old stop basis. If fresh Broken
    // readbacks contradict it, receiver cleanup cannot use the previous success.
    p.productionStopProof.reset();p.stoppedFailureOperation.clear();p.productionStopOperation.clear();
    try {
        if (p.acquisition) p.acquisition->StopProductionAfterFailure(path+"/stop-production",true);
        else p.preparation->StopProductionAfterFailure(path+"/stop-production",true);
        try { p.CheckBinding(); operationDirectory.Check(); intentProof.Check(); }
        catch (...) { p.storageFault=true; throw; }
        const auto complete="{\"phase\":\"backend_complete\","+binding+"}";
        audit.Append(complete);
        std::shared_ptr<ImmutableFile> stopProof(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.stoppedFailureOperation=status.operationId;p.productionStopOperation=id;p.productionStopProof=std::move(stopProof);
    }
    catch (...) { try { audit.Append("{\"phase\":\"failed\","+binding+",\"automatic_retry\":false}"); } catch (...) {} throw; }
}

void NativeEccCoordinator::RetireFailedSources(const std::string &id) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC source retirement busy; not queued");
    ++p.observationEpoch;OperationId(id);p.CheckBinding();const auto status=Status();
    if(!status.outcomeUnknown || !p.sourceFault || !p.acquisition || !p.productionStopProof || p.sourceRetirementAttempted ||
       p.stoppedFailureOperation!=status.operationId)throw std::logic_error("Source retirement requires the same confirmed failed-source stop and no prior attempt; no cold takeover");
    p.CheckProductionStopProof();p.CheckUnusedPath(id);const auto path=p.options.auditRoot+"/operations/"+id;
    p.journal->CheckNewId(id,GetDAQ::ConfigurationSha256("getdaq.native-ecc-source-retirement:1\n"+p.snapshotHash+'\n'+id));
    CreateHardwareAuditDirectory(path);Directory directory(path);HardwareAudit audit(path+"/operation.jsonl");
    const auto fields="\"operation_id\":"+JsonString(id)+",\"operation\":\"retire-failed-sources\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"source_end_requested\":false,\"parent_unknown_preserved\":true,\"run_complete\":false";
    const auto intent="{\"phase\":\"intent\","+fields+"}";audit.Append(intent);ImmutableFile intentProof(path+"/operation.jsonl",intent+'\n');p.sourceRetirementAttempted=true;
    try{
        p.acquisition->RetireFailedIceSources(path+"/source-retirement",true);
        p.CheckBinding();p.CheckProductionStopProof();directory.Check();intentProof.Check();
        std::vector<std::unique_ptr<ImmutableFile>> files;std::string entries="[";
        for(const auto &entry:p.acquisition->SourceRetirementAuditChains()){struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind source retirement audit");
            const auto bytes=ReadFailedDescriptionAudit(entry.first,identity,entry.second);files.emplace_back(new ImmutableFile(entry.first,bytes));
            if (entries.size()>1) entries+=',';
            entries+="{\"file\":"+JsonString(entry.first.substr(path.size()+1))+",\"sha256\":"+JsonString(GetDAQ::ConfigurationSha256(bytes))+",\"writer_chain_sha256\":"+JsonString(entry.second)+"}";}
        const auto manifest="{\"schema_version\":1,\"operation_id\":"+JsonString(id)+",\"failed_operation_id\":"+JsonString(status.operationId)+",\"audits\":"+entries+"]}";
        if(manifest.size()>1024*1024)throw std::runtime_error("Source retirement manifest exceeds 1 MiB");
        WriteNewFile(path+"/source-retirement-manifest.json",manifest);files.emplace_back(new ImmutableFile(path+"/source-retirement-manifest.json",manifest));
        const auto complete="{\"phase\":\"backend_complete\","+fields+",\"original_source_connections_retired\":true,\"source_retirement_manifest_sha256\":"+JsonString(GetDAQ::ConfigurationSha256(manifest))+"}";audit.Append(complete);
        p.sourceRetirementDirectory.reset(new Directory(path));p.sourceRetirementProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.sourceRetirementFiles=std::move(files);p.sourceRetirementOperation=id;p.sourceRetirementOriginal=status.operationId;p.sourceRetirementStopProof=p.productionStopProof;p.CheckSourceRetirementProof();
    }catch(...){try{audit.Append("{\"phase\":\"failed\","+fields+",\"automatic_retry\":false}");}catch(...){}throw;}
}

} // namespace GetDAQ
