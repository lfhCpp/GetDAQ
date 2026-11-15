#include "catalogtransfer.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/time.h>
#include <chrono>
#include <unistd.h>

namespace GetDAQ {
namespace {
const char *const kTransport="tcp-plain-v1";
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void SetSocketOptions(int fd) {
    timeval timeout={60,0};
    Require(!::setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)) && !::setsockopt(fd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout)),"Cannot set TCP inactivity timeout");
#ifdef SO_NOSIGPIPE
    const int noSignal=1;Require(!::setsockopt(fd,SOL_SOCKET,SO_NOSIGPIPE,&noSignal,sizeof(noSignal)),"Cannot suppress TCP SIGPIPE");
#endif
}
int Connect(const std::string &address,unsigned int port) {
    Require(!address.empty() && address.size()<=255 && port>0 && port<=65535,"Invalid TCP transfer endpoint");
    addrinfo hints={};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;
    addrinfo *results=nullptr;const auto service=std::to_string(port);const auto rc=::getaddrinfo(address.c_str(),service.c_str(),&hints,&results);
    if(rc)throw std::runtime_error(std::string("Cannot resolve TCP transfer address: ")+gai_strerror(rc));
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(15);
    int connected=-1;std::string lastError="connection timed out";
    for(auto *item=results;item;item=item->ai_next) {
        if(std::chrono::steady_clock::now()>=deadline)break;
        const int fd=::socket(item->ai_family,item->ai_socktype,item->ai_protocol);if(fd<0){lastError=std::strerror(errno);continue;}
        const int descriptorFlags=::fcntl(fd,F_GETFD),statusFlags=::fcntl(fd,F_GETFL);
        if(descriptorFlags<0 || statusFlags<0 || ::fcntl(fd,F_SETFD,descriptorFlags|FD_CLOEXEC) || ::fcntl(fd,F_SETFL,statusFlags|O_NONBLOCK)) {
            lastError=std::strerror(errno);::close(fd);continue;
        }
        int result=::connect(fd,item->ai_addr,item->ai_addrlen);
        if(result && errno==EINPROGRESS) {
            while(true) {
                const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
                if(remaining<=0){lastError="connection timed out";break;}
                struct pollfd event={fd,static_cast<short>(POLLOUT|POLLERR|POLLHUP),0};
                const int ready=::poll(&event,1,static_cast<int>(remaining));
                if(ready<0 && errno==EINTR)continue;
                if(ready<0){lastError=std::strerror(errno);break;}
                if(!ready){lastError="connection timed out";break;}
                int socketError=0;socklen_t errorLength=sizeof(socketError);
                if(::getsockopt(fd,SOL_SOCKET,SO_ERROR,&socketError,&errorLength)){lastError=std::strerror(errno);break;}
                if(!socketError)result=0;
                else lastError=std::strerror(socketError);
                break;
            }
        } else if(result)lastError=std::strerror(errno);
        if(!result) {
            if(::fcntl(fd,F_SETFL,statusFlags)) {lastError=std::strerror(errno);::close(fd);continue;}
            connected=fd;break;
        }
        ::close(fd);
    }
    ::freeaddrinfo(results);
    if(connected<0)throw std::runtime_error("Cannot connect to TCP transfer listener within 15 seconds: "+lastError);
    SetSocketOptions(connected);return connected;
}
int Listener(const std::string &address,unsigned int port,unsigned int &boundPort) {
    Require(!address.empty() && address.size()<=255 && port<=65535,"Invalid TCP transfer bind endpoint");
    addrinfo hints={};hints.ai_family=AF_UNSPEC;hints.ai_socktype=SOCK_STREAM;hints.ai_protocol=IPPROTO_TCP;hints.ai_flags=AI_PASSIVE;
    addrinfo *results=nullptr;const auto service=std::to_string(port);const auto rc=::getaddrinfo(address.c_str(),service.c_str(),&hints,&results);
    if(rc)throw std::runtime_error(std::string("Cannot resolve TCP bind address: ")+gai_strerror(rc));
    int listener=-1;
    for(auto *item=results;item;item=item->ai_next) {
        const int fd=::socket(item->ai_family,item->ai_socktype,item->ai_protocol);if(fd<0)continue;
        const int flags=::fcntl(fd,F_GETFD);
        if(flags<0 || ::fcntl(fd,F_SETFD,flags|FD_CLOEXEC) || ::bind(fd,item->ai_addr,item->ai_addrlen) || ::listen(fd,8)) {::close(fd);continue;}
        listener=fd;break;
    }
    ::freeaddrinfo(results);Require(listener>=0,"Cannot bind/listen on TCP transfer address");
    sockaddr_storage local={};socklen_t length=sizeof(local);Require(!::getsockname(listener,reinterpret_cast<sockaddr *>(&local),&length),"Cannot inspect TCP listener port");
    boundPort=local.ss_family==AF_INET ? ntohs(reinterpret_cast<sockaddr_in *>(&local)->sin_port) : ntohs(reinterpret_cast<sockaddr_in6 *>(&local)->sin6_port);
    return listener;
}
}

std::string SendReceiverCatalogOverTcp(const std::string &source,const std::string &address,unsigned int port,CatalogTransferProgress progress,bool resume) {
    SupervisorIO::Path(source);const int socket=Connect(address,port);SupervisorIO::Fd fd(socket);
    const auto receipt=SendReceiverCatalogStream(source,fd.value,fd.value,progress,resume,kTransport);
    const auto result=ParseStrictJson(receipt);const auto request=DecodeReceiverCatalogRequest(SupervisorIO::ReadFile(source+"/request.json",1024*1024));
    JsonLimits limits;limits.arrayItems=65536;const auto manifest=ParseStrictJson(SupervisorIO::ReadFile(source+"/manifest.json",16*1024*1024),limits);
    std::uint64_t expectedFiles=0,expectedBytes=0;
    for(const auto &sourceRow:manifest.At("sources").items)for(const auto &file:sourceRow.At("files").items) {
        ++expectedFiles;const auto bytes=SupervisorIO::Unsigned(file.At("bytes"),true);
        Require(expectedBytes<=UINT64_MAX-bytes,"Source archive byte count overflow");expectedBytes+=bytes;
    }
    Require(SupervisorIO::Boolean(result.At("complete")) && SupervisorIO::String(result.At("source_request_key"))==ReceiverCatalogRequestKey(request) &&
        SupervisorIO::Unsigned(result.At("copied_files"),true)==expectedFiles && SupervisorIO::Unsigned(result.At("copied_bytes"),true)==expectedBytes &&
        SupervisorIO::Boolean(result.At("content_verified")) && SupervisorIO::Boolean(result.At("source_preserved")),"TCP archive receipt scope differs");
    return receipt;
}

void ListenReceiverCatalogOverTcp(const std::string &bindAddress,unsigned int port,const std::string &destinationRoot,
                                  const std::function<void(unsigned int)> &ready,bool once) {
    SupervisorIO::Path(destinationRoot);SupervisorIO::Directory root(destinationRoot);(void)root;
    unsigned int actualPort=0;SupervisorIO::Fd listener(Listener(bindAddress,port,actualPort));if(ready)ready(actualPort);
    for(;;) {
        int accepted;
        do {accepted=::accept(listener.value,nullptr,nullptr);} while(accepted<0 && errno==EINTR);
        if(accepted<0)throw std::runtime_error(std::string("TCP transfer accept failed: ")+std::strerror(errno));
        SupervisorIO::Fd client(accepted);
        try {
            const int flags=::fcntl(client.value,F_GETFD);Require(flags>=0 && !::fcntl(client.value,F_SETFD,flags|FD_CLOEXEC),"Cannot protect accepted TCP descriptor");
            SetSocketOptions(client.value);
            const auto result=ReceiveReceiverCatalogStream(destinationRoot,client.value,client.value,false,kTransport,true);
            std::cerr<<"TCP archive complete: "<<result<<'\n';
            if(once)return;
        } catch(const std::exception &error) {
            std::cerr<<"TCP archive connection ended without completion: "<<error.what()<<'\n';
        }
    }
}
}
