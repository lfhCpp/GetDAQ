#ifndef GETDAQ_NATIVE_RECEIVER_FIXTURE_H
#define GETDAQ_NATIVE_RECEIVER_FIXTURE_H
#include "nativeecccoordinator.h"
#include "configurationsha256.h"
#include "system_acquisition_fixture.h"
#include "supervisorio.h"
#include <arpa/inet.h>
#include <chrono>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
namespace GetDAQReceiverFixture {
using namespace GetDAQAcquisitionFixture;
namespace IO=GetDAQ::SupervisorIO;
inline unsigned int Port() {
    static unsigned int next=10000+(unsigned(::getpid())%160)*128;
    for (unsigned int attempt=0;attempt<128;++attempt) {
        const auto port=next++; IO::Fd fd(::socket(AF_INET,SOCK_STREAM,0)); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
        Require(fd.value>=0,"Native receiver port socket");
        if (!::bind(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a))) return port;
    }
    throw std::runtime_error("No native receiver fixture port in bounded range");
}
inline std::string Frame(unsigned int event) {
    std::string bytes(4096,char(event)); const unsigned char header[]={6,0,0,64,7,0,1,4,0,2,0,4};
    for (std::size_t i=0;i<sizeof(header);++i) bytes[i]=char(header[i]);
    bytes[12]=bytes[13]=0; bytes[14]=3; bytes[15]=static_cast<char>(224);
    for (unsigned int i=0;i<4;++i) bytes[22+i]=char((event>>(24-8*i))&255);
    bytes[26]=bytes[27]=0; return bytes;
}
class RawSender : public Sender {
    std::mutex mutex;
    IO::Fd socket;
    unsigned int event=0;
    std::string expected;
    std::string receiverTarget, protocol;
    void Open() {
        Require(socket.value<0,"Raw sender duplicated connection");
        const auto colon=receiverTarget.find(':');sockaddr_in address={};address.sin_family=AF_INET;
        address.sin_port=htons(unsigned(std::stoul(receiverTarget.substr(colon+1))));
        Require(::inet_pton(AF_INET,receiverTarget.substr(0,colon).c_str(),&address.sin_addr)==1,"Raw sender IPv4");
        socket.value=::socket(AF_INET,SOCK_STREAM,0);timeval timeout={2,0};
        Require(socket.value>=0 && !::setsockopt(socket.value,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)) &&
            !::connect(socket.value,reinterpret_cast<sockaddr *>(&address),sizeof(address)),"Raw sender actual connect");
        if (protocol=="FDT") Send(std::string(4,'\0'));
    }
    void CloseConnection() {
        if (truncate) Send(Frame(event+1).substr(0,100));
        else if (protocol=="FDT") Send(std::string(4,char(255)));
        Require(socket.value>=0 && !::shutdown(socket.value,SHUT_WR),"Raw sender actual EOF");
        ::close(socket.value);socket.value=-1;
    }
    void Send(const std::string &bytes) {
        std::size_t sent=0;
        while (sent<bytes.size()) {
            const auto n=::send(socket.value,bytes.data()+sent,std::min<std::size_t>(277,bytes.size()-sent),0);
            Require(n>0,"Actual raw fixture socket write"); sent+=std::size_t(n);
        }
    }
public:
    bool truncate=false;
    std::function<void()> onConnected;
    RawSender(World &w,const std::string &id) : Sender(w,id),socket(-1) {}
    void connect(const std::string &type,const std::string &target,const Ice::Current &current) override {
        Sender::connect(type,target,current); { std::lock_guard<std::mutex> lock(mutex);
        Require(socket.value<0,"Raw sender duplicated connection");receiverTarget=target;protocol=type;expected.clear();event=0;
        if (protocol!="FDT") Open(); }
        if (onConnected) onConnected();
    }
    void daqStart(const Ice::Current &current) override {
        Sender::daqStart(current); std::lock_guard<std::mutex> lock(mutex);
        if (protocol=="FDT") Open();
        for (unsigned int i=0;i<2;++i) { const auto bytes=Frame(++event); Send(bytes); expected+=bytes; }
    }
    void daqStop(const Ice::Current &current) override {
        Sender::daqStop(current);std::lock_guard<std::mutex> lock(mutex);
        if (protocol=="FDT") CloseConnection();
    }
    void disconnect(const Ice::Current &current) override {
        { std::lock_guard<std::mutex> lock(mutex); if (protocol!="FDT" || socket.value>=0) CloseConnection(); }
        Sender::disconnect(current); // 失去控制应答时，实际EOF已经送达独立receiver。
    }
    std::string Expected() { std::lock_guard<std::mutex> lock(mutex); return expected; }
    bool Connected() { std::lock_guard<std::mutex> lock(mutex); return socket.value>=0; }
    void FixtureClose() { std::lock_guard<std::mutex> lock(mutex); if (socket.value>=0) { ::shutdown(socket.value,SHUT_WR); ::close(socket.value); socket.value=-1; } }
};
inline NativeEccOptions Options(Rig &r,const std::string &root) {
    NativeEccOptions o; o.auditRoot=root+"/controller"; o.deploymentId="actual-receiver-fixture"; o.endpoints=r.options; o.senders=r.senders;
    SystemAlignmentOptions topology; topology.encodingMinor=r.options.encodingMinor; topology.timeoutMs=r.options.timeoutMs;
    topology.exclusiveOwnerConfirmed=topology.daqStoppedConfirmed=true;
    SystemExperimentArtifact artifact; artifact.profileSha256=GetDAQ::ConfigurationSha256("actual-receiver-test-profile");
    ExperimentConfigurationSource src; src.path="actual-receiver-test-settings"; src.sha256=GetDAQ::ConfigurationSha256(src.path); artifact.sources.push_back(src);
    for (const auto &c:r.options.cobos) {
        AlignmentCoBoEndpoint e; e.id=c.id; e.nodeProxy=c.prepare.nodeProxy; e.linkProxy=c.linkProxy; e.expectedNodeName=c.prepare.expectedNodeName;
        e.description=c.prepare.description; e.enabled=c.enabled; topology.cobos.push_back(e);
        SystemExperimentCoBo s; s.id=c.id; s.enabled=c.enabled; s.prepare=c.prepare.settings; s.configure=c.configure; artifact.settings.cobos.push_back(s);
    }
    o.description=BuildSystemDescriptionOptions(topology,DescriptionPolicy::UseExisting); o.experimentArtifactBytes=EncodeSystemExperimentArtifact(artifact); return o;
}
inline void State(NativeEccCoordinator &ecc,EccState state) {
    const auto status=ecc.Status(); Require(status.state==state && !status.outcomeUnknown && !status.needsReconciliation,"Native ECC state did not commit");
}
}
#endif
