#include "nativeecccoordinator_p.h"
#include <stdexcept>
namespace GetDAQ {
void NativeEccCoordinator::Impl::Apply(EccCommand c,EccState from,EccState to) {
    Match(c,from,to); CreateHardwareAuditDirectory(context.path); Directory operationDirectory(context.path);
    HardwareAudit audit(context.path+"/operation.jsonl");
    if(c==EccCommand::Describe) {failedDescriptionAudit.reset();failedDescriptionDirectory.reset(new Directory(context.path));}
    if(c==EccCommand::Prepare) {failedPreparationAudit.reset();failedPreparationDirectory.reset(new Directory(context.path));}
    struct stat auditIdentity;
    if (::lstat((context.path+"/operation.jsonl").c_str(),&auditIdentity)!=0) throw std::runtime_error("Cannot bind native ECC operation audit identity");
    auto binding="\"operation_id\":"+JsonString(context.id)+",\"command\":"+std::to_string(int(c))+
        ",\"from\":"+std::to_string(int(from))+",\"to\":"+std::to_string(int(to))+",\"snapshot_sha256\":"+JsonString(snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(bound.artifactSha256)+
        ",\"request_key\":"+JsonString(context.key)+",\"retained_preparation_directory\":"+JsonString(prepareDirectory);
    if(c==EccCommand::ReprepareCoBo || c==EccCommand::ConfigureCoBo || c==EccCommand::TriggerAsAdPulser || c==EccCommand::BlinkCoBoLeds || c==EccCommand::SetCoBoLed || c==EccCommand::FlipCoBoLed || c==EccCommand::PulseCoBoLed) binding+=",\"cobo_id\":"+JsonString(context.maintenanceNodeId);
    if(c==EccCommand::FlipCoBoLed)binding+=",\"led_id\":"+std::to_string(context.maintenanceLedFlipId);
    if(c==EccCommand::PulseCoBoLed)binding+=",\"led_id\":"+std::to_string(context.maintenanceLedPulseId)+",\"period_ms\":"+std::to_string(context.maintenanceLedPulsePeriodMs);
    if(c==EccCommand::TriggerAsAdPulser) binding+=",\"pulser_voltage_mv\":"+std::to_string(context.pulserVoltageMv);
    audit.Append("{\"schema_version\":1,\"phase\":\"intent\","+binding+",\"control_lock_verified\":false,\"actual_loaded_configuration_verified\":false,\"network_service\":false}");
    try {
        switch (c) {
        case EccCommand::Describe: data.VerifyStopped(); description->Describe(context.path+"/describe"); break;
        case EccCommand::Prepare:
            data.VerifyStopped(); preparation.reset(new SystemPreparationClient(bound));
            prepareDirectory=context.path+"/prepare"; preparation->Prepare(prepareDirectory); break;
        case EccCommand::ReprepareCoBo:
            data.VerifyStopped();preparation->ReprepareCoBo(context.maintenanceNodeId,context.path+"/maintenance",true);break;
        case EccCommand::ConfigureCoBo:
            data.VerifyStopped();preparation->ConfigureCoBo(context.maintenanceNodeId,context.path+"/maintenance",true);break;
        case EccCommand::SetCoBoLed:
            data.VerifyStopped();preparation->SetCoBoLed(context.maintenanceNodeId,context.maintenanceAsadId,
                HardwareStep::LedType(context.maintenanceLedType),HardwareStep::LedState(context.maintenanceLedState),
                context.path+"/maintenance",[this] { data.VerifyStopped(); });break;
        case EccCommand::BlinkCoBoLeds:
            data.VerifyStopped();preparation->BlinkCoBoLeds(context.maintenanceNodeId,context.path+"/maintenance",[this] { data.VerifyStopped(); });break;
        case EccCommand::FlipCoBoLed:
            data.VerifyStopped();preparation->FlipCoBoLed(context.maintenanceNodeId,context.maintenanceLedFlipId,context.path+"/maintenance",[this] { data.VerifyStopped(); });break;
        case EccCommand::PulseCoBoLed:
            data.VerifyStopped();preparation->PulseCoBoLed(context.maintenanceNodeId,context.maintenanceLedPulseId,context.maintenanceLedPulsePeriodMs,context.path+"/maintenance",[this] { data.VerifyStopped(); });break;
        case EccCommand::TriggerAsAdPulser:
            data.VerifyStopped();preparation->TriggerAsAdPulser(context.maintenanceNodeId,context.pulserVoltageMv,
                context.path+"/maintenance",[this] { data.VerifyStopped(); });break;
        case EccCommand::Configure:
            data.VerifyStopped(); preparation->Configure();
            {
                const auto entries=preparation->ConfigureReadbacksJson();
                std::lock_guard<std::mutex> guard(readbackMutex);readbackEntries=entries;readbackPhase="captured";
            }
            data.StartReceivers(context.path+"/receivers-start",senders); data.VerifyReady();
            acquisition.reset(new SystemAcquisitionClient(*preparation,senders));
            if(std::any_of(senders.begin(),senders.end(),[](const DataSenderOptions &sender) {return sender.protocol=="ICE";}))
                acquisition->BindIceReceiverSessions(data.IceReceiverSessions());
            acquisition->ConnectData(context.path+"/connect-data");
            data.VerifyReady(); break;
        case EccCommand::Start: data.VerifyReady(); acquisition->Start(context.path+"/start"); break;
        case EccCommand::Stop: acquisition->Stop(context.path+"/stop"); data.VerifyReady(); break;
        case EccCommand::Pause: acquisition->Pause(context.path+"/pause"); data.VerifyReady(); break;
        case EccCommand::Resume: data.VerifyReady(); acquisition->Resume(context.path+"/resume"); break;
        case EccCommand::Breakup:
            acquisition->Breakup(context.path+"/breakup");
            {
                std::vector<std::string> unused;
                for (const auto &sender:senders) if (sender.protocol=="FDT") {
                    const auto status=acquisition->SenderStatus(sender.senderId);
                    if (status.outcomeUnknown || status.state!=DataSenderSnapshot::Disconnected) throw std::runtime_error("FDT sender not confirmed disconnected; no unused-source declaration");
                    if (!status.startIssued) unused.push_back(sender.senderId);
                }
                std::sort(unused.begin(),unused.end());
                data.FinishReceivers(context.path+"/receivers-finish",unused); data.VerifyStopped();
            }
            break;
        case EccCommand::Reset:
            data.VerifyStopped();
            if (from==EccState::Prepared) preparation->Unprepare(context.path+"/unprepare",true);
            if(context.verifyAlarmRelease) {
                if(from!=EccState::Prepared || !preparation || !acquisition || acquisition->GetState()!=SystemAcquisitionClient::State::Broken)
                    throw std::logic_error("Stopped alarm release requires the same completed Breakup and retained preparation");
                preparation->VerifyUnpreparedAfterFailure(context.path+"/stopped-alarm-observation");
                auto verifyOptions=options.description;
                for(auto &node:verifyOptions.nodes){node.policy=DescriptionPolicy::UseExisting;node.currentDescription=HardwareDescription();}
                SystemDescriptionClient verified(verifyOptions);verified.VerifyExistingDescription(context.path+"/description-observation");
                std::vector<std::unique_ptr<ImmutableFile>> proofs;
                auto chains=preparation->UnprepareAuditChains();
                const auto observed=preparation->FailureVerificationAuditChains();chains.insert(chains.end(),observed.begin(),observed.end());
                const auto descriptions=verified.VerificationAuditChains();chains.insert(chains.end(),descriptions.begin(),descriptions.end());
                for(const auto &entry:chains){struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot bind stopped alarm release audit");
                    proofs.emplace_back(new ImmutableFile(entry.first,ReadFailedDescriptionAudit(entry.first,identity,entry.second)));}
                data.VerifyStopped();CheckBinding();
                if(!knownOperationAudit || !knownOperationDirectory)throw std::logic_error("Missing retained Breakup proof");
                knownOperationAudit->Check();knownOperationDirectory->Check();
                if(!releaseOriginalAudit || !releaseOriginalDirectory)throw std::logic_error("Missing retained original stopped operation proof");
                releaseOriginalAudit->Check();releaseOriginalDirectory->Check();
                std::string manifest="[";
                for(std::size_t i=0;i<proofs.size();++i){proofs[i]->Check();if(i)manifest+=',';
                    const auto &entry=chains[i];struct stat identity;if(::lstat(entry.first.c_str(),&identity))throw std::runtime_error("Cannot inspect verified release audit");
                    const auto bytes=ReadFailedDescriptionAudit(entry.first,identity,entry.second);proofs[i]->Check();
                    manifest+="{\"path\":"+JsonString(entry.first)+",\"sha256\":"+JsonString(GetDAQ::ConfigurationSha256(bytes))+"}";}
                audit.Append("{\"phase\":\"stopped_alarm_release_verified\",\"read_only_final_observation\":true,\"original_run_complete\":false,\"fresh_prepare_required\":true,\"audits\":"+manifest+"]}");
            }
            // Described→Idle与收到的旧ECC一致：忘记本地拓扑，不destroy远端描述。
            break;
        }
        CheckBinding(); operationDirectory.Check();
        struct stat auditCurrent;
        if (::lstat((context.path+"/operation.jsonl").c_str(),&auditCurrent)!=0 || !S_ISREG(auditCurrent.st_mode) || !SameIdentity(auditIdentity,auditCurrent))
            throw std::runtime_error("Native ECC operation audit identity changed before commit");
        audit.Append("{\"phase\":\"backend_complete\","+binding+",\"ecc_state_committed\":false}");
        if(c==EccCommand::Configure || c==EccCommand::Stop || c==EccCommand::Breakup) {
            knownOperationAudit.reset(new ImmutableFile(context.path+"/operation.jsonl",ReadFailedDescriptionAudit(context.path+"/operation.jsonl",auditIdentity,audit.ChainSha256())));
            knownOperationDirectory.reset(new Directory(context.path));knownOperationId=context.id;
        }
        if (c==EccCommand::Reset) {
            if (from==EccState::Prepared) { acquisition.reset(); preparation.reset(); prepareDirectory.clear(); }
            else description.reset();
        }
    } catch (...) {
        try {
            audit.Append("{\"phase\":\"failed\","+binding+",\"automatic_retry\":false,\"automatic_cleanup\":false}");
            if(c==EccCommand::Describe) {
                failedDescriptionDirectory->Check();
                const auto bytes=ReadFailedDescriptionAudit(context.path+"/operation.jsonl",auditIdentity,audit.ChainSha256());
                failedDescriptionAudit.reset(new ImmutableFile(context.path+"/operation.jsonl",bytes));
                failedDescriptionDirectory->Check();failedDescriptionAudit->Check();
            }
            if(c==EccCommand::Prepare) {
                failedPreparationDirectory->Check();
                const auto bytes=ReadFailedDescriptionAudit(context.path+"/operation.jsonl",auditIdentity,audit.ChainSha256());
                failedPreparationAudit.reset(new ImmutableFile(context.path+"/operation.jsonl",bytes));
                failedPreparationDirectory->Check();failedPreparationAudit->Check();
            }
        } catch(...) {if(c==EccCommand::Describe || c==EccCommand::Prepare)storageFault=true;}
        throw;
    }
    // 此后仅状态机的journal.Complete提交ECC；不增加可能在已提交后失败的补写。
}
} // namespace GetDAQ
