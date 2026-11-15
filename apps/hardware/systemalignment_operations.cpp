#include "systemalignment_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace SystemAlignmentInternal;

std::string SystemAlignmentInternal::Reason(const std::exception &error) {
    const auto *alarm=dynamic_cast<const get::mutant::AlignmentAlarm *>(&error);
    const auto *command=dynamic_cast<const mdaq::utl::CmdException *>(&error);
    const std::string text=alarm ? alarm->reason : command ? command->reason : error.what();
    return text.size()>4096 ? "Remote error message omitted: exceeds 4096 bytes" : text;
}

std::string SystemAlignmentInternal::SlotJson(const std::vector<AlignmentSlotResult> &slots) {
    std::string json="[";
    for(const auto &slot:slots) {
        if(json.size()>1)json+=',';
        json+="{\"slot_index\":"+std::to_string(slot.slotIndex)+",\"amc_slot\":"+
            std::to_string(slot.slotIndex+1)+",\"present\":"+(slot.present ? "true" : "false")+
            ",\"delta_delay_ps\":"+std::to_string(slot.deltaDelayPs)+
            ",\"programmed_delay\":"+std::to_string(slot.programmedDelay)+"}";
    }
    return json+"]";
}

void SystemAlignmentClient::Impl::Record(const std::string &row) {
    if(audit->Bytes()>16*1024*1024 || row.size()+1>16*1024*1024-audit->Bytes())
        throw std::runtime_error("Alignment system audit exceeded 16 MiB; no next operation sent");
    audit->Append(row);
}

void SystemAlignmentClient::Impl::Command(const std::string &operation,const std::string &details,const std::function<void()> &call) {
    const std::string context="\"sequence\":"+std::to_string(sequence++)+",\"operation\":"+JsonString(operation)+details;
    Record("{"+context+",\"phase\":\"begin\"}");
    try { call(); }
    catch(const std::exception &error) {
        const auto why=Reason(error);
        try { Record("{"+context+",\"phase\":\"unknown\",\"error\":"+JsonString(why)+",\"automatic_retry\":false}"); } catch(...) {}
        throw std::runtime_error("Alignment/link command outcome UNKNOWN; stop without retry or restoration: "+why);
    }
    try { Record("{"+context+",\"phase\":\"acknowledged\",\"physical_effect_verified\":false}"); }
    catch(...) { throw std::runtime_error("Alignment/link command acknowledged but audit failed; do not repeat or restore automatically"); }
}

void SystemAlignmentClient::Impl::Link(std::size_t index,CoBoLinkMode mode) {
    Command("cobo-link",",\"cobo_id\":"+JsonString(options.cobos[index].id)+",\"mode\":"+std::to_string(unsigned(mode)),
        [this,index,mode] { links[index]->setupMutantLink(static_cast<get::cobo::MutantLinkMode>(mode)); });
}

void SystemAlignmentClient::Impl::Links(CoBoLinkMode mode,bool activeOnly) {
    for(std::size_t i=0;i<options.cobos.size();++i)if(!activeOnly || options.cobos[i].enabled)Link(i,mode);
}

std::vector<AlignmentSlotResult> SystemAlignmentClient::Impl::Slots(std::size_t module,AlignmentClock clock) {
    get::mutant::AlignmentResults wire;
    Command("align-delay-tdc",",\"mutant_id\":"+JsonString(options.mutants[module].id)+",\"clock\":"+std::to_string(unsigned(clock)),
        [this,module,clock,&wire] { servers[module]->alignDelayTDC(static_cast<get::mutant::Clock>(clock),false,wire); });
    if(wire.size()>12)throw std::runtime_error("Alignment result contains more than 12 slots; stop without restoration");
    std::vector<AlignmentSlotResult> result;
    for(const auto &entry:wire) {
        if(entry.first<0 || entry.first>11)throw std::runtime_error("Alignment result slot key is outside 0..11");
        AlignmentSlotResult slot;slot.slotIndex=unsigned(entry.first);slot.present=entry.second.isAmcPresent;
        slot.deltaDelayPs=entry.second.amcDeltaDelay;slot.programmedDelay=entry.second.amcProgDelay;result.push_back(slot);
    }
    Record("{\"phase\":\"slot_results\",\"mutant_id\":"+JsonString(options.mutants[module].id)+
        ",\"clock\":"+std::to_string(unsigned(clock))+",\"slots\":"+SlotJson(result)+"}");
    return result;
}

LocatedCoBo SystemAlignmentClient::Impl::Locate(std::size_t cobo,AlignmentClock clock) {
    Link(cobo,clock==AlignmentClock::Gmc ? CoBoLinkMode::GmcFeedback : CoBoLinkMode::WscaFeedback);
    std::vector<LocatedCoBo> present;
    for(std::size_t i=0;i<options.mutants.size();++i)for(const auto &slot:Slots(i,clock))if(slot.present) {
        LocatedCoBo board;board.coboId=options.cobos[cobo].id;board.mutantId=options.mutants[i].id;
        board.amcSlot=slot.slotIndex+1;present.push_back(board);
    }
    if(present.size()!=1)throw std::runtime_error("CoBo location must identify exactly one AMC slot; stop without restoration");
    Link(cobo,CoBoLinkMode::Normal);return present.front();
}

void SystemAlignmentClient::Impl::Wsca(std::size_t index) {
    const std::string name="hardware-"+std::to_string(hardwareSequence++)+".jsonl";
    Record("{\"phase\":\"hardware_begin\",\"mutant_id\":"+JsonString(options.mutants[index].id)+",\"audit_file\":"+JsonString(name)+"}");
    mutantHardware[index]->Execute(MutantWscaClock(true),directory+'/'+name);
    Record("{\"phase\":\"hardware_complete\",\"mutant_id\":"+JsonString(options.mutants[index].id)+",\"audit_file\":"+JsonString(name)+"}");
}

void SystemAlignmentClient::Impl::Multi(std::size_t index,AlignmentClock clock,unsigned int step,SystemAlignmentResult &result) {
    get::mutant::MultiShelveAlignmentResults wire;
    Command("multi-shelve-step",",\"mutant_id\":"+JsonString(options.mutants[index].id)+",\"clock\":"+
        std::to_string(unsigned(clock))+",\"step\":"+std::to_string(step),[this,index,clock,step,&wire] {
            const auto selected=static_cast<get::mutant::Clock>(clock);
            if(step==1)servers[index]->multiShelveAlignDelayStep1(selected,true,wire);
            else if(step==2)servers[index]->multiShelveAlignDelayStep2(selected,true,wire);
            else servers[index]->multiShelveAlignDelayStep3(selected,true,wire);
        });
    if(wire.comment.size()>4096)throw std::runtime_error("Alignment comment exceeds bounded result size");
    InterCrateAlignmentResult row;row.mutantId=options.mutants[index].id;row.clock=clock;row.step=step;
    row.measuredDelay=wire.measuredDelay;row.programmedDelay=wire.programmedDelay;row.comment=wire.comment;
    Record("{\"phase\":\"inter_crate_result\",\"mutant_id\":"+JsonString(row.mutantId)+",\"clock\":"+
        std::to_string(unsigned(clock))+",\"step\":"+std::to_string(step)+",\"measured_delay\":"+
        std::to_string(row.measuredDelay)+",\"programmed_delay\":"+std::to_string(row.programmedDelay)+
        ",\"comment\":"+JsonString(row.comment)+"}");
    result.interCrate.push_back(std::move(row));
}
}
