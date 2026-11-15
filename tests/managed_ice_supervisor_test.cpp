#include "datasupervisor.h"
#include "supervisorio.h"
#include "controlprotocol.h"
#include "receivercatalog.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <functional>
#include <iostream>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

namespace {
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;
void Require(bool value,const char *message) {if(!value) throw std::runtime_error(message);}
void Reject(const std::function<void()> &action) {bool rejected=false;try {action();} catch(const std::exception &) {rejected=true;}Require(rejected,"Expected refusal");}
unsigned int Port() {
    static unsigned int next=10000+(unsigned(::getpid())%140)*128;
    for(unsigned int i=0;i<128;++i) {const auto port=next++;Fd fd(::socket(AF_INET,SOCK_STREAM,0));sockaddr_in a={};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
        Require(fd.value>=0,"Port fixture socket");if(!::bind(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a))) return port;}
    throw std::runtime_error("No fixture port");
}
std::string Frame() {
    std::string value(4096,'\1');const unsigned char header[]={6,0,0,64,7,0,1,4,0,2,0,4,0,0,3,224};
    for(unsigned int i=0;i<sizeof(header);++i) value[i]=char(header[i]);
    value[26]=value[27]=0;return value;
}
void Tcp(unsigned int port) {
    Fd fd(::socket(AF_INET,SOCK_STREAM,0));sockaddr_in a={};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(port);
    Require(fd.value>=0 && !::connect(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)),"TCP fixture connect");const auto frame=Frame();std::size_t offset=0;
    while(offset<frame.size()) {const auto bytes=::send(fd.value,frame.data()+offset,frame.size()-offset,0);Require(bytes>0,"TCP fixture send");offset+=std::size_t(bytes);}::shutdown(fd.value,SHUT_WR);
}
int Client(const std::string &executable,const ReceiverSourcePlan &source,const std::string &session,const std::string &mode,const std::string &encoding) {
    const auto child=::fork();Require(child>=0,"Fork source fixture");
    if(!child) {::execl(executable.c_str(),executable.c_str(),"127.0.0.1",std::to_string(source.dataPort).c_str(),session.c_str(),mode.c_str(),encoding.c_str(),static_cast<char *>(nullptr));::_exit(127);}
    int status;while(::waitpid(child,&status,0)<0) {if(errno!=EINTR) throw std::runtime_error("Wait source fixture");}
    return WIFEXITED(status) ? WEXITSTATUS(status) : 255;
}
void Cleanup(DataSupervisor &manager) {
    for(const auto &receiver:manager.Status().receivers) if(receiver.pid>0 && !receiver.reaped) {
        // Fixture cleanup only; the production supervisor never uses this policy.
        siginfo_t info={};if(::waitid(P_PID,id_t(receiver.pid),&info,WEXITED|WNOHANG|WNOWAIT)) continue;
        if(!info.si_pid)::kill(receiver.pid,SIGTERM);
        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
        while(!info.si_pid && std::chrono::steady_clock::now()<deadline) {std::this_thread::sleep_for(std::chrono::milliseconds(20));::waitid(P_PID,id_t(receiver.pid),&info,WEXITED|WNOHANG|WNOWAIT);}
        if(!info.si_pid) ::kill(receiver.pid,SIGKILL);
        int status;while(::waitpid(receiver.pid,&status,0)<0 && errno==EINTR) {}
    }
}
void Replace(const std::string &path,const std::string &from,const std::string &to) {
    auto value=ReadFile(path,65536);const auto at=value.find(from);Require(at!=std::string::npos,"Receipt mutation text missing");value.replace(at,from.size(),to);
    Require(!::unlink(path.c_str()),"Remove owned receipt fixture");WriteNew(path,value);
}
}
int main(int argc,char **argv) {
    try {
        Require(argc==7,"Pass NATIVE_RECEIVER ICE_RECEIVER ICE_FIXTURE ROOT SCENARIO ENCODING");
        const std::string root=argv[4],scenario=argv[5];DataSupervisorOptions options;options.receiverExecutable=argv[1];options.iceReceiverExecutable=argv[2];
        options.iceReceiverSha256=ConfigurationSha256(ReadFile(argv[2],64*1024*1024));options.auditRoot=root+"/deployment";options.deploymentId="ice-test";
        options.configurationSha256=std::string(64,'a');options.artifactSha256=std::string(64,'b');options.catalogRoot=root+"/catalog";CreateDirectory(options.catalogRoot);CreateDirectory(root+"/incoming");
        options.startupTimeoutMs=1000;options.finishTimeoutMs=700;options.statusTimeoutMs=300;
        for(unsigned int i=0;i<2;++i) {ReceiverSourcePlan source;source.id="C"+std::to_string(i);source.inputProtocol=(scenario=="mixed" && i==0) ? "TCP" : "ICE";
            if(scenario=="byte-storage") source.storageMode="byte-storage";
            source.outputRoot=root+"/incoming";source.bindAddress="127.0.0.1";source.dataPort=Port();source.statusPort=Port();source.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort);
            source.queueMiB=source.maxFrameMiB=1;source.fileNaming=i==1 ? "get" : "native";options.sources.push_back(source);}
        if(scenario=="wrong-program") {auto bad=options;bad.iceReceiverSha256=std::string(64,'f');Reject([&] {DataSupervisor manager(bad);});Require(::access(options.auditRoot.c_str(),F_OK)!=0,"Invalid program created deployment");}
        else {
            Fd busy;
            if(scenario=="abort-startup") {busy.value=::socket(AF_INET,SOCK_STREAM,0);sockaddr_in a={};a.sin_family=AF_INET;a.sin_addr.s_addr=htonl(INADDR_LOOPBACK);a.sin_port=htons(options.sources[1].dataPort);Require(busy.value>=0 && !::bind(busy.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)) && !::listen(busy.value,1),"Occupy owned test port");}
            DataSupervisor manager(options);
            try {
                if(scenario=="abort-startup") {Reject([&] {manager.Start(root+"/start");});manager.AbortStartup(root+"/abort");Require(manager.Status().state==DataSupervisorStatus::Aborted,"Empty startup not aborted");}
                else {
                    manager.Start(root+"/start");manager.VerifyReady();const auto initial=manager.Status();
                    Reject([&] {manager.Finish(root+"/invalid-unused",{"C1"});});Require(manager.Status().state==DataSupervisorStatus::Ready,"ICE unused declaration changed lifecycle");
                    for(unsigned int i=0;i<2;++i) {
                        const auto &source=options.sources[i];if(source.inputProtocol=="TCP") {Tcp(source.dataPort);continue;}
                        std::string mode=scenario=="byte-storage" ? "byte-storage" : "complete";if(scenario=="empty") mode="empty";
                        if(i==1 && scenario=="missing-end") mode="legacy";
                        if(i==1 && scenario=="truncated") mode="truncated";
                        const auto code=Client(argv[3],source,initial.sessionId,mode,argv[6]);Require(code==0 || mode=="truncated","ICE source fixture failed");
                    }
                    if(scenario=="receipt-tamper") {
                        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);const auto path=initial.receivers[1].outputDirectory+"/.getdaq-ice-receiver-seal.json";
                        while(::access(path.c_str(),F_OK)) {Require(std::chrono::steady_clock::now()<deadline,"ICE receipt timeout");std::this_thread::sleep_for(std::chrono::milliseconds(20));}
                        Replace(path,"ordered-same-connection-finishStream","controller-stop");
                    }
                    if(scenario=="missing-end" || scenario=="truncated" || scenario=="receipt-tamper") {
                        Reject([&] {manager.Finish(root+"/finish");});Require(manager.Status().state==DataSupervisorStatus::Failed,"Unconfirmed ICE source became Stopped");Reject([&] {manager.Catalog(root+"/catalog-audit","catalog-test","Test",0,initial.sessionId);});
                        if(scenario=="missing-end") {siginfo_t info={};Require(!::waitid(P_PID,id_t(initial.receivers[1].pid),&info,WEXITED|WNOHANG|WNOWAIT) && !info.si_pid,"Production Finish signaled missing source end");}
                    } else {
                        manager.Finish(root+"/finish");manager.VerifyStopped();const auto stopped=manager.Status();
                        const auto codec=DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(stopped)));
                        for(unsigned int i=0;i<2;++i) {const auto &receiver=stopped.receivers[i];const bool ice=options.sources[i].inputProtocol=="ICE";
                            Require(receiver.pid==initial.receivers[i].pid && receiver.reaped && receiver.exitCode==0 && receiver.sealVerified && receiver.sourceEndConfirmed==ice && codec.receivers[i].sourceEndConfirmed==ice,"Final source identity/end proof differs");
                            Require(receiver.receivedBytes==(scenario=="empty" ? 0 : scenario=="byte-storage" ? 65537 : ice ? 8192 : 4096),"Managed byte count differs");
                            const auto seal=ParseStrictJson(ReadFile(receiver.outputDirectory+(ice ? "/.getdaq-ice-receiver-seal.json" : "/.getdaq-receiver-seal.json"),65536));Require(Boolean(seal.At("sender_eof_confirmed"))==!ice,"ICE end became TCP EOF");
                            if(scenario=="byte-storage") Require(String(seal.At("storage_mode"))=="byte-storage","Managed raw-byte storage mode was not sealed");}
                        const auto receipt=manager.Catalog(root+"/catalog-audit","catalog-test","Test",0,initial.sessionId);const auto target=ParseStrictJson(receipt).At("directory").text;
                        Require(InspectReceiverCatalog(target)==receipt,"Managed mixed catalog differs");
                        if(scenario=="archive-tamper") {Replace(target+"/source_1/receiver-seal.json","ordered-same-connection-finishStream","controller-stop");Reject([&] {InspectReceiverCatalog(target);});}
                        WriteNew(root+"/stopped.json",EncodeSupervisorStatus(stopped));
                    }
                }
            } catch(...) {Cleanup(manager);throw;}Cleanup(manager);
        }
        WriteNew(root+"/fixture-result.json","{\"scenario\":"+EncodeJsonString(scenario)+",\"passed\":true,\"real_hardware\":false}\n");
        std::cout<<"PASS managed ICE "<<scenario<<" encoding "<<argv[6]<<'\n';return 0;
    } catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
