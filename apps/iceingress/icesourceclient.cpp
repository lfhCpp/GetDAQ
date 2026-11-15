#include "icesourceclient.h"
#include "IceIngress.h"
#include "../../common/text/utf8.h"
#include <Ice/Ice.h>
#include <algorithm>
#include <arpa/inet.h>
#include <limits>
#include <mutex>
#include <stdexcept>
namespace GetDAQ {
namespace {
void Session(const std::string &value) {
    if(value.size()!=64 || value.find_first_not_of("0123456789abcdef")!=std::string::npos) throw std::invalid_argument("ICE source session must be lowercase HEX64");
}
void Validate(const IceSourceOptions &o) {
    in_addr address;
    if(::inet_pton(AF_INET,o.address.c_str(),&address)!=1 || !o.port || o.port>65535 || o.encodingMinor>1 ||
       o.timeoutMs<100 || o.timeoutMs>600000 || !o.chunkBytes || o.chunkBytes>64*1024*1024-512)
        throw std::invalid_argument("Invalid bounded ICE source address/port/encoding/timeout/chunk");
    const auto host=ntohl(address.s_addr);
    if(!host || host==0xffffffffU || (host>>28)==14 || (host>>24)==0 || (host>>24)>=240) throw std::invalid_argument("ICE source requires a unicast IPv4 receiver");
    if(!o.expectedSession.empty()) Session(o.expectedSession);
}
}
class IceSourceClient::Impl {
public:
    explicit Impl(const IceSourceOptions &o) : options(o) {}
    IceSourceOptions options;
    Ice::CommunicatorPtr communicator;
    Ice::ConnectionPtr connection;
    mdaq::daq::DataProcessorPrx data;
    GetDAQ::DataIngressPrx ending;
    mutable std::mutex statusMutex;
    std::mutex operation;
    IceSourceSnapshot snapshot;
    IceSourceSnapshot Status() const {std::lock_guard<std::mutex> lock(statusMutex);return snapshot;}
    void State(IceSourceSnapshot::State state) {std::lock_guard<std::mutex> lock(statusMutex);snapshot.state=state;}
    void Unknown(const std::string &message) {
        std::lock_guard<std::mutex> lock(statusMutex);snapshot.state=IceSourceSnapshot::Unknown;snapshot.error=EccErrorText(message);
    }
};
IceSourceClient::IceSourceClient(const IceSourceOptions &o) : fImpl(new Impl(o)) {Validate(o);}
IceSourceClient::~IceSourceClient() {
    if(fImpl->communicator) try {fImpl->communicator->destroy();} catch(...) {}
}
void IceSourceClient::Connect() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operation);
    if(p.Status().state!=IceSourceSnapshot::Fresh) throw std::logic_error("ICE source Connect requires the same Fresh object; no replay/reconnect");
    p.State(IceSourceSnapshot::Connecting);
    try {
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.RetryIntervals","-1");
        init.properties->setProperty("Ice.Override.ConnectTimeout",std::to_string(p.options.timeoutMs));
        init.properties->setProperty("Ice.Override.Timeout",std::to_string(p.options.timeoutMs));
        init.properties->setProperty("Ice.MessageSizeMax","65536");
        p.communicator=Ice::initialize(init);Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=Ice::Byte(p.options.encodingMinor);
        auto proxy=p.communicator->stringToProxy("DataProcessor:tcp -h "+p.options.address+" -p "+std::to_string(p.options.port))
            ->ice_encodingVersion(encoding)->ice_collocationOptimized(false)->ice_twoway();
        if(!mdaq::daq::DataProcessorPrx::checkedCast(proxy)) throw std::runtime_error("ICE receiver does not implement original DataProcessor");
        if(!GetDAQ::DataIngressPrx::checkedCast(proxy)) throw std::runtime_error("ICE receiver lacks explicit source-end extension");
        p.connection=proxy->ice_getConnection();if(!p.connection || p.connection->type()!="tcp") throw std::runtime_error("ICE source lacks the checked TCP connection");
        auto fixed=p.connection->createProxy(proxy->ice_getIdentity())->ice_encodingVersion(encoding);
        p.ending=GetDAQ::DataIngressPrx::uncheckedCast(fixed->ice_twoway());
        const auto session=p.ending->streamSession();Session(session);
        if(!p.options.expectedSession.empty() && session!=p.options.expectedSession) throw std::runtime_error("ICE source expected session differs before sending bytes");
        p.data=mdaq::daq::DataProcessorPrx::uncheckedCast(fixed->ice_oneway());
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.sessionId=session;p.snapshot.state=IceSourceSnapshot::Connected;}
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
}
void IceSourceClient::Send(const std::uint8_t *data,std::size_t bytes) {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operation);
    if(p.Status().state!=IceSourceSnapshot::Connected) throw std::logic_error("ICE source Send requires Connected; UNKNOWN never retries");
    if(bytes && !data) throw std::invalid_argument("ICE source nonempty input requires a pointer");
    const auto before=p.Status();
    if(bytes>std::uint64_t(std::numeric_limits<Ice::Long>::max())-before.locallySentBytes) throw std::invalid_argument("ICE source stream exceeds signed 64-bit end counter");
    p.State(IceSourceSnapshot::Sending);
    try {
        while(bytes) {
            const auto count=std::min(bytes,p.options.chunkBytes);
            {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.issuedBytes+=count;++p.snapshot.issuedChunks;}
            p.data->processData(std::make_pair(reinterpret_cast<const Ice::Byte *>(data),reinterpret_cast<const Ice::Byte *>(data)+count));
            {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.locallySentBytes+=count;}
            data+=count;bytes-=count;
        }
        p.State(IceSourceSnapshot::Connected);
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
}
void IceSourceClient::Finish() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operation);const auto before=p.Status();
    if(before.state!=IceSourceSnapshot::Connected) throw std::logic_error("ICE source Finish requires Connected; no replay/automatic completion");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.state=IceSourceSnapshot::Finishing;p.snapshot.finishIssued=true;}
    try {
        p.ending->finishStream(before.sessionId,Ice::Long(before.locallySentBytes));
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.state=IceSourceSnapshot::Finished;p.snapshot.sourceEndAcknowledged=true;}
    } catch(const std::exception &e) {p.Unknown(e.what());throw;}
}
void IceSourceClient::RetireWithoutEnd() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operation);const auto before=p.Status();
    if(before.failureRetirementIssued || before.state==IceSourceSnapshot::Fresh || before.state==IceSourceSnapshot::Connecting || !p.connection)
        throw std::logic_error("Retirement requires the same original connected stream and no earlier attempt");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureRetirementIssued=true;p.snapshot.state=IceSourceSnapshot::Unknown;}
    try {
        p.connection->close(true); // Force-close: never sends finishStream or claims a complete stream.
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.dataConnectionClosed=true;}
    } catch(const std::exception &e){p.Unknown(e.what());throw;}
    catch(...){p.Unknown("Explicit failed stream close raised a nonstandard exception");throw;}
}
IceSourceSnapshot IceSourceClient::Status() const {return fImpl->Status();}
}
