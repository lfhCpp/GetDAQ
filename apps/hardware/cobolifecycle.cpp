#include "cobolifecycle.h"
#include "cobolifecyclejournal.h"

#include <stdexcept>

namespace GetDAQ {
CoBoLifecycleLedger::CoBoLifecycleLedger(const std::vector<std::pair<std::string,bool>> &topology)
{
    if(topology.empty() || topology.size()>192)
        throw std::invalid_argument("CoBo lifecycle ledger requires 1..192 fixed entries");
    fEntries.reserve(topology.size());
    for(std::size_t i=0;i<topology.size();++i) {
        const auto &item=topology[i];
        if(item.first.empty() || !fIndexById.emplace(item.first,i).second)
            throw std::invalid_argument("CoBo lifecycle ledger IDs must be nonempty and unique");
        fEntries.push_back(Entry{item.first,item.second,InitialCoBoLifecycleState(item.second)});
    }
}

CoBoLifecycleLedger::~CoBoLifecycleLedger() = default;

void CoBoLifecycleLedger::BeginPersistence(const std::string &path,const std::string &scopeSha256)
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(fJournal || fTransitioned)throw std::logic_error("CoBo lifecycle persistence must begin before any transition");
    CoBoLifecycleJournal::Topology topology;
    for(const auto &entry:fEntries)topology.emplace_back(entry.id,entry.enabled);
    fJournal.reset(new CoBoLifecycleJournal(path,scopeSha256,topology));
}

void CoBoLifecycleLedger::RequirePersistenceHealthy() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(fJournal)fJournal->RequireHealthy();
}

CoBoLifecycleCheckpoint CoBoLifecycleLedger::PersistenceCheckpoint() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fJournal)throw std::logic_error("CoBo lifecycle has no durable journal checkpoint");
    return fJournal->Checkpoint();
}

std::size_t CoBoLifecycleLedger::Size() const { return fEntries.size(); }

const std::string &CoBoLifecycleLedger::IdAt(std::size_t index) const
{
    if(index>=fEntries.size()) throw std::out_of_range("CoBo lifecycle index is outside the fixed topology");
    return fEntries[index].id;
}

bool CoBoLifecycleLedger::EnabledAt(std::size_t index) const
{
    if(index>=fEntries.size()) throw std::out_of_range("CoBo lifecycle index is outside the fixed topology");
    return fEntries[index].enabled;
}

CoBoLifecycleState CoBoLifecycleLedger::StateAt(std::size_t index) const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(index>=fEntries.size()) throw std::out_of_range("CoBo lifecycle index is outside the fixed topology");
    return fEntries[index].state;
}

CoBoLifecycleState CoBoLifecycleLedger::StateFor(const std::string &id) const
{
    std::lock_guard<std::mutex> lock(fMutex);
    const auto found=fIndexById.find(id);
    if(found==fIndexById.end()) throw std::invalid_argument("Unknown CoBo lifecycle ID");
    return fEntries[found->second].state;
}

std::vector<CoBoLifecycleState> CoBoLifecycleLedger::Snapshot() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    std::vector<CoBoLifecycleState> states;
    states.reserve(fEntries.size());
    for(const auto &entry:fEntries) states.push_back(entry.state);
    return states;
}

void CoBoLifecycleLedger::RequireConfiguredForAcquisition() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    if(fJournal)fJournal->RequireHealthy();
    for(const auto &entry:fEntries) {
        const auto expected=entry.enabled ? CoBoLifecycleState::Configured : CoBoLifecycleState::Disabled;
        if(entry.state!=expected)
            throw std::logic_error("CoBo lifecycle must be Configured/Disabled before acquisition; "+entry.id+" is "+CoBoLifecycleStateName(entry.state));
    }
}

CoBoLifecycleLedger::Entry &CoBoLifecycleLedger::ValidateTransition(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next)
{
    if(fixedIndex>=fEntries.size() || fEntries[fixedIndex].id!=id)
        throw std::logic_error("CoBo lifecycle ID/index differs from the fixed topology");
    auto &entry=fEntries[fixedIndex];
    if((!entry.enabled && next!=CoBoLifecycleState::Disabled) || (entry.enabled && next==CoBoLifecycleState::Disabled))
        throw std::logic_error("CoBo lifecycle cannot cross the configured active/disabled boundary");
    if(!CanTransitionCoBoLifecycle(entry.state,next))
        throw std::logic_error("Illegal CoBo lifecycle transition from "+std::string(CoBoLifecycleStateName(entry.state))+" to "+CoBoLifecycleStateName(next));
    return entry;
}

CoBoLifecycleState CoBoLifecycleLedger::Transition(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next)
{
    std::lock_guard<std::mutex> lock(fMutex);
    auto &entry=ValidateTransition(id,fixedIndex,next);
    const auto previous=entry.state;
    fTransitioned=true;
    try {if(fJournal)fJournal->Append(id,fixedIndex,previous,next);}
    catch(...) {if(entry.enabled)entry.state=CoBoLifecycleState::Unknown;throw;}
    entry.state=next;
    return previous;
}

void CoBoLifecycleLedger::MarkUnknown(const std::string &id,std::size_t fixedIndex)
{
    std::lock_guard<std::mutex> lock(fMutex);
    auto &entry=ValidateTransition(id,fixedIndex,CoBoLifecycleState::Unknown);
    const auto previous=entry.state;
    entry.state=CoBoLifecycleState::Unknown; fTransitioned=true;
    if(fJournal)fJournal->Append(id,fixedIndex,previous,entry.state);
}

bool CoBoLifecycleLedger::TransitionForNegativeStop(const std::string &id,std::size_t fixedIndex,CoBoLifecycleState next)
{
    if(next!=CoBoLifecycleState::Stopping && next!=CoBoLifecycleState::Stopped && next!=CoBoLifecycleState::Unknown)
        throw std::logic_error("Negative CoBo lifecycle transition cannot grant positive control");
    std::lock_guard<std::mutex> lock(fMutex);
    auto &entry=ValidateTransition(id,fixedIndex,next);
    const auto previous=entry.state; fTransitioned=true;
    try {if(fJournal)fJournal->Append(id,fixedIndex,previous,next);}
    catch(...) {entry.state=CoBoLifecycleState::Unknown;return false;}
    entry.state=next;return true;
}

} // namespace GetDAQ
