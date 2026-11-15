#include "systemacquisition_p.h"
#include <stdexcept>
namespace GetDAQ {
SystemAcquisitionClient::SystemAcquisitionClient(SystemPreparationClient &prepared,const std::vector<DataSenderOptions> &senders)
    : fImpl(new Impl(prepared,prepared.CoBoLifecycleLedgerShared())) {
    auto &p = *fImpl;
    if (prepared.GetState() != SystemPreparationClient::State::Configured) throw std::logic_error("System acquisition requires the same Configured preparation object");
    p.coboLifecycle->RequireConfiguredForAcquisition();
    ValidateDataSenderSet(senders); std::map<std::string,DataSenderOptions> byId;
    for (const auto &s : senders) byId.emplace(s.senderId,s);
    std::vector<DataSenderOptions> ordered;
    const auto bind = [&](const std::string &id,unsigned int mask) {
        auto it = byId.find(id);
        if (it == byId.end() || it->second.circularBufferMask != mask || it->second.encodingMinor != p.options.encodingMinor)
            throw std::invalid_argument("System sender ID/activity/mask/encoding differs from prepared configuration");
        ordered.push_back(it->second); byId.erase(it);
    };
    for (const auto &c : p.options.cobos) {
        if (c.enabled) { p.plans.push_back(BuildCoBoAcquisitionPlan(c.configure)); bind(c.id,CompileCoBoConfigure(c.configure).readout.aget.asadMask); }
        else p.plans.emplace_back();
    }
    for (std::size_t i = 0; i < p.options.mutants.size(); ++i) {
        const auto &m = p.options.mutants[i]; if (!m.settings.isSlave) p.master = i;
        if (m.settings.readoutEnabled) bind(m.endpoint.id,1);
    }
    if (!byId.empty()) throw std::invalid_argument("Extra/inactive sender in prepared system binding");
    p.links.reset(new DataLinkSetClient(ordered));
    p.senderOptions=ordered;
}
SystemAcquisitionClient::~SystemAcquisitionClient() {}
void SystemAcquisitionClient::BindIceReceiverSessions(const std::vector<IceReceiverSessionBinding> &bindings) {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);
    if(p.state!=State::Configured) throw std::logic_error("ICE receiver binding requires Configured before ConnectData");
    p.links->BindIceReceiverSessions(bindings);
}
void SystemAcquisitionClient::ConnectData(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Configured) throw std::logic_error("ConnectData requires Configured");
    p.Check(); p.Begin(path,"connect-data"); p.state = State::Connecting;
    try {
        p.Preflight(true); p.linkDirectory=path+"/data-links";
        p.Record("{\"phase\":\"data_links_intent\",\"data_links_audit_directory\":"+JsonString(p.linkDirectory)+"}");
        p.links->Connect(p.linkDirectory,[&] { p.Check(); }); p.Check();p.LifecycleAll(CoBoLifecycleState::Ready);p.Complete("data-connected",State::Ready);
    }
    catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::Start(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    const bool restart=p.state==State::Stopped;
    if (p.state != State::Ready && !restart) throw std::logic_error("Start requires Ready/Stopped");
    p.Check(); p.Begin(path,"start"); p.state = State::Starting;
    try { p.StartNodes(false,restart); p.Complete("running",State::Running); } catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::Resume(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Paused) throw std::logic_error("Resume requires Paused");
    p.Check(); p.Begin(path,"resume"); p.state = State::Resuming;
    try { p.StartNodes(true); p.Complete("running",State::Running); } catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::Pause(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Running) throw std::logic_error("Pause requires Running");
    p.Begin(path,"pause"); p.state = State::Pausing;
    try { p.Quiesce(CoBoLifecycleState::Paused); p.Complete("paused-link-retained",State::Paused); } catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::Stop(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Running && p.state != State::Paused) throw std::logic_error("Stop requires Running/Paused; failed/unknown outcomes require recovery");
    const bool paused = p.state == State::Paused; p.Begin(path,"stop"); p.state = State::Stopping;
    try { if (paused) {p.Preflight(false);p.LifecycleAll(CoBoLifecycleState::Stopped);} else p.Quiesce(); p.Complete("stopped-link-retained",State::Stopped); } catch (...) { p.Fail(); throw; }
}
void SystemAcquisitionClient::DisconnectData(const std::string &path) {
    auto &p = *fImpl; std::lock_guard<std::mutex> lock(p.operation);
    if (p.state != State::Ready && p.state != State::Stopped) throw std::logic_error("DisconnectData requires Ready/Stopped");
    p.Begin(path,"disconnect-data"); p.state = State::Disconnecting;
    try { p.Preflight(false); p.links->Disconnect();p.LifecycleAll(CoBoLifecycleState::Configured);p.Complete("closed-receiver-seal-unverified",State::Closed); } catch (...) { p.Fail(); throw; }
}
SystemAcquisitionClient::State SystemAcquisitionClient::GetState() const { return fImpl->state.load(); }
std::string SystemAcquisitionClient::CoBoLifecycleJson() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.operation);std::string result="[";
    for(std::size_t i=0;i<p.options.cobos.size();++i){if(i)result+=',';result+="{\"id\":"+JsonString(p.options.cobos[i].id)+",\"state\":"+JsonString(CoBoLifecycleStateName(p.coboLifecycle->StateAt(i)))+"}";}
    return result+"]";
}
void SystemAcquisitionClient::Breakup(const std::string &path) {
    auto &p=*fImpl; std::lock_guard<std::mutex> lock(p.operation);
    const auto from=p.state.load();
    if (from!=State::Ready && from!=State::Running && from!=State::Paused && from!=State::Stopped && from!=State::Closed)
        throw std::logic_error("Breakup requires Ready/Running/Paused/Stopped/Closed; UNKNOWN must be reconciled");
    if (p.prepared.GetState()!=SystemPreparationClient::State::Configured)
        throw std::logic_error("Breakup requires the same Configured preparation object");
    p.Begin(path,"breakup"); p.state=State::Breaking;
    try {
        if (from==State::Running) p.Quiesce();
        p.Preflight(false);
        if (from!=State::Closed) p.links->Disconnect();
        p.Record("{\"phase\":\"all_sender_disconnect_confirmed\",\"data_links_audit_directory\":"+JsonString(p.linkDirectory)+
            ",\"eof_verified\":false,\"receiver_sealed\":false}");
        const auto lifecycleBefore=p.coboLifecycle->Snapshot();
        p.prepared.InvalidateConfiguration(path+"/configuration");
        p.RecordLifecycleMirror(lifecycleBefore);
        p.Complete("broken-prepare-snapshot-retained",State::Broken);
    } catch (...) {
        if (p.prepared.GetState()==SystemPreparationClient::State::Failed) {
        }
        p.Fail(); throw;
    }
}
DataSenderSnapshot SystemAcquisitionClient::SenderStatus(const std::string &id) const { return fImpl->links->Sender(id).Snapshot(); }
std::vector<std::shared_ptr<DataSenderClient>> SystemAcquisitionClient::ObservationClients() const {return fImpl->links->ObservationClients();}

}
