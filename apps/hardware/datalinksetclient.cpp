#include "datalinksetclient.h"
#include "hardwareaudit.h"
#include "hardwareclient.h"
#include <atomic>
#include <mutex>
#include <map>
#include <set>
#include <stdexcept>
namespace GetDAQ {
class DataLinkSetClient::Impl {
public:
    std::vector<DataSenderOptions> options;
    std::vector<std::shared_ptr<DataSenderClient>> senders;
    std::atomic<State> state{State::Fresh};
    std::mutex operation;
    std::unique_ptr<HardwareAudit> audit;
    std::string directory;
    void Fail() {
        state = State::Failed;
        try { audit->Append("{\"phase\":\"failed\",\"automatic_cleanup\":false,\"automatic_retry\":false,\"receiver_sealed\":false}"); } catch (...) {}
    }
    std::string Context(std::size_t i,const char *stage) {
        return "\"sender_id\":"+JsonString(options[i].senderId)+",\"stage\":"+JsonString(stage)+",\"audit_file\":"+JsonString(std::string(stage)+'-'+std::to_string(i)+".jsonl");
    }
};
DataLinkSetClient::DataLinkSetClient(const std::vector<DataSenderOptions> &options) : fImpl(new Impl) {
    ValidateDataSenderSet(options); fImpl->options = options;
    for (const auto &o : options) fImpl->senders.emplace_back(std::unique_ptr<DataSenderClient>(new DataSenderClient(o)));
}
DataLinkSetClient::~DataLinkSetClient() {}
void DataLinkSetClient::BindIceReceiverSessions(const std::vector<IceReceiverSessionBinding> &bindings) {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operation);
    if(p.state!=State::Fresh) throw std::logic_error("ICE set session binding requires Fresh");
    std::map<std::string,std::string> sessions;std::set<std::string> expected;
    for(const auto &options:p.options) if(options.protocol=="ICE") expected.insert(options.senderId);
    for(const auto &binding:bindings) {
        if(!expected.count(binding.senderId) || binding.sessionId.size()!=64 || binding.sessionId.find_first_not_of("0123456789abcdef")!=std::string::npos || !sessions.emplace(binding.senderId,binding.sessionId).second)
            throw std::invalid_argument("ICE receiver session set has unknown/duplicate ID or malformed session");
    }
    if(sessions.size()!=expected.size()) throw std::invalid_argument("ICE receiver session set does not cover every ICE sender");
    for(const auto &sender:p.senders) if(!sender->Snapshot().receiverSession.empty()) throw std::logic_error("ICE session set was already bound; no replacement");
    for(const auto &entry:sessions) Sender(entry.first).BindIceReceiverSession(entry.second);
}
void DataLinkSetClient::Connect(const std::string &directory,const std::function<void()> &check) {
    auto &p = *fImpl; std::lock_guard<std::mutex> operation(p.operation);
    if (p.state != State::Fresh) throw std::logic_error("Data link set Connect requires Fresh; do not replay partial connections");
    // 第二/末尾节点失效必须在第一条connect前发现；这些RPC只有ice_isA。
    for (const auto &sender : p.senders) { if (check) check(); sender->Validate(); }
    if (check) check();
    CreateHardwareAuditDirectory(directory); p.directory = directory;
    p.audit.reset(new HardwareAudit(directory+"/data-links.jsonl"));
    p.audit->Append("{\"schema_version\":1,\"phase\":\"connect_intent\",\"senders\":"+std::to_string(p.senders.size())+
        ",\"control_lock_verified\":false,\"remote_stopped_verified\":false,\"receiver_sealed\":false}");
    p.state = State::Connecting;
    try {
        for (std::size_t i = 0; i < p.senders.size(); ++i) {
            const auto context = p.Context(i,"connect");
            if (check) check();
            p.audit->Append("{\"phase\":\"sender_begin\","+context+"}");
            p.senders[i]->Connect(directory+"/connect-"+std::to_string(i)+".jsonl",check);
            p.audit->Append("{\"phase\":\"sender_complete\","+context+"}");
        }
        if (check) check();
        p.audit->Append("{\"phase\":\"connected\",\"production_ecc_state_committed\":false,\"receiver_sealed\":false}");
        p.state = State::Connected;
    } catch (...) { p.Fail(); throw; }
}
void DataLinkSetClient::Disconnect() {
    auto &p = *fImpl; std::lock_guard<std::mutex> operation(p.operation);
    if (p.state != State::Connected) throw std::logic_error("Data link set Disconnect requires Connected; explicit recovery required");
    for (const auto &sender : p.senders) {
        const auto snapshot = sender->Snapshot();
        if (snapshot.state != DataSenderSnapshot::Connected && snapshot.state != DataSenderSnapshot::Stopped)
            throw std::logic_error("All data senders must be locally Connected/Stopped before Disconnect; stop hardware and flush first");
    }
    for (const auto &sender : p.senders) sender->Validate();
    try {
        p.audit->Append("{\"phase\":\"disconnect_intent\",\"receiver_sealed\":false}"); p.state = State::Disconnecting;
        for (std::size_t i = 0; i < p.senders.size(); ++i) {
            const auto context = p.Context(i,"disconnect"); p.audit->Append("{\"phase\":\"sender_begin\","+context+"}");
            p.senders[i]->Disconnect(p.directory+"/disconnect-"+std::to_string(i)+".jsonl");
            p.audit->Append("{\"phase\":\"sender_complete\","+context+"}");
        }
        p.audit->Append("{\"phase\":\"disconnected\",\"eof_verified\":false,\"receiver_sealed\":false,\"production_ecc_state_committed\":false}");
        p.state = State::Disconnected;
    } catch (...) { p.Fail(); throw; }
}
DataLinkSetClient::State DataLinkSetClient::GetState() const { return fImpl->state.load(); }
std::vector<std::shared_ptr<DataSenderClient>> DataLinkSetClient::ObservationClients() const {return fImpl->senders;}
DataSenderClient &DataLinkSetClient::Sender(const std::string &id) const {
    for (std::size_t i = 0; i < fImpl->options.size(); ++i) if (fImpl->options[i].senderId == id) return *fImpl->senders[i];
    throw std::invalid_argument("Sender ID is absent from data link set");
}
}
