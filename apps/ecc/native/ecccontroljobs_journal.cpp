#include "ecccontroljobs_p.h"
#include <stdexcept>
namespace GetDAQ {
namespace {
void Append(int fd,const std::string &bytes) {
    std::size_t offset=0;
    while (offset<bytes.size()) { const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset); if (n<0 && errno==EINTR) continue; if (n<=0) throw std::runtime_error("Cannot write ECC network journal"); offset+=std::size_t(n); }
    if (::fsync(fd)) throw std::runtime_error("Cannot sync ECC network journal");
}
}

void EccControlJobs::Impl::Check(bool allowInputFault) {
    if (storageFault) throw std::runtime_error("ECC network storage fault is latched");
    try{configuration.Check();if(successor)successor->Check();}
    catch(...){inputFault=true;{std::lock_guard<std::mutex> lock(stateMutex);needsReconciliation=true;message="Fixed external inputs changed; original targets retained, explicit fault production stop may be requested";}if(!allowInputFault)throw;}
    if(inputFault && !allowInputFault)throw std::runtime_error("ECC fixed input fault is latched; no positive command or input reload");
    try {
        root->Check(); operations->Check(); struct stat st,held;
        if (::lstat((directory+"/policy.json").c_str(),&st) || !Same(st,policyIdentity) || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || ReadFile(directory+"/policy.json",65536)!=policy)
            throw std::runtime_error("ECC network policy changed");
        if (::lstat((directory+"/operations.jsonl").c_str(),&st) || ::fstat(journal.value,&held) || !Same(st,journalIdentity) || !Same(st,held) || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() ||
            (st.st_mode&0777)!=0600 || ReadFile(directory+"/operations.jsonl",64*1024*1024)!=journalBytes) throw std::runtime_error("ECC network journal changed");
        for (const auto &entry:requestProofs) entry.second->Check();
    } catch (...) { std::lock_guard<std::mutex> lock(stateMutex); storageFault=needsReconciliation=true; message="ECC input/control storage changed; restoration does not clear gate"; throw; }
}
void EccControlJobs::Impl::Record(const std::string &event,const std::string &fields,bool allowInputFault) {
    Check(allowInputFault); const auto bytes="{\"schema\":1,\"sequence\":"+std::to_string(sequence)+",\"snapshot_sha256\":"+EncodeJsonString(snapshot)+",\"event\":"+EncodeJsonString(event)+fields+"}\n";
    if (journalBytes.size()+bytes.size()>64*1024*1024) throw std::runtime_error("ECC network journal budget exhausted");
    Append(journal.value,bytes); journalBytes+=bytes; ++sequence; Check(allowInputFault);
}

EccControlJobs::Impl::Impl(FixedEccConfiguration &config,std::shared_ptr<const EccSuccessorPlan> next,const std::string &receipt) : configuration(config),successor(next),data(RuntimeReceiverOptions(config),next),controller(StartupOptions(config,data,next),data),instance(RandomSession()),snapshot(controller.SnapshotSha256()),journal(-1){
        if (successor) { data.AdoptControlHandoff(receipt);data.VerifyStopped();successor->Check(); }
        else if (!receipt.empty()) throw std::invalid_argument("Handoff receipt requires a frozen successor plan");
        config.Check(); directory=config.ControllerOptions().auditRoot+"/network"; struct stat st; const bool existing=::lstat(directory.c_str(),&st)==0;
        if (!existing && errno!=ENOENT) throw std::runtime_error("Cannot inspect ECC network root");
        if (!existing) CreateDirectory(directory);
        root.reset(new Directory(directory));
        policy="{\"schema\":1,\"kind\":\"getdaq.ecc-network-policy\",\"service_id\":"+EncodeJsonString(config.ServiceId())+",\"client_id\":"+EncodeJsonString(config.ClientId())+
            ",\"input_binding_sha256\":"+EncodeJsonString(config.BindingSha256())+",\"snapshot_sha256\":"+EncodeJsonString(snapshot)+
            (successor ? ",\"successor_plan_sha256\":"+EncodeJsonString(successor->Sha256())+",\"receiver_controller_epoch\":"+EncodeJsonString(successor->Epoch()) : "")+"}\n";
        if (!existing) { CreateDirectory(directory+"/operations"); WriteNew(directory+"/policy.json",policy); WriteNew(directory+"/operations.jsonl",""); }
        operations.reset(new Directory(directory+"/operations"));
        if (::lstat((directory+"/policy.json").c_str(),&policyIdentity) || ReadFile(directory+"/policy.json",65536)!=policy) throw std::runtime_error("ECC network immutable policy differs");
        journal.value=::open((directory+"/operations.jsonl").c_str(),O_RDWR|O_APPEND|O_NOFOLLOW|O_CLOEXEC);
        if (journal.value<0 || ::fstat(journal.value,&journalIdentity) || !S_ISREG(journalIdentity.st_mode)) throw std::runtime_error("Invalid ECC network journal");
        journalBytes=ReadFile(directory+"/operations.jsonl",64*1024*1024);
        if (!journalBytes.empty() && journalBytes.back()!='\n') throw std::runtime_error("Incomplete ECC network journal; no repair/replay");
        std::string replayInstance; std::size_t start=0;
        while (start<journalBytes.size()) {
            const auto end=journalBytes.find('\n',start); JsonLimits limits; limits.stringBytes=1024*1024;
            const auto v=ParseStrictJson(journalBytes.substr(start,end-start),limits); start=end+1;
            if (Unsigned(v.At("schema"))!=1 || Unsigned(v.At("sequence"))!=sequence++ || String(v.At("snapshot_sha256"))!=snapshot) throw std::runtime_error("ECC network journal sequence/scope differs");
            const auto event=String(v.At("event"));
            if (event=="opened") { ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","instance_id"}); replayInstance=String(v.At("instance_id")); Digest(replayInstance); continue; }
            if (event=="owner") { ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","client_epoch"}); if (!ownerEpoch.empty()) throw std::runtime_error("Repeated ECC network owner"); ownerEpoch=String(v.At("client_epoch")); Digest(ownerEpoch); continue; }
            if(event=="input-fault-held") {
                ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","failed_operation_id","input_binding_sha256"});
                SupervisorOperationId(String(v.At("failed_operation_id")));
                if(inputFaultRecorded || String(v.At("input_binding_sha256"))!=config.BindingSha256())throw std::runtime_error("Unexpected input fault hold binding");
                inputFault=inputFaultRecorded=true;continue;
            }
            if (event=="alarm-held") {
                ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","operation_id","evidence_sha256","evidence"});
                if (!config.AlarmStopEnabled() || alarmHandled) throw std::runtime_error("Unexpected/repeated alarm hold in ECC journal");
                alarmOperationId=String(v.At("operation_id")); if (!alarmOperationId.empty()) SupervisorOperationId(alarmOperationId);
                alarmObservation=String(v.At("evidence")); const auto evidence=ParseStrictJson(alarmObservation);
                if (!Boolean(evidence.At("stop_requested")) || ConfigurationSha256("getdaq.ecc-alarm-observation:1\n"+alarmObservation)!=String(v.At("evidence_sha256")))
                    throw std::runtime_error("ECC alarm observation binding differs");
                alarmHandled=true; alarmPhase="reopened-held"; continue;
            }
            if (event=="receiver-held") {
                ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","operation_id","evidence_sha256","evidence"});
                if (!config.ReceiverStopEnabled() || receiverHandled) throw std::runtime_error("Unexpected/repeated receiver hold in ECC journal");
                receiverOperationId=String(v.At("operation_id")); SupervisorOperationId(receiverOperationId);
                receiverEvidence=String(v.At("evidence")); JsonLimits limits; limits.stringBytes=1024*1024;
                const auto evidence=ParseStrictJson(receiverEvidence,limits);
                ExactSupervisorFields(evidence,{"bound_status","observation"});
                const auto bound=DecodeEccControlSystemStatus(String(evidence.At("bound_status")));
                const auto observed=DecodeEccControlReceiverObservation(String(evidence.At("observation")));
                if (bound.snapshotSha256!=snapshot || bound.instanceId!=replayInstance ||
                    FailedBoundReceivers(bound,observed,data.BindingSha256()).empty() ||
                    ConfigurationSha256("getdaq.ecc-receiver-failure:1\n"+receiverEvidence)!=String(v.At("evidence_sha256")))
                    throw std::runtime_error("ECC receiver failure evidence binding differs");
                receiverHandled=true; receiverPhase="reopened-held"; continue;
            }
            const auto id=String(v.At("operation_id")); SupervisorOperationId(id);
            if (event=="accepted") {
                ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","operation_id","request_key","request"}); EccControlOperation op;
                op.request=DecodeEccControlRequest(String(v.At("request"))); op.requestKey=String(v.At("request_key")); op.phase="accepted";
                if(handoffFenced) {
                    if(op.request.command!="complete-data-handoff")throw std::runtime_error("ECC journal contains a new command after control handoff intent");
                    CheckHandoffCompletion(op.request,true);
                } else if(op.request.command=="complete-data-handoff")throw std::runtime_error("ECC completion lacks an earlier original handoff intent");
                if (op.request.operationId!=id || op.request.instanceId!=replayInstance || op.request.snapshotSha256!=snapshot || op.request.clientId!=config.ClientId() || op.request.clientEpoch!=ownerEpoch ||
                    op.requestKey!=EccControlRequestKey(op.request) || !jobs.emplace(id,op).second) throw std::runtime_error("ECC network intent binding differs");
                operationOrder.push_back(id);
                if (jobs.size()>8192) throw std::runtime_error("ECC network operation budget exceeded in journal");
                requestProofs.emplace(id,std::make_shared<EccControlRequestProof>(directory+"/operations/"+id,op.request,true));
                if ((op.request.command=="handoff-data-control" || op.request.command=="retire-data-plans")) {handoffFenced=true;handoffOperationId=id;}
                if(op.request.command=="complete-data-handoff")handoffCompletionOperationId=id;
                continue;
            }
            auto found=jobs.find(id); if (found==jobs.end() || String(v.At("request_key"))!=found->second.requestKey) throw std::runtime_error("ECC network result lacks bound intent");
            auto &op=found->second;
            if (event=="running" && op.phase=="accepted") { ExactSupervisorFields(v,{"schema","sequence","snapshot_sha256","event","operation_id","request_key"}); op.phase=event; continue; }
            if ((event=="complete" || event=="failed" || event=="unknown") && (op.phase=="accepted" || op.phase=="running")) {
                std::set<std::string> fields={"schema","sequence","snapshot_sha256","event","operation_id","request_key","message","result"};
                const bool stoppedAlarmRelease=event=="complete" && op.request.command=="release-stopped-alarm";
                const bool preparationAlarmRecovery=event=="complete" && op.request.command=="reconcile-preparation-with-alarm";
                const bool alarmRecovery=event=="complete" && (op.request.command=="reconcile-acquisition-with-alarm" || preparationAlarmRecovery);
                const bool acquisitionRecovery=(alarmRecovery && !preparationAlarmRecovery) || (event=="complete" && (op.request.command=="reconcile-acquisition" || op.request.command=="reconcile-acquisition-with-sources"));
                if(acquisitionRecovery){fields.insert("receiver_policy_reset");fields.insert("receiver_stop_operation_id");}
                if(alarmRecovery || stoppedAlarmRelease){fields.insert("alarm_policy_reset");fields.insert("alarm_stop_operation_id");fields.insert("alarm_observation_sha256");}
                ExactSupervisorFields(v,fields);op.phase=event;op.message=String(v.At("message"));op.resultJson=String(v.At("result"));ParseStrictJson(op.resultJson);
                if(stoppedAlarmRelease) {
                    const auto digest=CheckStoppedAlarmBinding(op.request,true);
                    const auto state=DecodeEccControlControllerStatus(op.resultJson);
                    if(!Boolean(v.At("alarm_policy_reset")) || String(v.At("alarm_stop_operation_id"))!=alarmOperationId || String(v.At("alarm_observation_sha256"))!=digest ||
                       state.state!=2 || state.transition || state.outcomeUnknown || state.needsReconciliation || state.operationId!=op.request.operationId)
                        throw std::runtime_error("Stopped alarm release lacks original acknowledgement and verified Described result");
                    alarmHandled=false;alarmOperationId.clear();alarmObservation="null";alarmPhase="watching";
                }
                if(acquisitionRecovery || preparationAlarmRecovery) {
                    const auto state=DecodeEccControlControllerStatus(op.resultJson);
                    const bool reset=acquisitionRecovery && Boolean(v.At("receiver_policy_reset"));
                    if((alarmHandled && !alarmRecovery) || reset!=receiverHandled || (acquisitionRecovery && String(v.At("receiver_stop_operation_id"))!=(reset ? receiverOperationId : "")) ||
                       state.state!=2 || state.transition || state.outcomeUnknown || state.needsReconciliation || state.operationId!=op.request.operationId)
                        throw std::runtime_error("Receiver recovery completion lacks original hold or verified target");
                    if(reset) {
                        JsonLimits limits;limits.stringBytes=1024*1024;
                        const auto held=ParseStrictJson(receiverEvidence,limits);
                        if(DecodeEccControlSystemStatus(String(held.At("bound_status"))).controller.operationId!=op.request.failedOperationId)
                            throw std::runtime_error("Receiver recovery replay original binding differs");
                    }
                    if(alarmRecovery) {
                        const auto digest=CheckAlarmRecoveryBinding(op.request,true);
                        if((!preparationAlarmRecovery && !reset) || !Boolean(v.At("alarm_policy_reset")) || String(v.At("alarm_stop_operation_id"))!=alarmOperationId ||
                           String(v.At("alarm_observation_sha256"))!=digest)
                            throw std::runtime_error("Alarm recovery completion lacks bound acknowledgement");
                        alarmHandled=false;alarmOperationId.clear();alarmObservation="null";alarmPhase="watching";
                    }
                    if(reset){receiverHandled=false;receiverOperationId.clear();receiverEvidence="null";receiverPhase="watching";}
                }
                continue;
            }
            throw std::runtime_error("Invalid ECC network journal transition");
        }
        needsReconciliation=existing || controller.Status().needsReconciliation || controller.Status().outcomeUnknown;
        if (existing) alarmHandled=true; // 重开不发现/重建旧订阅，也不重放旧告警停止。
        if (existing) receiverHandled=true;
        if(existing && config.AlarmStopEnabled())alarmPhase=alarmOperationId.empty() ? "startup-reconciliation" : "reopened-held";
        if(existing && config.ReceiverStopEnabled())receiverPhase=receiverOperationId.empty() ? "startup-reconciliation" : "reopened-held";
        if (config.ReceiverStopEnabled() && !existing) receiverPhase="watching";
        else if (config.ReceiverStopEnabled() && receiverPhase=="disabled") receiverPhase="startup-reconciliation";
        if (config.AlarmStopEnabled() && !existing) alarmPhase="watching";
        else if (config.AlarmStopEnabled() && alarmPhase=="disabled") alarmPhase="startup-reconciliation";
        for (auto &entry:jobs) if (entry.second.phase=="accepted" || entry.second.phase=="running") { entry.second.phase="unknown"; entry.second.message="Service reopened unfinished intent; no replay"; }
        Record("opened",",\"instance_id\":"+EncodeJsonString(instance),inputFault.load());
    }
}
