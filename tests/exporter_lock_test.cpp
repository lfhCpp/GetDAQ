#include "exporter.h"
#include <algorithm>
#include <arpa/inet.h>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <iostream>
#include <mutex>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace {
std::mutex gateMutex;
std::condition_variable gateChanged;
bool armed=false,entered=false,released=false;
std::atomic<bool> failAcceptOnce{false};
void Require(bool okay,const char *message) { if(!okay)throw std::runtime_error(message); }
void Release() { std::lock_guard<std::mutex> lock(gateMutex);released=true;gateChanged.notify_all(); }
struct GateCleanup { ~GateCleanup(){Release();} };
struct Socket {
    int fd;
    explicit Socket(int value):fd(value){Require(fd>=0,"socket failed");}
    ~Socket(){if(fd>=0)::close(fd);}
    void ResetClose() {
        linger reset={1,0};
        Require(::setsockopt(fd,SOL_SOCKET,SO_LINGER,&reset,sizeof(reset))==0,"set reset close");
        ::close(fd);fd=-1;
    }
};
template<class Predicate> bool Wait(Predicate ready,unsigned milliseconds=5000) {
    const auto end=std::chrono::steady_clock::now()+std::chrono::milliseconds(milliseconds);
    while(!ready()) {
        if(std::chrono::steady_clock::now()>=end)return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return true;
}
void Exact(int fd,const std::vector<uint8_t> &payload) {
    std::vector<uint8_t> bytes(payload.size()+8);std::size_t offset=0;
    while(offset<bytes.size()) {const auto n=::recv(fd,bytes.data()+offset,bytes.size()-offset,0);Require(n>0,"export incomplete");offset+=std::size_t(n);}
    uint64_t size=0;for(unsigned i=0;i<8;++i)size|=uint64_t(bytes[i])<<(8*i);
    Require(size==payload.size() && std::equal(payload.begin(),payload.end(),bytes.begin()+8),"legacy length header or payload changed");
}
}

extern "C" ssize_t __real_send(int,const void *,size_t,int);
extern "C" ssize_t __wrap_send(int fd,const void *data,size_t size,int flags) {
    {
        std::unique_lock<std::mutex> lock(gateMutex);
        if(armed) {armed=false;entered=true;gateChanged.notify_all();gateChanged.wait(lock,[]{return released;});}
    }
    return __real_send(fd,data,size,flags);
}
extern "C" int __real_accept(int,struct sockaddr *,socklen_t *);
extern "C" int __wrap_accept(int fd,struct sockaddr *address,socklen_t *length) {
    if(failAcceptOnce.exchange(false)) {errno=EIO;return -1;}
    return __real_accept(fd,address,length);
}

namespace {
unsigned ReservePort() {
    Socket reserve(::socket(AF_INET,SOCK_STREAM,0));sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    Require(::bind(reserve.fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))==0,"reserve port");
    socklen_t length=sizeof(address);Require(::getsockname(reserve.fd,reinterpret_cast<sockaddr *>(&address),&length)==0,"query port");return ntohs(address.sin_port);
}
void Connect(Socket &client,unsigned port) {
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=htons(port);
    Require(::connect(client.fd,reinterpret_cast<sockaddr *>(&address),sizeof(address))==0,"connect observer");
}
void VerifyWorkerFailureClosesClients() {
    const auto port=ReservePort();GetDAQ::Exporter exporter("127.0.0.1",port,16384);Socket client(::socket(AF_INET,SOCK_STREAM,0));
    Connect(client,port);Require(Wait([&]{return exporter.Clients()==1;}),"failure fixture observer not accepted");
    failAcceptOnce=true;Require(Wait([&]{return exporter.Failed();}),"injected exporter accept failure was not observed");
    Require(exporter.Clients()==0 && exporter.Disconnected()==1,"failed exporter did not retire its connected client");
    pollfd event={client.fd,POLLIN|POLLHUP,0};Require(::poll(&event,1,1000)>0,"failed exporter left client socket open");
    char byte=0;Require(::recv(client.fd,&byte,1,0)==0,"failed exporter did not send client EOF");
}

void VerifyOverBudgetFrameRetiresObserver() {
    const auto port=ReservePort();GetDAQ::Exporter exporter("127.0.0.1",port,1024);Socket client(::socket(AF_INET,SOCK_STREAM,0));
    Connect(client,port);Require(Wait([&]{return exporter.Clients()==1;}),"budget fixture observer not accepted");
    const std::vector<uint8_t> payload(1024,0x4d); // The length header makes this larger than the per-client budget.
    exporter.Publish(payload.data(),payload.size());
    Require(Wait([&]{return exporter.Clients()==0 && exporter.Disconnected()==1;}),"over-budget frame did not retire observer");
    Require(!exporter.Failed() && exporter.Bytes()==0,"over-budget observer caused output or worker failure");
}

void VerifyDisconnectedObserverCannotRaiseSigpipe() {
    {std::lock_guard<std::mutex> lock(gateMutex);armed=false;entered=false;released=false;}
    const auto port=ReservePort();GetDAQ::Exporter exporter("127.0.0.1",port,16384);GateCleanup cleanup;
    Socket client(::socket(AF_INET,SOCK_STREAM,0));Connect(client,port);
    Require(Wait([&]{return exporter.Clients()==1;}),"SIGPIPE fixture observer not accepted");
    const std::vector<uint8_t> payload(4096,0x71);
    {std::lock_guard<std::mutex> lock(gateMutex);armed=true;}
    exporter.Publish(payload.data(),payload.size());
    {std::unique_lock<std::mutex> lock(gateMutex);Require(gateChanged.wait_for(lock,std::chrono::seconds(5),[]{return entered;}),"SIGPIPE fixture send gate not reached");}
    client.ResetClose();
    Release();
    Require(Wait([&]{return exporter.Clients()==0;}),"reset observer was not disconnected");
    Require(!exporter.Failed(),"reset observer caused exporter worker failure");
}
}

int main() {
    try {
        const unsigned port=ReservePort();
        {
        GetDAQ::Exporter exporter("127.0.0.1",port,16384);GateCleanup cleanup;
        Socket client(::socket(AF_INET,SOCK_STREAM,0));
        timeval timeout={5,0};::setsockopt(client.fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        Connect(client,port);
        Require(Wait([&]{return exporter.Clients()==1;}),"observer not accepted");
        const std::vector<uint8_t> first(2048,0x35),second(3072,0xa6);
        {std::lock_guard<std::mutex> lock(gateMutex);armed=true;}
        exporter.Publish(first.data(),first.size());
        {std::unique_lock<std::mutex> lock(gateMutex);Require(gateChanged.wait_for(lock,std::chrono::seconds(5),[]{return entered;}),"send gate not reached");}
        std::atomic<bool> published{false};
        std::thread producer([&]{exporter.Publish(second.data(),second.size());published=true;});
        const bool independent=Wait([&]{return published.load();},500);
        Release();producer.join();
        Exact(client.fd,first);Exact(client.fd,second);
        Require(independent,"DAQ publication waited for the export socket call");
        Require(!exporter.Failed(),"export worker failed");
        }
        VerifyWorkerFailureClosesClients();
        VerifyOverBudgetFrameRetiresObserver();
        VerifyDisconnectedObserverCannotRaiseSigpipe();
        std::cout<<"PASS publication completed while export send was held; legacy headers and exact ordered payload preserved; over-budget observer retired; failed worker closed its clients; reset observer did not raise SIGPIPE; no raw files\n";
        return 0;
    } catch(const std::exception &error) {std::cerr<<error.what()<<'\n';return 1;}
}
