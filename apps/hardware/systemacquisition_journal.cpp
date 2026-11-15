#include "systemacquisition_p.h"
#include <stdexcept>
#include "cobolifecyclejournal.h"
#include "hardwareclient.h"

namespace GetDAQ {
SystemAcquisitionClient::Impl::Impl(SystemPreparationClient &p,std::shared_ptr<CoBoLifecycleLedger> lifecycle)
    : prepared(p),options(p.Configuration()),coboLifecycle(std::move(lifecycle)) {
    if(!coboLifecycle || coboLifecycle->Size()!=options.cobos.size())
        throw std::logic_error("System acquisition requires the preparation object's fixed CoBo lifecycle ledger");
}

void SystemAcquisitionClient::Impl::Lifecycle(std::size_t index,CoBoLifecycleState next) {
    if(index>=options.cobos.size() || !options.cobos[index].enabled || coboLifecycle->StateAt(index)==next)return;
    const auto &id=options.cobos[index].id;
    const auto from=coboLifecycle->StateAt(index);
    if(next==CoBoLifecycleState::Unknown)coboLifecycle->MarkUnknown(id,index);
    else coboLifecycle->Transition(id,index,next);
    try {Record("{\"phase\":\"cobo_lifecycle\",\"id\":"+JsonString(id)+",\"from\":"+
           JsonString(CoBoLifecycleStateName(from))+",\"state\":"+JsonString(CoBoLifecycleStateName(next))+
           ",\"lifecycle_checkpoint\":"+EncodeCoBoLifecycleCheckpoint(coboLifecycle->PersistenceCheckpoint())+"}");}
    catch(...) {try {coboLifecycle->MarkUnknown(id,index);} catch(...) {} throw;}
}

void SystemAcquisitionClient::Impl::NegativeLifecycle(std::size_t index,CoBoLifecycleState next) {
    const auto &id=options.cobos[index].id;
    const auto from=coboLifecycle->StateAt(index);
    bool durable=coboLifecycle->TransitionForNegativeStop(id,index,next);
    std::string checkpoint="null";
    if(durable) {
        try {checkpoint=EncodeCoBoLifecycleCheckpoint(coboLifecycle->PersistenceCheckpoint());}
        catch(...) {
            // A checkpoint read failure must not suppress explicit producer
            // stops. Withdraw durable evidence and retain UNKNOWN instead.
            durable=false;try {coboLifecycle->MarkUnknown(id,index);} catch(...) {}
        }
    }
    Record("{\"phase\":\"cobo_lifecycle\",\"id\":"+JsonString(id)+",\"from\":"+
           JsonString(CoBoLifecycleStateName(from))+",\"state\":"+JsonString(CoBoLifecycleStateName(coboLifecycle->StateAt(index)))+
           ",\"lifecycle_persisted\":"+(durable ? "true" : "false")+",\"lifecycle_checkpoint\":"+checkpoint+"}");
}

void SystemAcquisitionClient::Impl::LifecycleAll(CoBoLifecycleState next) {
    for(std::size_t i=0;i<options.cobos.size();++i)if(options.cobos[i].enabled)Lifecycle(i,next);
}

void SystemAcquisitionClient::Impl::RecordLifecycleMirror(const std::vector<CoBoLifecycleState> &before) {
    if(before.size()!=coboLifecycle->Size())throw std::logic_error("CoBo lifecycle mirror differs from fixed topology");
    for(std::size_t i=0;i<before.size();++i) {
        const auto after=coboLifecycle->StateAt(i);
        if(before[i]==after)continue;
        Record("{\"phase\":\"cobo_lifecycle\",\"id\":"+JsonString(options.cobos[i].id)+",\"from\":"+
               JsonString(CoBoLifecycleStateName(before[i]))+",\"state\":"+JsonString(CoBoLifecycleStateName(after))+
               ",\"lifecycle_checkpoint\":"+EncodeCoBoLifecycleCheckpoint(coboLifecycle->PersistenceCheckpoint())+"}");
    }
}

void SystemAcquisitionClient::Impl::Record(const std::string &row) {
    if (audit->Bytes()+row.size()+1 > 16*1024*1024) throw std::runtime_error("System run audit budget exceeded; no next command sent");
    audit->Append(row);
}

void SystemAcquisitionClient::Impl::Begin(const std::string &path,const char *action) {
    CreateHardwareAuditDirectory(path); directory = path; sequence = 0;
    audit.reset(new HardwareAudit(path+"/system-run.jsonl"));
    Record("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":"+JsonString(action)+
        ",\"configuration_sha256\":"+JsonString(options.configurationSha256)+",\"artifact_sha256\":"+JsonString(options.artifactSha256)+
        ",\"control_lock_verified\":false,\"actual_loaded_configuration_verified\":false,\"production_ecc_state_committed\":false,\"receiver_sealed\":false}");
}

void SystemAcquisitionClient::Impl::Complete(const char *stage,SystemAcquisitionClient::State target) {
    Record("{\"phase\":\"complete\",\"stage\":"+JsonString(stage)+",\"production_ecc_state_committed\":false,\"receiver_sealed\":false}"); state = target;
}

void SystemAcquisitionClient::Impl::Fail() {
    state = State::Failed;
    for(std::size_t i=0;i<coboLifecycle->Size();++i)
        if(coboLifecycle->StateAt(i)==CoBoLifecycleState::Starting || coboLifecycle->StateAt(i)==CoBoLifecycleState::Resuming || coboLifecycle->StateAt(i)==CoBoLifecycleState::Stopping)
            try {Lifecycle(i,CoBoLifecycleState::Unknown);} catch (...) {}
    try { Record("{\"phase\":\"failed\",\"automatic_cleanup\":false,\"automatic_retry\":false,\"receiver_sealed\":false}"); } catch (...) {}
}

std::string SystemAcquisitionClient::Impl::File(const std::string &id,const char *stage) {
    const auto file = "step-"+std::to_string(sequence++)+".jsonl";
    Record("{\"phase\":\"step_begin\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"audit_file\":"+JsonString(file)+"}");
    return directory+'/'+file;
}
}
