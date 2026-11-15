#include "receiver_catalog_fixture.h"
#include "catalogtransfer.h"
#include "supervisorio.h"
#include <sys/resource.h>
#include <sys/wait.h>
#include <csignal>
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <spawn.h>
#include <sys/socket.h>
#include <dirent.h>
#include <limits.h>
#include <arpa/inet.h>
#include <poll.h>
#include <chrono>
#include <memory>
#include <thread>
#include <vector>
#include <cstring>
extern char **environ;
using namespace GetDAQCatalogFixture;
namespace {
void Fill(Fixture &fixture) {
    for(std::size_t i=0;i<fixture.options.stopped.receivers.size();++i)for(int j=0;j<2;++j) {
        const auto path=fixture.options.stopped.receivers[i].outputDirectory+"/data"+std::to_string(j)+".graw";Fd file(::open(path.c_str(),O_RDWR|O_CLOEXEC));struct stat info;Require(file.value>=0 && !::fstat(file.value,&info),"Open deterministic payload");
        std::array<unsigned char,65536> bytes;for(std::size_t k=0;k<bytes.size();++k)bytes[k]=static_cast<unsigned char>((k*17+i*31+j)%251);
        std::uint64_t written=0;while(written<std::uint64_t(info.st_size)) {const auto wanted=std::min<std::uint64_t>(bytes.size(),std::uint64_t(info.st_size)-written);const auto n=::write(file.value,bytes.data(),wanted);Require(n>0,"Write synthetic payload");written+=n;}Require(!::fsync(file.value),"Sync synthetic payload");
    }
}
std::string FileSha256(const std::string &path) {
    Fd file(::open(path.c_str(),O_RDONLY|O_CLOEXEC));Require(file.value>=0,"Open file for bounded streaming SHA-256");
    Sha256Stream hash;std::array<char,65536> bytes;
    while(true) {
        const auto count=::read(file.value,bytes.data(),bytes.size());
        if(count<0 && errno==EINTR)continue;
        Require(count>=0,"Read file for bounded streaming SHA-256");
        if(!count)break;
        hash.Update(bytes.data(),static_cast<std::size_t>(count));
    }
    return hash.Finish();
}
void StreamVectors() {
    const std::vector<std::string> values={"","abc",std::string(55,'a'),std::string(56,'a'),std::string(63,'b'),std::string(64,'c'),std::string(65,'d'),std::string(1000000,'a')};
    for(const auto &value:values)for(std::size_t step:{std::size_t(1),std::size_t(7),std::size_t(64),std::size_t(65536)}) {
        Sha256Stream hash;for(std::size_t at=0;at<value.size();at+=step)hash.Update(value.data()+at,std::min(step,value.size()-at));
        const auto digest=hash.Finish();Require(digest==ConfigurationSha256(value),"Streaming SHA differs from existing implementation");
        if(value=="abc")Require(digest=="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad","SHA abc oracle differs");
        if(value.size()==1000000)Require(digest=="cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0","SHA million a oracle differs");
        Reject([&] {hash.Update(nullptr,0);});Reject([&] {hash.Finish();});
    }
}
struct TcpListenerProcess {pid_t pid;unsigned int port;};
TcpListenerProcess StartTcpListener(const std::string &root) {
    int channel[2];Require(!::pipe(channel),"Create TCP listener readiness pipe");
    const auto child=::fork();Require(child>=0,"Fork isolated TCP listener");
    if(!child) {
        ::close(channel[0]);
        try {
            ListenReceiverCatalogOverTcp("127.0.0.1",0,root,[&](unsigned int port) {
                const std::uint32_t encoded=htonl(port);const auto count=::write(channel[1],&encoded,sizeof(encoded));
                if(count!=sizeof(encoded))throw std::runtime_error("Write TCP listener readiness");
            },true);
            ::close(channel[1]);::_exit(0);
        } catch(const std::exception &error) {std::cerr<<"TCP test listener failed: "<<error.what()<<'\n';::close(channel[1]);::_exit(1);}
    }
    ::close(channel[1]);std::uint32_t encoded=0;std::size_t offset=0;
    while(offset<sizeof(encoded)) {const auto count=::read(channel[0],reinterpret_cast<char *>(&encoded)+offset,sizeof(encoded)-offset);if(count<0 && errno==EINTR)continue;if(count<=0)break;offset+=static_cast<std::size_t>(count);}
    ::close(channel[0]);
    if(offset!=sizeof(encoded)) {int status=0;::waitpid(child,&status,0);throw std::runtime_error("TCP listener did not become ready");}
    const auto port=ntohl(encoded);Require(port>0,"TCP listener returned an invalid port");return {child,port};
}
struct CliChild {
    pid_t pid;
    explicit CliChild(pid_t child):pid(child){}
    ~CliChild(){Stop();}
    pid_t Release(){const auto child=pid;pid=-1;return child;}
    void Stop() {
        if(pid<=0)return;
        ::kill(pid,SIGTERM);
        for(unsigned int attempt=0;attempt<100;++attempt) {
            int status=0;const auto result=::waitpid(pid,&status,WNOHANG);
            if(result==pid || (result<0 && errno==ECHILD)){pid=-1;return;}
            if(result<0 && errno!=EINTR)break;
            ::usleep(10000);
        }
        ::kill(pid,SIGKILL);int status=0;while(::waitpid(pid,&status,0)<0 && errno==EINTR){}
        pid=-1;
    }
    int Wait(unsigned int timeoutSeconds,struct rusage *usage=nullptr) {
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(timeoutSeconds);
        while(true) {
            int status=0;const auto result=usage ? ::wait4(pid,&status,WNOHANG,usage) : ::waitpid(pid,&status,WNOHANG);
            if(result==pid){pid=-1;return status;}
            if(result<0 && errno==EINTR)continue;
            if(result<0){pid=-1;throw std::runtime_error("Wait for catalog CLI child failed");}
            if(std::chrono::steady_clock::now()>=deadline){Stop();throw std::runtime_error("Catalog CLI child timed out");}
            ::usleep(10000);
        }
    }
};
struct TcpCliListener {std::unique_ptr<CliChild> process;unsigned int port;};
TcpCliListener StartTcpCliListener(const std::string &catalog,const std::string &root) {
    int channel[2];Require(!::pipe(channel),"Create catalog CLI listener readiness pipe");
    const auto child=::fork();Require(child>=0,"Fork catalog CLI listener");
    if(!child) {
        ::close(channel[0]);
        if(::dup2(channel[1],STDOUT_FILENO)<0)::_exit(126);
        ::close(channel[1]);
        ::execl(catalog.c_str(),catalog.c_str(),"--listen-catalog-tcp","127.0.0.1","0",root.c_str(),"--once",static_cast<char *>(nullptr));
        ::_exit(127);
    }
    ::close(channel[1]);Fd output(channel[0]);CliChild process(child);std::string line;char bytes[256];
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(10);
    while(line.find('\n')==std::string::npos) {
        const auto remaining=std::chrono::duration_cast<std::chrono::milliseconds>(deadline-std::chrono::steady_clock::now()).count();
        Require(remaining>0,"Catalog CLI listener did not announce readiness in time");
        struct pollfd event={output.value,POLLIN,0};const auto ready=::poll(&event,1,static_cast<int>(remaining));
        if(ready<0 && errno==EINTR)continue;
        Require(ready>0,"Catalog CLI listener exited or did not become ready");
        const auto count=::read(output.value,bytes,sizeof(bytes));
        if(count<0 && errno==EINTR)continue;
        Require(count>0,"Catalog CLI listener closed its readiness stream");
        line.append(bytes,static_cast<std::size_t>(count));
        Require(line.size()<=8192,"Catalog CLI readiness line exceeded its limit");
    }
    const auto message=ParseStrictJson(line.substr(0,line.find('\n')));
    Require(message.At("phase").text=="listening" && message.At("port").type==JsonValue::Number,"Catalog CLI listener returned malformed readiness JSON");
    errno=0;char *end=nullptr;const auto parsed=std::strtoul(message.At("port").text.c_str(),&end,10);
    Require(!errno && end && !*end && parsed>0 && parsed<=65535,"Catalog CLI listener returned an invalid port");
    TcpCliListener listener;listener.process.reset(new CliChild(process.Release()));listener.port=static_cast<unsigned int>(parsed);return listener;
}
unsigned int ClosedLoopbackPort() {
    const int socket=::socket(AF_INET,SOCK_STREAM,0);Require(socket>=0,"Create closed-port probe socket");
    sockaddr_in address={};address.sin_family=AF_INET;address.sin_addr.s_addr=htonl(INADDR_LOOPBACK);address.sin_port=0;
    const bool bound=::bind(socket,reinterpret_cast<sockaddr *>(&address),sizeof(address))==0;
    socklen_t length=sizeof(address);const bool inspected=bound && ::getsockname(socket,reinterpret_cast<sockaddr *>(&address),&length)==0;
    const int closeResult=::close(socket);
    Require(inspected && !closeResult && ntohs(address.sin_port)>0,"Reserve and release a loopback port for refused-connect test");
    return ntohs(address.sin_port);
}
std::string RunCatalogCli(const std::string &catalog,const std::vector<std::string> &arguments,bool expectSuccess=true,
                          unsigned int timeoutSeconds=30,struct rusage *usage=nullptr) {
    int channel[2];Require(!::pipe(channel),"Create catalog CLI output pipe");Fd output(::open("/dev/null",O_RDONLY|O_CLOEXEC));
    Require(output.value>=0,"Open null device for CLI stdin");
    std::vector<std::string> words;words.push_back(catalog);words.insert(words.end(),arguments.begin(),arguments.end());
    std::vector<char *> argv;for(auto &word:words)argv.push_back(&word[0]);argv.push_back(nullptr);
    posix_spawn_file_actions_t actions;int code=::posix_spawn_file_actions_init(&actions);
    if(code){::close(channel[0]);::close(channel[1]);throw std::runtime_error("Initialize catalog CLI spawn actions failed");}
    code=::posix_spawn_file_actions_adddup2(&actions,output.value,STDIN_FILENO);
    if(!code)code=::posix_spawn_file_actions_adddup2(&actions,channel[1],STDOUT_FILENO);
    if(!code && !expectSuccess)code=::posix_spawn_file_actions_adddup2(&actions,channel[1],STDERR_FILENO);
    if(!code)code=::posix_spawn_file_actions_addclose(&actions,channel[0]);
    if(!code)code=::posix_spawn_file_actions_addclose(&actions,channel[1]);
    pid_t child=0;if(!code)code=::posix_spawn(&child,catalog.c_str(),&actions,nullptr,argv.data(),environ);
    ::posix_spawn_file_actions_destroy(&actions);::close(channel[1]);
    if(code){::close(channel[0]);throw std::runtime_error("Spawn catalog CLI failed: "+std::string(std::strerror(code)));}
    CliChild process(child);const auto status=process.Wait(timeoutSeconds,usage);Fd pipeOutput(channel[0]);std::string result;char bytes[4096];
    while(true){const auto count=::read(pipeOutput.value,bytes,sizeof(bytes));if(count<0 && errno==EINTR)continue;Require(count>=0,"Read catalog CLI output failed");if(!count)break;result.append(bytes,static_cast<std::size_t>(count));Require(result.size()<=1024*1024,"Catalog CLI output exceeded its limit");}
    const bool success=WIFEXITED(status) && WEXITSTATUS(status)==0;
    if(success!=expectSuccess)throw std::runtime_error("Catalog CLI exited with an unexpected status: "+result);
    return result;
}
std::uint64_t MaxRssKiB(const struct rusage &usage) {
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss)/1024;
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#endif
}
void PlainTcpCliLoopback(const std::string &catalog) {
    Fixture fixture;Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
    const auto remoteRoot=fixture.root+"/tcp-cli-remote";CreateDirectory(remoteRoot);
    auto listener=StartTcpCliListener(catalog,remoteRoot);
    const auto refusedAt=std::chrono::steady_clock::now();
    const auto refused=RunCatalogCli(catalog,{"--send-catalog-tcp",fixture.target,"127.0.0.1",std::to_string(ClosedLoopbackPort())},false);
    const auto refusedSeconds=std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-refusedAt).count();
    Require(refused.find("Cannot connect to TCP transfer listener within 15 seconds:")!=std::string::npos && refusedSeconds<5,
        "TCP CLI did not report a prompt, bounded connection refusal");
    const auto receipt=RunCatalogCli(catalog,{"--send-catalog-tcp",fixture.target,"127.0.0.1",std::to_string(listener.port)});
    const auto serverStatus=listener.process->Wait(30);
    Require(WIFEXITED(serverStatus) && WEXITSTATUS(serverStatus)==0,"TCP CLI listener failed after plain TCP send");
    const auto key=ReceiverCatalogRequestKey(fixture.options);const auto destination=remoteRoot+"/getdaq-transfer-"+key.substr(0,32);
    Require(VerifyTransferredCatalog(destination)==receipt && InspectReceiverCatalog(fixture.target).size()>0,"TCP CLI receipt/readback failed or source was changed");
}
void PlainTcpLoopback(const std::string &catalog) {
    Fixture fixture;Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
    const auto remoteRoot=fixture.root+"/tcp-remote";CreateDirectory(remoteRoot);
    auto listener=StartTcpListener(remoteRoot);
    const auto key=ReceiverCatalogRequestKey(fixture.options);const auto destination=remoteRoot+"/getdaq-transfer-"+key.substr(0,32);
    bool interrupted=false;
    Reject([&]{SendReceiverCatalogOverTcp(fixture.target,"127.0.0.1",listener.port,[&](const std::string &phase,std::uint64_t){
        if(phase=="file-sent" && !interrupted){interrupted=true;throw std::runtime_error("Interrupt plain TCP after first file");}
    });});
    Require(interrupted && Exists(destination+".partial") && !Exists(destination),"Interrupted plain TCP transfer did not retain only an incomplete partial");
    std::uint64_t resumedPayload=0;
    const auto receipt=SendReceiverCatalogOverTcp(fixture.target,"127.0.0.1",listener.port,[&](const std::string &phase,std::uint64_t bytes){
        if(phase=="file-sent")resumedPayload=bytes;
    },true);
    int status=0;const auto finishedListener=::waitpid(listener.pid,&status,0);
    Require(finishedListener==listener.pid && WIFEXITED(status) && WEXITSTATUS(status)==0,"Plain TCP resume listener did not finish cleanly");
    Require(VerifyTransferredCatalog(destination)==receipt && resumedPayload>0 && resumedPayload<24576,"Plain TCP resume failed independent verification or resent the whole payload");
    listener=StartTcpListener(remoteRoot);bool sentPayload=false;
    const auto resumed=SendReceiverCatalogOverTcp(fixture.target,"127.0.0.1",listener.port,[&](const std::string &phase,std::uint64_t){if(phase=="sending" || phase=="file-sent")sentPayload=true;},true);
    const auto resumedListener=::waitpid(listener.pid,&status,0);
    Require(resumedListener==listener.pid && WIFEXITED(status) && WEXITSTATUS(status)==0,"Completed plain TCP resume listener did not finish cleanly");
    Require(resumed==receipt && !sentPayload,"Completed plain TCP resume retransmitted payload or changed its receipt");
    if(!catalog.empty())PlainTcpCliLoopback(catalog);
}
void LargePlainTcpCliLoopback(const std::string &catalog,std::uint64_t fileBytes) {
    const std::uint64_t maximumAggregateRaw=16ULL*1024*1024*1024;
    Require(!catalog.empty() && fileBytes>0 && fileBytes<=(maximumAggregateRaw-16384)/4,
        "Large TCP test size must keep source and destination raw below the 16 GiB test budget");
    std::uint64_t aggregateRaw=0,largestRaw=0,parentRss=0,senderRss=0,receiverRss=0;
    {
        Fixture fixture(fileBytes);Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
        const auto remoteRoot=fixture.root+"/large-tcp-cli-remote";CreateDirectory(remoteRoot);
        auto listener=StartTcpCliListener(catalog,remoteRoot);
        struct rusage senderUsage={},receiverUsage={};
        const auto receipt=RunCatalogCli(catalog,{"--send-catalog-tcp",fixture.target,"127.0.0.1",std::to_string(listener.port)},true,1800,&senderUsage);
        const auto serverStatus=listener.process->Wait(1800,&receiverUsage);
        Require(WIFEXITED(serverStatus) && WEXITSTATUS(serverStatus)==0,"Large TCP CLI listener failed after transfer");
        const auto key=ReceiverCatalogRequestKey(fixture.options);const auto destination=remoteRoot+"/getdaq-transfer-"+key.substr(0,32);
        Require(VerifyTransferredCatalog(destination)==receipt,"Large TCP CLI target failed independent full-content verification");
        Require(InspectReceiverCatalog(fixture.target).size()>0,"Large TCP transfer changed or removed its source catalog");
        for(unsigned int source=0;source<2;++source)for(unsigned int file=0;file<2;++file) {
            const auto name="source_"+std::to_string(source)+"/data"+std::to_string(file)+".graw";
            struct stat sourceInfo,targetInfo;
            Require(!::stat((fixture.target+'/'+name).c_str(),&sourceInfo) && !::stat((destination+'/'+name).c_str(),&targetInfo) &&
                S_ISREG(sourceInfo.st_mode) && S_ISREG(targetInfo.st_mode) && sourceInfo.st_size==targetInfo.st_size,
                "Large TCP source/target file size differs");
            const auto bytes=static_cast<std::uint64_t>(sourceInfo.st_size);
            Require(bytes<=maximumAggregateRaw-aggregateRaw && bytes<=maximumAggregateRaw-aggregateRaw-bytes,
                "Large TCP source/destination raw exceeded the 16 GiB test budget");
            aggregateRaw+=2*bytes;largestRaw=std::max(largestRaw,bytes);
        }
        struct rusage parentUsage={};Require(!::getrusage(RUSAGE_SELF,&parentUsage),"Read large TCP test process peak memory");
        parentRss=MaxRssKiB(parentUsage);
        senderRss=MaxRssKiB(senderUsage);
        receiverRss=MaxRssKiB(receiverUsage);
        Require(aggregateRaw==4*fileBytes+16384,"Large TCP aggregate raw accounting differs from fixture layout");
        const auto parsed=ParseStrictJson(receipt);
        Require(Boolean(parsed.At("complete")),
            "Large TCP sender did not return a complete transfer receipt");
    }
    std::cout<<"PASS large ordinary-TCP catalog CLI transfer; aggregate_raw_bytes="<<aggregateRaw
        <<"; largest_raw_file_bytes="<<largestRaw<<"; test_max_rss_kib process="<<parentRss
        <<" sender="<<senderRss<<" receiver="<<receiverRss<<"; owned raw cleaned\n";
}
void PipeSignalThreadIsolation() {
    Fixture fixture;Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
    int channel[2];Require(!::socketpair(AF_UNIX,SOCK_STREAM,0,channel),"Create archive signal-isolation socket pair");
    struct sigaction before={};Require(!::sigaction(SIGPIPE,nullptr,&before),"Read process SIGPIPE disposition before archive stream");
    const char ready[]="GDAQRDY1";Require(::write(channel[1],ready,sizeof(ready)-1)==ssize_t(sizeof(ready)-1),"Write archive stream readiness tag");
    std::string failure;
    std::thread sender([&] {
        try {SendReceiverCatalogStream(fixture.target,channel[0],channel[0]);}
        catch(const std::exception &error) {failure=error.what();}
        ::close(channel[0]);
    });
    pollfd event={channel[1],POLLIN,0};const int readyForRead=::poll(&event,1,5000);char observed=0;
    const auto readCount=readyForRead>0 ? ::read(channel[1],&observed,1) : -1;
    struct sigaction during={};const bool dispositionRead=!::sigaction(SIGPIPE,nullptr,&during);
    const bool isolated=readCount==1 && dispositionRead && during.sa_handler==before.sa_handler;
    ::close(channel[1]);sender.join();
    Require(isolated && !failure.empty(),"Archive stream changed process-wide SIGPIPE handling or did not stop after its peer closed");
}
void OversizedTcpMetadataRejectedBeforeStaging() {
    Fixture fixture;
    const auto remoteRoot=fixture.root+"/bounded-tcp-root";
    CreateDirectory(remoteRoot);
    const std::uint32_t limits[]={1024*1024,16*1024*1024,65536,32*1024*1024};
    const char *const fields[]={"request","manifest","receipt","source checkpoint"};
    for(std::size_t oversized=0;oversized<4;++oversized) {
        int channel[2];Require(!::socketpair(AF_UNIX,SOCK_STREAM,0,channel),"Create bounded TCP metadata protocol pair");
        timeval timeout={5,0};
        const int timeoutResult=::setsockopt(channel[1],SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
        const auto timeoutFailure="Bound malformed metadata response wait: errno="+std::to_string(errno);
        Require(!timeoutResult,timeoutFailure.c_str());
        std::string serverError;
        std::thread server([&] {
            try { ReceiveReceiverCatalogStream(remoteRoot,channel[0],channel[0],false,"tcp-plain-v1",true); }
            catch(const std::exception &error) { serverError=error.what(); }
            ::close(channel[0]);
        });
        const auto exact=[&](void *buffer,std::size_t size) {
            auto *bytes=static_cast<char *>(buffer);std::size_t done=0;
            while(done<size) {const auto n=::read(channel[1],bytes+done,size-done);if(n<0 && errno==EINTR)continue;Require(n>0,"Read bounded metadata protocol response");done+=static_cast<std::size_t>(n);}
        };
        const auto send=[&](const void *buffer,std::size_t size) {
            const auto *bytes=static_cast<const char *>(buffer);std::size_t done=0;
            while(done<size) {const auto n=::write(channel[1],bytes+done,size-done);if(n<0 && errno==EINTR)continue;Require(n>0,"Send bounded metadata protocol probe");done+=static_cast<std::size_t>(n);}
        };
        std::string clientError,message;
        try {
            char tag[8];exact(tag,sizeof(tag));Require(std::string(tag,sizeof(tag))=="GDAQRDY1","TCP receiver did not send protocol readiness");
            send("GDAQNEW2",8);
            std::uint32_t sizes[]={htonl(0),htonl(0),htonl(0),htonl(0)};sizes[oversized]=htonl(limits[oversized]+1);
            send(sizes,sizeof(sizes));
            exact(tag,sizeof(tag));Require(std::string(tag,sizeof(tag))=="GDAQFAIL","TCP receiver did not reject oversized metadata");
            std::uint32_t encoded=0;exact(&encoded,sizeof(encoded));const auto messageSize=ntohl(encoded);
            Require(messageSize>0 && messageSize<=1024,"Bounded metadata rejection message exceeds protocol limit");
            message.resize(messageSize);exact(&message[0],message.size());
        } catch(const std::exception &error) { clientError=error.what(); }
        ::shutdown(channel[1],SHUT_RDWR);::close(channel[1]);server.join();
        if(!clientError.empty())throw std::runtime_error(std::string("TCP oversized ")+fields[oversized]+" boundary exchange failed: "+clientError);
        Require(serverError.find("metadata length exceeds limit")!=std::string::npos && message.find("metadata length exceeds limit")!=std::string::npos,
                "TCP receiver did not identify the metadata limit violation");
    }
    DIR *directory=::opendir(remoteRoot.c_str());Require(directory!=nullptr,"Inspect TCP root after malformed metadata");
    unsigned int entries=0;while(const auto *entry=::readdir(directory))if(std::string(entry->d_name)!="." && std::string(entry->d_name)!=".." && std::string(entry->d_name)!=".getdaq-catalog-transfer.lock")++entries;
    ::closedir(directory);
    Require(entries==0,"Malformed TCP metadata created archive staging content");
}
void RealSshLoopback() {
    const char *enabled=std::getenv("GETDAQ_CATALOG_TEST_REAL_SSH");if(!enabled || std::string(enabled)!="1")return;
    const char *destination=std::getenv("GETDAQ_CATALOG_TEST_SSH_TARGET");
    const char *remoteProgram=std::getenv("GETDAQ_CATALOG_TEST_REMOTE_PROGRAM");
    const char *remoteRoot=std::getenv("GETDAQ_CATALOG_TEST_REMOTE_ROOT");
    Require(destination && remoteProgram && remoteRoot,"Real SSH test is missing its isolated endpoint");
    const auto environmentBytes=[](const char *name,std::uint64_t fallback) {
        const char *value=std::getenv(name);if(!value)return fallback;
        errno=0;char *end=nullptr;const auto parsed=std::strtoull(value,&end,10);
        if(errno || end==value || *end || !parsed)throw std::invalid_argument(std::string("Invalid positive byte budget: ")+name);
        return static_cast<std::uint64_t>(parsed);
    };
    const std::uint64_t aggregateLimit=20ULL*1024ULL*1024ULL*1024ULL;
    const std::uint64_t largeFileBytes=environmentBytes("GETDAQ_CATALOG_TEST_REAL_SSH_FILE_BYTES",16384);
    const std::uint64_t payloadBudget=environmentBytes("GETDAQ_CATALOG_TEST_REAL_SSH_PAYLOAD_BUDGET_BYTES",1024*1024);
    if(payloadBudget>aggregateLimit)throw std::invalid_argument("Real SSH payload budget exceeds the 20-GiB aggregate test limit");
    if(largeFileBytes>(aggregateLimit-8192)/2)throw std::invalid_argument("Real SSH file size exceeds the 20-GiB aggregate test limit");
    std::uint64_t plannedBytes=0,payload=0;std::string fixtureRoot;
    {
        Fixture fixture(largeFileBytes);fixtureRoot=fixture.root;
        for(const auto &receiver:fixture.options.stopped.receivers) {
            if(receiver.receivedBytes>aggregateLimit-plannedBytes)throw std::invalid_argument("Real SSH fixture exceeds the 20-GiB aggregate test limit");
            plannedBytes+=receiver.receivedBytes;
        }
        Require(plannedBytes<=payloadBudget,"Real SSH fixture exceeds its configured payload budget before raw files are written");
        Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
        const auto receipt=SendReceiverCatalogOverSsh(fixture.target,destination,remoteProgram,remoteRoot);
        const auto key=ReceiverCatalogRequestKey(fixture.options);
        const auto target=std::string(remoteRoot)+"/getdaq-transfer-"+key.substr(0,32);
        Require(VerifyTransferredCatalog(target)==receipt && InspectReceiverCatalog(fixture.target).size()>0,
            "Real OpenSSH transfer did not verify the independent target or preserve its source");
        payload=Unsigned(ParseStrictJson(receipt).At("copied_bytes"),true);
        Require(payload<=payloadBudget && payload<=aggregateLimit,"Real OpenSSH fixture exceeded its configured payload budget");
        for(std::size_t i=0;i<fixture.options.stopped.receivers.size();++i)for(int j=0;j<2;++j) {
            const auto name="source_"+std::to_string(i)+"/data"+std::to_string(j)+".graw";
            struct stat sourceInfo,destinationInfo;
            Require(!::stat((fixture.target+'/'+name).c_str(),&sourceInfo) && !::stat((target+'/'+name).c_str(),&destinationInfo) &&
                    S_ISREG(sourceInfo.st_mode) && S_ISREG(destinationInfo.st_mode) && sourceInfo.st_size==destinationInfo.st_size &&
                    FileSha256(fixture.target+'/'+name)==FileSha256(target+'/'+name),
                "Real OpenSSH target payload differs from the sealed source");
        }
    }
    struct stat removedFixture;errno=0;
    Require(::lstat(fixtureRoot.c_str(),&removedFixture)!=0 && errno==ENOENT,"Real SSH source fixture directory was not removed");
    std::cout<<"PASS real OpenSSH loopback transfer; payload="<<payload<<" bytes; source_fixture_removed=true; source_raw_bytes="
        <<plannedBytes<<"; source_raw_files=4; source retained through verification; target verified\n";
}
}
int main(int argc,char **argv) {
    if(std::getenv("GETDAQ_CATALOG_TEST_FAKE_SSH")) {
        if(argc<2)::_exit(120);
        std::string command=argv[argc-1];const char *limit=std::getenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS");
        if(limit)command="trap '' XFSZ; ulimit -f "+std::string(limit)+"; "+command;
        ::execl("/bin/sh","sh","-c",command.c_str(),static_cast<char *>(nullptr));::_exit(121);
    }
    if(argc==4 && std::string(argv[1])=="--large-tcp-loopback") {
        errno=0;char *end=nullptr;const auto parsed=std::strtoull(argv[2],&end,10);
        if(errno || !end || *end || !parsed){std::cerr<<"FAIL invalid large TCP file size\n";return 2;}
        try {LargePlainTcpCliLoopback(argv[3],static_cast<std::uint64_t>(parsed));return 0;}
        catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
    }
    if(argc==4 && std::string(argv[1])=="--prepare-netns-catalog") {
        errno=0;char *end=nullptr;const auto parsed=std::strtoull(argv[3],&end,10);
        if(errno || !end || *end || !parsed || parsed>4*1024*1024) {std::cerr<<"FAIL invalid network-namespace catalog fixture size\n";return 2;}
        try {
            if(::setenv("GETDAQ_CATALOG_TEST_ROOT",argv[2],1))throw std::runtime_error("Cannot set private catalog fixture root");
            Fixture fixture(static_cast<std::uint64_t>(parsed));Fill(fixture);
            {ReceiverCatalog original(fixture.options);original.Execute();}
            fixture.owned.Preserve();
            std::cout<<fixture.target<<'\n';
            return 0;
        } catch(const std::exception &error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
    }
    try {
        StreamVectors();PipeSignalThreadIsolation();OversizedTcpMetadataRejectedBeforeStaging();RealSshLoopback();PlainTcpLoopback(argc==2 ? argv[1] : std::string());unsigned int cases=0;
        for(int fault=0;fault<3;++fault) {
            Fixture fixture(2*1024*1024);Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
            const auto target=fixture.root+"/resume-copy";bool interrupted=false;
            Reject([&]{CopyReceiverCatalog(fixture.target,target,[&](const std::string &phase,std::uint64_t){if(phase=="copying"){interrupted=true;throw std::runtime_error("Interrupt after first native chunk checkpoint");}});});
            Require(interrupted && !Exists(target+"/transfer-complete.json"),"Interrupted copy produced completion");
            const auto partial=target+"/source_0/data0.graw";struct stat before,after;Require(!::stat(partial.c_str(),&before) && before.st_size==1024*1024,"Missing one MiB partial prefix");
            if(fault) {
                const auto path=fault==1 ? partial : fixture.target+"/source_0/data0.graw";
                Fd data(::open(path.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC));char byte=0;Require(data.value>=0 && ::pread(data.value,&byte,1,17)==1,"Read fault byte");byte^=1;Require(::pwrite(data.value,&byte,1,17)==1,"Inject prefix/source mutation");
                Reject([&]{ResumeReceiverCatalog(fixture.target,target);});Require(!::stat(partial.c_str(),&after) && before.st_size==after.st_size && Same(before,after) && !Exists(target+"/transfer-complete.json"),"Rejected resume appended/truncated/replaced partial data");
            } else {
                const auto receipt=ResumeReceiverCatalog(fixture.target,target);
                Require(!::stat(partial.c_str(),&after) && Same(before,after) && after.st_size==2*1024*1024,"Resume replaced or failed to append original file");
                Require(VerifyTransferredCatalog(target)==receipt,"Resumed target failed independent complete verification");
                Require(ResumeReceiverCatalog(fixture.target,target)==receipt,"Already complete resume was not observation only");
                WriteNew(target+"/unexpected-data", "owned test entry");Reject([&]{ResumeReceiverCatalog(fixture.target,target);});Reject([&]{VerifyTransferredCatalog(target);});Require(!::unlink((target+"/unexpected-data").c_str()),"Remove only owned unexpected fixture entry");
                Require(!::unlink((target+"/resume-source.json").c_str()),"Remove only owned source checkpoint to model old format");Reject([&]{ResumeReceiverCatalog(fixture.target,target);});Require(VerifyTransferredCatalog(target)==receipt,"Old complete datasets lost independent verification compatibility");
            }
            ++cases;
        }
        {
            Fixture fixture(1024*1024);Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
            const auto sharedRoot=std::getenv("GETDAQ_TRANSFER_TEST_SHM_ROOT");OwnedTestDirectory other(sharedRoot ? std::string(sharedRoot)+"/transfer-" : "/dev/shm/getdaq-transfer-test-");
            struct stat left,right;Require(!::stat(fixture.target.c_str(),&left) && !::stat(other.Path().c_str(),&right) && left.st_dev!=right.st_dev,"Cross-filesystem fixture requires distinct st_dev");
            const auto target=other.Path()+"/copy";
            if(argc==2) {
                std::vector<std::string> words={argv[1],"--copy-catalog",fixture.target,target};std::vector<char *> args;for(auto &word:words)args.push_back(&word[0]);args.push_back(nullptr);pid_t child=0;
                Require(!::posix_spawn(&child,argv[1],nullptr,nullptr,args.data(),environ),"Spawn actual native catalog CLI");int code=0;Require(::waitpid(child,&code,0)==child && WIFEXITED(code) && WEXITSTATUS(code)==0,"Actual catalog CLI copy failed");
            } else CopyReceiverCatalog(fixture.target,target);
            const auto result=ReadFile(target+"/transfer-complete.json",65536);Require(VerifyTransferredCatalog(target)==result,"Independent cross-filesystem verification differs");
            Require(InspectReceiverCatalog(fixture.target).size()>0,"Original catalog not preserved");
            for(int i=0;i<2;++i)for(int j=0;j<2;++j) {
                const auto name="source_"+std::to_string(i)+"/data"+std::to_string(j)+".graw";struct stat src,dst;
                Require(!::stat((fixture.target+'/'+name).c_str(),&src) && !::stat((target+'/'+name).c_str(),&dst) && !Same(src,dst) && dst.st_nlink==1,"Transfer accidentally retained source inode alias");
                Require(ReadFile(fixture.target+'/'+name,2*1024*1024)==ReadFile(target+'/'+name,2*1024*1024),"Copied payload differs");
            }
            Require(!::rename(fixture.root.c_str(),(fixture.root+".offline").c_str()),"Temporarily hide owned original dataset");
            try {Require(VerifyTransferredCatalog(target)==result,"Copy still depended on original disk");}
            catch(...) {::rename((fixture.root+".offline").c_str(),fixture.root.c_str());throw;}
            Require(!::rename((fixture.root+".offline").c_str(),fixture.root.c_str()),"Restore owned source fixture");
            Reject([&] {CopyReceiverCatalog(fixture.target,target);});
            Fd corrupt(::open((target+"/source_0/data0.graw").c_str(),O_RDWR|O_CLOEXEC));const char changed='z';Require(corrupt.value>=0 && ::pwrite(corrupt.value,&changed,1,17)==1,"Corrupt one copied byte without changing size");
            Reject([&] {VerifyTransferredCatalog(target);});++cases;
        }
        {
            Fixture fixture(65536);Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
            const auto remoteRoot=fixture.root+"/remote archive root with spaces";Require(!::mkdir(remoteRoot.c_str(),0700),"Create private synthetic remote root");
            const char *remoteProgramSetting=std::getenv("GETDAQ_CATALOG_TEST_REMOTE_PROGRAM");const std::string remoteProgram=remoteProgramSetting ? remoteProgramSetting : argv[1];
            char fakeTemplate[]="/tmp/getdaq-catalog-fake-ssh-XXXXXX";char *fakeBinRaw=::mkdtemp(fakeTemplate);Require(fakeBinRaw,"Create isolated fake SSH executable directory");
            const auto fakeBin=std::string(fakeBinRaw),fakeSsh=fakeBin+"/ssh";char executable[PATH_MAX];Require(::realpath(argv[0],executable),"Resolve transfer test executable");
            Require(!::symlink(executable,fakeSsh.c_str()),"Create isolated fake OpenSSH executable");
            const char *previousPath=std::getenv("PATH");const std::string oldPath=previousPath ? previousPath : "";Require(!::setenv("PATH",(fakeBin+":"+oldPath).c_str(),1),"Select isolated fake SSH command");
            Require(!::setenv("GETDAQ_CATALOG_TEST_FAKE_SSH","1",1),"Set test-only fake SSH selector");
            std::string receipt;bool sawCompletion=false;std::uint64_t lastSent=0;
            try {
                receipt=SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,remoteRoot,[&](const std::string &phase,std::uint64_t bytes) {
                    lastSent=bytes;if(phase=="complete")sawCompletion=true;
                });
                Reject([&] {SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,remoteRoot);});
                const auto raceRoot=fixture.root+"/remote publish race";Require(!::mkdir(raceRoot.c_str(),0700),"Create private remote publication race root");
                const auto raceTarget=raceRoot+"/getdaq-transfer-"+ReceiverCatalogRequestKey(fixture.options).substr(0,32);bool createdByCompetitor=false;
                Reject([&] {SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,raceRoot,[&](const std::string &phase,std::uint64_t) {
                    if(phase=="remote-accepted" && !createdByCompetitor) {Require(!::mkdir(raceTarget.c_str(),0700),"Create competing publication destination");createdByCompetitor=true;}
                });});
                Require(createdByCompetitor && Exists(raceTarget) && !Exists(raceTarget+"/transfer-intent.json") && Exists(raceTarget+".partial/transfer-intent.json"),"Racing destination was replaced or partial evidence was lost");
            const auto key=ReceiverCatalogRequestKey(fixture.options);const auto target=remoteRoot+"/getdaq-transfer-"+key.substr(0,32);
            Require(sawCompletion,"Remote archive sender omitted its terminal progress event");
            const auto expectedSent=std::stoull(String(ParseStrictJson(receipt).At("copied_bytes")));
            if(lastSent!=expectedSent)throw std::runtime_error("Remote archive sender progress byte total differs: "+std::to_string(lastSent));
            Require(VerifyTransferredCatalog(target)==receipt && InspectReceiverCatalog(fixture.target).size()>0,"Remote archive completion/source preservation differs");
            for(std::size_t i=0;i<fixture.options.stopped.receivers.size();++i)for(int j=0;j<2;++j) {
                const auto name="source_"+std::to_string(i)+"/data"+std::to_string(j)+".graw";
                Require(ReadFile(fixture.target+'/'+name,65536)==ReadFile(target+'/'+name,65536),"Remote stream payload differs from the sealed source");
            }
            WriteNew(target+"/.transfer-complete.pending",receipt);
            Require(!::rename(target.c_str(),(target+".partial").c_str()) && !::unlink((target+".partial/transfer-complete.json").c_str()),"Model interruption during remote final publication");
            bool resentDuringPublicationRecovery=false;
            const auto publicationReceipt=ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,remoteRoot,[&](const std::string &phase,std::uint64_t) {
                if(phase=="sending")resentDuringPublicationRecovery=true;
            });
            Require(publicationReceipt==receipt && !resentDuringPublicationRecovery && VerifyTransferredCatalog(target)==receipt &&
                !Exists(target+"/.transfer-complete.pending"),"Interrupted final publication did not resume without payload retransmission");
            const auto ackRoot=fixture.root+"/remote lost-ack root";Require(!::mkdir(ackRoot.c_str(),0700),"Create lost-ack remote root");bool lostAck=false;
            Reject([&] {SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,ackRoot,[&](const std::string &phase,std::uint64_t) {
                if(phase=="complete"){lostAck=true;throw std::runtime_error("Simulated lost source-side final receipt");}
            });});Require(lostAck,"Lost-ack fixture did not interrupt after remote completion");bool resentAfterLostAck=false;
            const auto ackReceipt=ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,ackRoot,[&](const std::string &phase,std::uint64_t){if(phase=="sending" || phase=="file-sent")resentAfterLostAck=true;});
            Require(VerifyTransferredCatalog(ackRoot+"/getdaq-transfer-"+key.substr(0,32))==ackReceipt && !resentAfterLostAck,"Lost completion receipt caused payload retransmission");
            const auto resumableRoot=fixture.root+"/remote resumable root";Require(!::mkdir(resumableRoot.c_str(),0700),"Create private resumable remote root");
            Require(!::setenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS","33",1),"Set remote partial-write limit");
            Reject([&] {SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,resumableRoot);});
            ::unsetenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS");
            const auto resumableTarget=resumableRoot+"/getdaq-transfer-"+key.substr(0,32),resumablePartial=resumableTarget+".partial";
            const auto partialFile=resumablePartial+"/source_0/data0.graw";struct stat beforeMutation,afterMutation;
            Require(!::stat(partialFile.c_str(),&beforeMutation) && beforeMutation.st_size==16896 && beforeMutation.st_size%4096!=0 && Exists(resumablePartial+"/transfer-intent.json") && !Exists(resumablePartial+"/transfer-complete.json"),"Interrupted SSH transfer did not preserve its unaligned prefix and intent");
            Fd prefix(::open(partialFile.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC));char original=0;Require(prefix.value>=0 && ::pread(prefix.value,&original,1,17)==1,"Read SSH resume-prefix fixture");const char changed=char(original^1);Require(::pwrite(prefix.value,&changed,1,17)==1,"Mutate SSH resume-prefix fixture");
            Reject([&] {ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,resumableRoot);});
            Require(!::stat(partialFile.c_str(),&afterMutation) && afterMutation.st_size==beforeMutation.st_size && !Exists(resumablePartial+"/transfer-complete.json"),"Mismatched SSH prefix was appended or completed");
            Require(::pwrite(prefix.value,&original,1,17)==1 && !::fsync(prefix.value),"Restore SSH resume-prefix fixture");
            bool resumedSending=false;std::uint64_t resumedBytes=0;
            const auto resumedReceipt=ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,resumableRoot,[&](const std::string &phase,std::uint64_t bytes) {
                if(phase=="sending" || phase=="file-sent"){resumedSending=true;resumedBytes=bytes;}if(phase=="complete")resumedBytes=bytes;
            });
            Require(VerifyTransferredCatalog(resumableTarget)==resumedReceipt && resumedBytes==expectedSent-std::uint64_t(beforeMutation.st_size) && resumedSending,"SSH resume did not send only the missing suffixes");
            bool retransmitted=false;Require(ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,resumableRoot,[&](const std::string &phase,std::uint64_t){if(phase=="sending")retransmitted=true;})==resumedReceipt && !retransmitted,"Completed SSH resume retransmitted payload");
            const auto changedSourceRoot=fixture.root+"/remote changed-source root";Require(!::mkdir(changedSourceRoot.c_str(),0700),"Create changed-source test root");
            Require(!::setenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS","33",1),"Set changed-source partial-write limit");
            Reject([&] {SendReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,changedSourceRoot);});::unsetenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS");
            const auto changedSourceTarget=changedSourceRoot+"/getdaq-transfer-"+key.substr(0,32),changedSourcePartial=changedSourceTarget+".partial";
            const auto changedSourceFile=fixture.target+"/source_0/data0.graw";Fd sourceFile(::open(changedSourceFile.c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC));char sourceByte=0;
            Require(sourceFile.value>=0 && ::pread(sourceFile.value,&sourceByte,1,9)==1,"Read original source byte");const char alteredSourceByte=char(sourceByte^1);Require(::pwrite(sourceFile.value,&alteredSourceByte,1,9)==1 && !::fsync(sourceFile.value),"Mutate bound source after interrupted SSH transfer");
            Reject([&] {ResumeReceiverCatalogOverSsh(fixture.target,"daq@archive.example",remoteProgram,changedSourceRoot);});struct stat changedPrefix;
            Require(!::stat((changedSourcePartial+"/source_0/data0.graw").c_str(),&changedPrefix) && changedPrefix.st_size==16896 && !Exists(changedSourcePartial+"/transfer-complete.json"),"Changed original source was allowed to append to remote partial");
            Require(::pwrite(sourceFile.value,&sourceByte,1,9)==1 && !::fsync(sourceFile.value),"Restore source test byte");
            }
            catch(...) {if(previousPath)::setenv("PATH",oldPath.c_str(),1);else ::unsetenv("PATH");::unsetenv("GETDAQ_CATALOG_TEST_FAKE_SSH");::unsetenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS");::unlink(fakeSsh.c_str());::rmdir(fakeBin.c_str());throw;}
            if(previousPath)::setenv("PATH",oldPath.c_str(),1);else ::unsetenv("PATH");
            ::unsetenv("GETDAQ_CATALOG_TEST_FAKE_SSH");::unsetenv("GETDAQ_CATALOG_TEST_REMOTE_FILE_LIMIT_BLOCKS");Require(!::unlink(fakeSsh.c_str()) && !::rmdir(fakeBin.c_str()),"Remove fake SSH executable after test");
            ++cases;
        }
        {
            Fixture fixture;Fill(fixture);{ReceiverCatalog original(fixture.options);original.Execute();}
            for(int fault=0;fault<4;++fault) {
                const auto target=fixture.root+"/failed-copy-"+std::to_string(fault);bool injected=false;
                Reject([&] {CopyReceiverCatalog(fixture.target,target,[&](const std::string &phase,std::uint64_t) {
                    if(phase!="before-complete" || injected)return;
                    injected=true;
                    if(fault==0)throw std::runtime_error("Simulated interruption before complete");
                    const auto path=(fault==2 ? fixture.target : target)+"/source_0/data0.graw";
                    Fd file(::open(path.c_str(),O_RDWR|O_CLOEXEC));const char byte=fault==3 ? 'r' : 'q';Require(file.value>=0 && ::pwrite(file.value,&byte,1,2)==1,"Inject same-size source/target mutation");
                    if(fault==3) {
                        auto manifest=ReadFile(target+"/transfer-manifest.json",1024*1024);const auto parsed=ParseStrictJson(manifest);
                        const auto old=String(parsed.At("files").items[0].At("sha256")),newDigest=ConfigurationSha256(ReadFile(path,65536));
                        const auto at=manifest.find(old);Require(at!=std::string::npos,"Find target SHA fixture");manifest.replace(at,old.size(),newDigest);
                        Fd metadata(::open((target+"/transfer-manifest.json").c_str(),O_WRONLY|O_TRUNC|O_CLOEXEC));Require(metadata.value>=0 && ::write(metadata.value,manifest.data(),manifest.size())==ssize_t(manifest.size()),"Replace owned manifest with matching corrupt SHA");
                    }
                });});
                Require(injected && !Exists(target+"/transfer-complete.json") && Exists(target+"/transfer-intent.json"),"Failure fabricated completion or erased intent");Reject([&] {CopyReceiverCatalog(fixture.target,target);});++cases;
            }
        }
        {
            Fixture fixture;{ReceiverCatalog original(fixture.options);original.Execute();}
            Reject([&] {CopyReceiverCatalog(fixture.target,fixture.target+"/inside");});
            Reject([&] {CopyReceiverCatalog(fixture.target,fixture.options.stopped.receivers[0].outputDirectory+"/inside");});
            Fd live(::open((fixture.target+"/.getdaq-transfer.lock").c_str(),O_RDWR|O_CREAT|O_CLOEXEC,0600));Require(live.value>=0 && !::flock(live.value,LOCK_EX|LOCK_NB),"Hold transfer source lease");
            const auto target=fixture.root+"/conflicting-copy";Reject([&] {CopyReceiverCatalog(fixture.target,target);});Require(!Exists(target),"Conflicting transfer created a target");++cases;
        }
        {
            Fixture fixture(131072);{ReceiverCatalog original(fixture.options);original.Execute();}const auto target=fixture.root+"/limited-copy";const auto child=::fork();Require(child>=0,"Fork isolated file-limit fixture");
            if(!child) {
                ::signal(SIGXFSZ,SIG_IGN);struct rlimit limit={16384,16384};if(::setrlimit(RLIMIT_FSIZE,&limit))::_exit(2);
                try {CopyReceiverCatalog(fixture.target,target);::_exit(3);}catch(...) {::_exit(Exists(target+"/transfer-complete.json") ? 4 : 0);}
            }
            int status=0;Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0,"Short/failed write did not preserve incomplete transfer");
            Require(Exists(target+"/transfer-intent.json") && !Exists(target+"/transfer-complete.json"),"File-limit failure erased evidence or completed");++cases;
            struct stat prefix,completed;Require(!::stat((target+"/source_0/data0.graw").c_str(),&prefix) && prefix.st_size==16384,"File-limit fixture did not leave an unaligned partial prefix");
            const auto resumed=ResumeReceiverCatalog(fixture.target,target);Require(!::stat((target+"/source_0/data0.graw").c_str(),&completed) && Same(prefix,completed) && completed.st_size==131072 && VerifyTransferredCatalog(target)==resumed,"Unaligned partial prefix was replaced/truncated or failed resume");++cases;
        }
        std::cout<<"PASS streaming SHA standard/chunk vectors, thread-local SIGPIPE isolation, all four oversized TCP metadata fields rejected before staging, plain TCP new-transfer/interruption-resume/readback/completed-no-payload-resume"<<(argc==2 ? "/CLI bounded connection refusal/listener-send separate-process/readback" : "")<<", and "<<cases<<" native catalog transfer scenarios; distinct filesystems, source retained, same-size corruption, interruption, file-limit failure; synthetic_raw_bytes="<<FixtureRawBytes()<<"; owned raw cleaned\n";
    } catch(const std::exception &error) {std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
}
