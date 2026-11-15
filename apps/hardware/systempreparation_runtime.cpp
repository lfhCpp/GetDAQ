#include "systempreparation_p.h"
#include "systempreparationauditcapture_p.h"
#include "cobolifecyclejournal.h"
#include "configurationreadback.h"
#include "hardwareclient.h"
#include <stdexcept>

namespace GetDAQ {
void SystemPreparationClient::Impl::Record(const std::string &row) {
    const std::uint64_t limit = 64*1024*1024;
    if (audit->Bytes() > limit || row.size()+1 > limit-audit->Bytes()) throw std::runtime_error("System preparation audit budget exceeded; no next command sent");
    audit->Append(row);
}

void SystemPreparationClient::Impl::Check() const{
    if (state == State::Failed) throw std::runtime_error("System preparation failed; explicit Stop/reconciliation required");
    coboLifecycle->RequirePersistenceHealthy();
    for (const auto &p : preparations) if (p) p->CheckInterlock();
}

void SystemPreparationClient::Impl::Execute(HardwareClient &hw,const std::vector<HardwareStep> &steps,const std::string &id,const char *stage) {
    const auto file = "hardware-"+std::to_string(sequence++)+".jsonl";
    const auto context = "\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"audit_file\":"+JsonString(file);
    Record("{\"phase\":\"hardware_begin\","+context+"}");
    hw.Execute(steps,directory+'/'+file,[this] { Check(); });
    Record("{\"phase\":\"hardware_complete\","+context+"}");
    if (state==State::Configuring && (std::string(stage)=="cobo-configure" || std::string(stage)=="mutant-configure")) {
        const auto evidence=DecodeConfigurationReadbackAudit(ReadSystemPreparationConfigureAudit(directory+'/'+file),hw.CompletedAuditChainSha256());
        const auto record="{\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+",\"audit_file\":"+JsonString(file)+
            ",\"evidence\":"+EncodeConfigurationReadbackEvidence(evidence)+"}";
        Record("{\"phase\":\"configure_readback_captured\",\"entry\":"+record+"}");
        configureReadbacks.push_back(record);
    }
}

void SystemPreparationClient::Impl::Fail() {
    state = State::Failed;
    if (audit) try { Record("{\"phase\":\"failed\",\"automatic_retry\":false,\"automatic_cleanup\":false,\"hardware_stop_sent\":false,\"production_ecc_state_committed\":false}"); } catch (...) {}
}

void SystemPreparationClient::Impl::SetCoBoLifecycle(std::size_t index,const CoBoLifecycleState next) {
    if(index>=options.cobos.size() || !coboLifecycle || index>=coboLifecycle->Size())
        throw std::logic_error("CoBo lifecycle target index is outside the fixed configuration");
    const auto &cobo=options.cobos[index];
    if(next==CoBoLifecycleState::Unknown)coboLifecycle->MarkUnknown(cobo.id,index);
    else coboLifecycle->Transition(cobo.id,index,next);
    try {Record("{\"phase\":\"cobo_lifecycle\",\"id\":"+JsonString(cobo.id)+",\"state\":"+JsonString(CoBoLifecycleStateName(next))+
                ",\"lifecycle_checkpoint\":"+EncodeCoBoLifecycleCheckpoint(coboLifecycle->PersistenceCheckpoint())+"}");}
    catch(...) {try {coboLifecycle->MarkUnknown(cobo.id,index);} catch(...) {} throw;}
}

void SystemPreparationClient::Impl::MarkCoBoUnknown(std::size_t index,const std::string &id) {
    if (index>=options.cobos.size() || options.cobos[index].id!=id) return;
    try { SetCoBoLifecycle(index,CoBoLifecycleState::Unknown); } catch (...) {}
}
}
