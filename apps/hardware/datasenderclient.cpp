// Host control of the received CEA GET DaqCtrlNode interface. The original
// CeCILL source/interfaces remain in source/ and third_party/licenses/.
#include "datasenderclient_p.h"
#include "datasenderclient_validation_p.h"
#include "hardwareclient.h"
#include "strictjson.h"
#include <Ice/Ice.h>
#include <mdaq/DaqControl.h>
#include <algorithm>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
namespace DataSenderClientInternal {
Parser::Parser(int timeout) {
    Ice::InitializationData init; init.properties=Ice::createProperties();
    init.properties->setProperty("Ice.RetryIntervals","-1");
    init.properties->setProperty("Ice.Override.ConnectTimeout",std::to_string(timeout));
    init.properties->setProperty("Ice.Override.Timeout",std::to_string(timeout));
    init.properties->setProperty("Ice.MessageSizeMax","64");
    communicator=Ice::initialize(init);
}
Parser::~Parser() { if(communicator)try{communicator->destroy();}catch(...){} }
}

using DataSenderValidation::Scalars;
namespace {
using DataSenderClientInternal::Parser;
std::vector<std::string> NodeKeys(const Ice::ObjectPrx &base) {
    if (!base || base->ice_getRouter() || base->ice_getLocator() || !base->ice_getAdapterId().empty())
        throw std::invalid_argument("Data control requires direct endpoints without router/locator");
    std::vector<std::string> endpoints;
    for (const auto &e : base->ice_getEndpoints()) {
        const auto info = Ice::TCPEndpointInfoPtr::dynamicCast(e->getInfo());
        if (!info || info->host.empty() || info->port < 1 || info->port > 65535)
            throw std::invalid_argument("Data control requires explicit host/port TCP endpoints");
        auto host = info->host;
        std::transform(host.begin(),host.end(),host.begin(),[](unsigned char c) { return char(c >= 'A' && c <= 'Z' ? c-'A'+'a' : c); });
        // 超时、压缩和本地绑定选项不改变目标身份，不可用这些选项避开集合去重。
        endpoints.push_back(JsonString(host)+':'+std::to_string(info->port));
    }
    if (endpoints.empty() || endpoints.size() > 16) throw std::invalid_argument("Invalid bounded data control endpoint set");
    std::sort(endpoints.begin(),endpoints.end());
    endpoints.erase(std::unique(endpoints.begin(),endpoints.end()),endpoints.end());
    const auto id = base->ice_getIdentity();
    const std::string identity = JsonString(id.category)+JsonString(id.name)+JsonString(base->ice_getFacet());
    for (auto &e : endpoints) e = identity+std::to_string(e.size())+':'+e;
    return endpoints;
}

}
std::vector<std::string> CanonicalDataSenderNodeKeys(const std::string &proxy,int timeoutMs) {
    DataSenderClientInternal::Parser parser(timeoutMs);
    return NodeKeys(parser.communicator->stringToProxy(proxy));
}

DataSenderSnapshot DataSenderClient::Impl::Status() const { std::lock_guard<std::mutex> lock(statusMutex); return snapshot; }

DataSenderClient::DataSenderClient(const DataSenderOptions &o) : fImpl(new Impl) {
    Scalars(o); fImpl->options = o; fImpl->parser.reset(new DataSenderClientInternal::Parser(o.timeoutMs));
    auto base = fImpl->parser->communicator->stringToProxy(o.controlProxy); NodeKeys(base);
    Ice::EncodingVersion encoding; encoding.major = 1; encoding.minor = o.encodingMinor;
    fImpl->proxy = mdaq::DaqCtrlNodePrx::uncheckedCast(base->ice_twoway()->ice_collocationOptimized(false)->ice_timeout(o.timeoutMs)->ice_encodingVersion(encoding));
}
DataSenderClient::~DataSenderClient() {}
void DataSenderClient::Validate() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);
    p.Validate();
}
void DataSenderClient::BindIceReceiverSession(const std::string &session) {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto status=p.Status();
    if(p.options.protocol!="ICE" || status.state!=DataSenderSnapshot::Fresh || !status.receiverSession.empty()) throw std::logic_error("ICE session binding requires the same unbound Fresh sender");
    if(session.size()!=64 || session.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("ICE receiver session requires lowercase HEX64");
    std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.receiverSession=session;
}
void DataSenderClient::Connect(const std::string &path,const std::function<void()> &check) { fImpl->Change(DataSenderSnapshot::Connected,path,check); }
void DataSenderClient::Start(const std::string &path,const std::function<void()> &check) { fImpl->Change(DataSenderSnapshot::Running,path,check); }
void DataSenderClient::Stop(const std::string &path) { fImpl->Change(DataSenderSnapshot::Stopped,path,{}); }
void DataSenderClient::Disconnect(const std::string &path) { fImpl->Change(DataSenderSnapshot::Disconnected,path,{}); }
DataSenderSnapshot DataSenderClient::Snapshot() const { return fImpl->Status(); }
std::string DataSenderClient::CompletedCommandAuditChainSha256() const {
    std::lock_guard<std::mutex> lock(fImpl->operationMutex);
    if(fImpl->completedCommandChain.empty() || fImpl->Status().outcomeUnknown)
        throw std::logic_error("No confirmed sender command audit chain");
    return fImpl->completedCommandChain;
}

}
