#ifndef GETDAQ_TEST_RECEIVER_OBSERVATION_BARRIER_H
#define GETDAQ_TEST_RECEIVER_OBSERVATION_BARRIER_H
#include "native_remote_service_fixture.h"
#include <condition_variable>
namespace GetDAQRemoteServiceFixture {
// Concurrent test-only HTTP relay: hold one genuine failed-child observation
// after the data service produced it; unrelated command/status requests proceed.
class ReceiverObservationBarrier {
    IO::Fd fListener;
    unsigned int fTarget;
    std::atomic<bool> fStopping{false},fArmed{false};
    std::atomic<bool> fTargetOffline{false};
    std::thread fAccept;
    std::vector<std::thread> fClients;
    std::mutex fMutex;
    std::condition_variable fCondition;
    bool fEntered=false,fReleased=false,fSent=false,fClientStillConnected=false;
    std::string fBody,fFailure;
    void Serve(int descriptor) {
        IO::Fd client(descriptor);
        try {
            timeval timeout={5,0};::setsockopt(client.value,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
            const auto request=ReadHttp(client.value);
            if(fTargetOffline) {SendBytes(client.value,"HTTP/1.1 503 Service Unavailable\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");return;}
            IO::Fd target(ConnectPort(fTarget));SendBytes(target.value,request);
            const auto response=ReadHttp(target.value);bool held=false;
            if (fArmed && request.compare(0,18,"GET /v1/receivers ")==0) {
                const auto body=response.substr(response.find("\r\n\r\n")+4);
                const auto value=GetDAQ::ParseStrictJson(body);
                const auto observed=GetDAQ::DecodeSupervisorObservation(value.At("observation"));
                bool failed=false;
                for (const auto &p:observed.receivers) failed=failed || ((p.state=="exited" || p.state=="reaped") && (p.exitCode>0 || p.signal>0));
                if (failed && fArmed.exchange(false)) {
                    std::unique_lock<std::mutex> lock(fMutex);fBody=body;fEntered=true;held=true;fCondition.notify_all();
                    if (!fCondition.wait_for(lock,std::chrono::seconds(5),[this] {return fReleased || fStopping.load();}))
                        throw std::runtime_error("Receiver observation test barrier not released");
                }
            }
            if (held) {
                char byte;const auto peek=::recv(client.value,&byte,1,MSG_PEEK|MSG_DONTWAIT);const int error=errno;
                {std::lock_guard<std::mutex> lock(fMutex);fClientStillConnected=peek<0 && (error==EAGAIN || error==EWOULDBLOCK);}
            }
            SendBytes(client.value,response);
            if (held) {std::lock_guard<std::mutex> lock(fMutex);fSent=true;fCondition.notify_all();}
        } catch (const std::exception &e) {std::lock_guard<std::mutex> lock(fMutex);if(fFailure.empty())fFailure=e.what();fCondition.notify_all();}
    }
public:
    ReceiverObservationBarrier(unsigned int port,unsigned int target) : fListener(::socket(AF_INET,SOCK_STREAM,0)),fTarget(target) {
        sockaddr_in a={};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
        Require(fListener.value>=0 && !::bind(fListener.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)) && !::listen(fListener.value,16),"Receiver barrier listen");
        ::fcntl(fListener.value,F_SETFD,FD_CLOEXEC);
        fAccept=std::thread([this] {
            while(!fStopping) {
                pollfd p={fListener.value,POLLIN,0};if(::poll(&p,1,25)<=0)continue;
                const auto client=::accept(fListener.value,nullptr,nullptr);if(client<0)continue;
                if(fClients.size()>=512) {::close(client);std::lock_guard<std::mutex> lock(fMutex);fFailure="Receiver barrier client budget exceeded";return;}
                fClients.emplace_back([this,client] {Serve(client);});
            }
        });
    }
    ~ReceiverObservationBarrier() {fStopping=true;Release();if(fAccept.joinable())fAccept.join();for(auto &thread:fClients)if(thread.joinable())thread.join();}
    void Arm() {fArmed=true;}
    void TargetOffline() {fTargetOffline=true;}
    void WaitEntered() {
        std::unique_lock<std::mutex> lock(fMutex);
        Require(fCondition.wait_for(lock,std::chrono::seconds(3),[this] {return fEntered || !fFailure.empty();}) && fEntered,"Receiver observation did not enter barrier");
    }
    void Release() {std::lock_guard<std::mutex> lock(fMutex);fReleased=true;fCondition.notify_all();}
    void WaitSent() {
        std::unique_lock<std::mutex> lock(fMutex);
        Require(fCondition.wait_for(lock,std::chrono::seconds(2),[this] {return fSent || !fFailure.empty();}) && fSent && fClientStillConnected,
            "Held receiver observation expired/was not delivered to the original pending client");
    }
    std::string Body() {std::lock_guard<std::mutex> lock(fMutex);return fBody;}
    std::string Failure() {std::lock_guard<std::mutex> lock(fMutex);return fFailure;}
};
}
#endif
