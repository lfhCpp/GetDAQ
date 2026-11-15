#include "iceingress.h"
#include "iceingress_options.h"
#include "IceIngress.h"
#include "../datareceiver/netutil.h"
#include <Ice/Ice.h>
#include <atomic>
#include <chrono>
#include <csignal>
#include <iostream>
#include <mutex>
#include <stdexcept>
#include <unistd.h>

namespace {
volatile std::sig_atomic_t interrupted=0;
void Stop(int) {interrupted=1;}
class Servant : public GetDAQ::DataIngress {
public:
    Servant(GetDAQ::IceIngressRecorder &recorder,const std::string &session,std::size_t maximumChunk)
        : fRecorder(recorder),fSession(session),fMaximumChunk(maximumChunk) {}
    std::string streamSession(const Ice::Current &current) override {
        if(!current.con || current.con->type()!="tcp") throw std::invalid_argument("ICE source session discovery requires TCP");
        return fSession; // Does not bind a source, receive bytes or request completion.
    }
    void processData(const std::pair<const Ice::Byte *,const Ice::Byte *> &bytes,const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fInput);
        try {
            Connection(current);if(fRecorder.EndRequested()) {++fRejected;throw std::runtime_error("ICE payload after finishStream is not accepted");}
            const auto size=bytes.first==bytes.second ? 0 : std::size_t(bytes.second-bytes.first);
            if(size>fMaximumChunk) {++fRejected;throw std::runtime_error("ICE payload exceeds configured message budget");}
            ++fCalls;fRecorder.Feed(bytes.first,size);
        } catch(const std::exception &e) {fRecorder.Fail(e.what());throw;}
    }
    void finishStream(const std::string &session,Ice::Long expectedBytes,const Ice::Current &current) override {
        std::lock_guard<std::mutex> lock(fInput);
        if(session!=fSession) {++fRejected;throw std::invalid_argument("ICE finishStream session differs; no end accepted");}
        // A separate control connection is not an ordering barrier for data RPCs.
        if(fConnection && fConnection!=current.con) {++fRejected;throw std::invalid_argument("ICE finishStream must use the same data connection");}
        if(expectedBytes<0) {++fRejected;throw std::invalid_argument("ICE finishStream byte count must be nonnegative");}
        if(std::uint64_t(expectedBytes)!=fRecorder.ReceivedBytes()) {++fRejected;throw std::invalid_argument("ICE finishStream byte count differs; no end accepted");}
        Connection(current);fRecorder.EndStream(std::uint64_t(expectedBytes));
    }
    std::string Status() const {
        auto status=fRecorder.Status();status.resize(status.size()-2);
        return status+",\"bound_connections\":"+std::to_string(fBound.load())+",\"rpc_data_calls\":"+std::to_string(fCalls.load())+
            ",\"rejected_requests\":"+std::to_string(fRejected.load())+"}\n";
    }
private:
    void Connection(const Ice::Current &current) {
        if(!current.con || current.con->type()!="tcp") {++fRejected;throw std::invalid_argument("ICE ingress requires a real TCP connection");}
        if(fConnection && fConnection!=current.con) {++fRejected;throw std::invalid_argument("ICE ingress cannot mix multiple source connections");}
        if(!fConnection) {fConnection=current.con;fBound=1;fRecorder.BindSource();}
    }
    GetDAQ::IceIngressRecorder &fRecorder;
    std::string fSession;
    std::size_t fMaximumChunk;
    std::mutex fInput;
    Ice::ConnectionPtr fConnection;
    std::atomic<std::uint64_t> fBound{0},fCalls{0},fRejected{0};
};
void ServeStatus(int listener,const Servant &servant) {
    if(listener<0 || !GetDAQ::Readable(listener,20)) return;
    const int socket=::accept(listener,nullptr,nullptr);if(socket<0) return;
    try {
        GetDAQ::Nonblock(socket);char request[1024];std::size_t used=0;
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(200);
        while(used<sizeof(request) && std::chrono::steady_clock::now()<deadline) {
            if(!GetDAQ::Readable(socket,20)) continue;
            const auto bytes=::recv(socket,request+used,sizeof(request)-used,0);
            if(bytes<=0) break;
            used+=std::size_t(bytes);
            if(std::string(request,used).find("\r\n\r\n")!=std::string::npos) break;
        }
        const bool okay=std::string(request,used).find("GET /status HTTP/1.")==0;
        const std::string body=okay ? servant.Status() : "{\"error\":\"Only GET /status is supported\"}\n";
        const std::string response=std::string(okay ? "HTTP/1.1 200 OK\r\n" : "HTTP/1.1 404 Not Found\r\n")+
            "Content-Type: application/json\r\nConnection: close\r\nContent-Length: "+std::to_string(body.size())+"\r\n\r\n"+body;
        std::size_t offset=0;
        while(offset<response.size() && std::chrono::steady_clock::now()<deadline) {
#ifdef MSG_NOSIGNAL
            const int flags=MSG_NOSIGNAL;
#else
            const int flags=0; // main ignores SIGPIPE; macOS has no MSG_NOSIGNAL.
#endif
            const auto bytes=::send(socket,response.data()+offset,response.size()-offset,flags);
            if(bytes>0) offset+=std::size_t(bytes);else if(errno==EINTR) continue;else break;
        }
    } catch(...) {}
    ::close(socket);
}
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;int listener=-1;
    try {
        const auto options=GetDAQ::ParseIceIngressOptions(argc,argv);
        if(options.help) {GetDAQ::PrintIceIngressUsage(std::cout);return 0;}
        const GetDAQ::IceIngressConfig config=options.config;
        const std::string &bind=options.bind;
        const unsigned int port=options.port,statusPort=options.statusPort,messageMiB=options.messageMiB;
        std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);std::signal(SIGPIPE,SIG_IGN);std::signal(SIGXFSZ,SIG_IGN);
        GetDAQ::IceIngressRecorder recorder(config);
        Ice::InitializationData init;init.properties=Ice::createProperties();
        init.properties->setProperty("Ice.RetryIntervals","-1");init.properties->setProperty("Ice.ACM.Server","0");
        init.properties->setProperty("Ice.MessageSizeMax",std::to_string(messageMiB*1024));
        init.properties->setProperty("Ice.ThreadPool.Server.Size","1");init.properties->setProperty("Ice.ThreadPool.Server.SizeMax","1");
        init.properties->setProperty("Ice.ThreadPool.Server.Serialize","1");init.properties->setProperty("Ice.Warn.Dispatch","0");
        communicator=Ice::initialize(init);auto adapter=communicator->createObjectAdapterWithEndpoints("IceIngress","tcp -h "+bind+" -p "+std::to_string(port));
        IceUtil::Handle<Servant> servant=new Servant(recorder,config.sessionId,std::size_t(messageMiB)*1024*1024);
        adapter->add(servant,communicator->stringToIdentity("DataProcessor"));
        if(statusPort) listener=GetDAQ::Listen("127.0.0.1",statusPort);
        adapter->activate();std::cout<<servant->Status()<<std::flush;
        while(!interrupted && !recorder.Failed() && !recorder.EndRequested()) {
            if(listener>=0) ServeStatus(listener,*servant);else std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
        if(interrupted) recorder.Fail("ICE input interrupted; no confirmed source stream end");
        // Deactivation waits for outstanding dispatch, not for a missing source end.
        adapter->deactivate();adapter->waitForDeactivate();recorder.InputClosed();const int result=recorder.Finish();
        std::cout<<servant->Status();if(listener>=0) {::close(listener);listener=-1;}
        communicator->destroy();communicator=nullptr;return result;
    } catch(const std::exception &e) {
        if(listener>=0) ::close(listener);
        if(communicator) try {communicator->destroy();} catch(...) {}
        std::cerr<<e.what()<<'\n';return 2;
    }
}
