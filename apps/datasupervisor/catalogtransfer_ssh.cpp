#include "catalogtransfer.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "supervisorcommon.h"
#include "controlprotocol.h"
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace GetDAQ {
namespace {
using namespace SupervisorIO;
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
JsonValue Json(const std::string &bytes) {JsonLimits limit;limit.arrayItems=65536;return ParseStrictJson(bytes,limit);}
std::string RemoteDestination(const std::string &root,const std::string &key) {return root+"/getdaq-transfer-"+key.substr(0,32);}

std::string SendReceiverCatalogOverSshMode(const std::string &source,const std::string &destination,const std::string &remoteProgram,const std::string &remoteRoot,CatalogTransferProgress progress,bool resume) {
    Path(source);Path(remoteProgram);Path(remoteRoot);Text(destination,255);
    Require(destination[0]!='-' && destination.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_.@:-[]") == std::string::npos,"SSH target contains unsupported characters");
    const auto quote=[](const std::string &value) {std::string out="'";for(char c:value){if(c=='\'')out+="'\\''";else out+=c;}return out+"'";};
    const auto remoteCommand="exec "+quote(remoteProgram)+(resume ? " --resume-catalog-stream " : " --receive-catalog-stream ")+quote(remoteRoot);
    int toChild[2]={-1,-1},fromChild[2]={-1,-1};
    const auto closePipes=[&] {for(int *ends:{toChild,fromChild})for(int i=0;i<2;++i)if(ends[i]>=0){::close(ends[i]);ends[i]=-1;}};
    if(::pipe(toChild))throw std::runtime_error(std::string("Cannot create OpenSSH input pipe: ")+std::strerror(errno));
    if(::pipe(fromChild)){const auto error=errno;closePipes();throw std::runtime_error(std::string("Cannot create OpenSSH output pipe: ")+std::strerror(error));}
    int *pipeEnds[]={&toChild[0],&toChild[1],&fromChild[0],&fromChild[1]};
    for(auto end:pipeEnds) {
        if(*end<=STDERR_FILENO) {const int replacement=::fcntl(*end,F_DUPFD,STDERR_FILENO+1);if(replacement<0){const auto error=errno;closePipes();throw std::runtime_error(std::string("Cannot reserve OpenSSH pipe descriptor: ")+std::strerror(error));}::close(*end);*end=replacement;}
        if(::fcntl(*end,F_SETFD,FD_CLOEXEC)){const auto error=errno;closePipes();throw std::runtime_error(std::string("Cannot protect OpenSSH pipe descriptor: ")+std::strerror(error));}
    }
    posix_spawn_file_actions_t actions;const auto init=::posix_spawn_file_actions_init(&actions);
    if(init){closePipes();throw std::runtime_error(std::string("Cannot initialize OpenSSH process actions: ")+std::strerror(init));}
    int setup=::posix_spawn_file_actions_adddup2(&actions,toChild[0],STDIN_FILENO);
    if(!setup)setup=::posix_spawn_file_actions_adddup2(&actions,fromChild[1],STDOUT_FILENO);
    for(auto end:pipeEnds)if(!setup)setup=::posix_spawn_file_actions_addclose(&actions,*end);
    if(setup){::posix_spawn_file_actions_destroy(&actions);closePipes();throw std::runtime_error(std::string("Cannot configure OpenSSH process actions: ")+std::strerror(setup));}
    std::vector<std::string> words={"ssh","-T","-o","BatchMode=yes","-o","Compression=no","-o","ConnectTimeout=15","-o","ServerAliveInterval=15","-o","ServerAliveCountMax=3",destination,remoteCommand};
    std::vector<char *> args;for(auto &word:words)args.push_back(&word[0]);args.push_back(nullptr);pid_t child=0;
    const auto spawn=::posix_spawnp(&child,"ssh",&actions,nullptr,args.data(),environ);::posix_spawn_file_actions_destroy(&actions);
    ::close(toChild[0]);toChild[0]=-1;::close(fromChild[1]);fromChild[1]=-1;
    if(spawn){closePipes();throw std::runtime_error(std::string("Cannot start OpenSSH archive transport: ")+std::strerror(spawn));}
    struct WaitForChild {pid_t pid;bool reaped;explicit WaitForChild(pid_t value):pid(value),reaped(false){}~WaitForChild(){if(!reaped){int status=0;while(::waitpid(pid,&status,0)<0 && errno==EINTR){}}}void Finish(){int status=0;pid_t result;do{result=::waitpid(pid,&status,0);}while(result<0 && errno==EINTR);reaped=true;Require(result==pid && WIFEXITED(status) && WEXITSTATUS(status)==0,"OpenSSH remote archive command failed after stream completion");}} childGuard(child);
    Fd remoteInput(toChild[1]),remoteOutput(fromChild[0]);toChild[1]=-1;fromChild[0]=-1;
    const auto receipt=SendReceiverCatalogStream(source,remoteOutput.value,remoteInput.value,progress,resume,"openssh-stdio-v1");
    ::close(remoteInput.value);remoteInput.value=-1;childGuard.Finish();const auto sourceRequest=ReadFile(source+"/request.json",1024*1024);const auto sourceData=DecodeReceiverCatalogRequest(sourceRequest);
    Require(String(Json(receipt).At("destination"))==RemoteDestination(remoteRoot,ReceiverCatalogRequestKey(sourceData)),"SSH receiver published an unexpected destination");return receipt;
}
}

std::string SendReceiverCatalogOverSsh(const std::string &source,const std::string &destination,const std::string &remoteProgram,const std::string &remoteRoot,CatalogTransferProgress progress) {
    return SendReceiverCatalogOverSshMode(source,destination,remoteProgram,remoteRoot,progress,false);
}
std::string ResumeReceiverCatalogOverSsh(const std::string &source,const std::string &destination,const std::string &remoteProgram,const std::string &remoteRoot,CatalogTransferProgress progress) {
    return SendReceiverCatalogOverSshMode(source,destination,remoteProgram,remoteRoot,progress,true);
}
}
