#ifndef GETDAQ_NATIVE_REMOTE_SERVICE_FIXTURE_H
#define GETDAQ_NATIVE_REMOTE_SERVICE_FIXTURE_H
#include "native_receiver_fixture.h"
#include "planio.h"
#include "controlhttp.h"
#include <atomic>
#include <cerrno>
#include <csignal>
#include <fstream>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <sys/wait.h>
extern char **environ;
namespace GetDAQRemoteServiceFixture {
using namespace GetDAQReceiverFixture;
template<class F> void Wait(F action,unsigned int timeout=5000) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(timeout);
    while (!action()) { Require(std::chrono::steady_clock::now()<deadline,"Remote fixture observation timeout"); std::this_thread::sleep_for(std::chrono::milliseconds(15)); }
}
inline void SendBytes(int fd,const std::string &bytes) {
    std::size_t offset=0;
    while (offset<bytes.size()) { const auto n=::send(fd,bytes.data()+offset,bytes.size()-offset,0); Require(n>0,"Fixture relay/raw write"); offset+=std::size_t(n); }
}
inline int ConnectPort(unsigned int port) {
    IO::Fd fd(::socket(AF_INET,SOCK_STREAM,0)); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
    Require(fd.value>=0 && !::connect(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)),"Fixture relay/data connect");
    timeval timeout={5,0}; ::setsockopt(fd.value,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)); ::setsockopt(fd.value,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    const int result=fd.value; fd.value=-1; return result;
}
inline std::string ReadHttp(int fd) {
    std::string bytes; std::size_t expected=0;
    for (;;) {
        char buffer[8192]; const auto n=::recv(fd,buffer,sizeof(buffer),0);
        Require(n>0,"Fixture HTTP relay incomplete packet"); bytes.append(buffer,std::size_t(n)); Require(bytes.size()<=1024*1024+16384,"Fixture HTTP relay size");
        const auto split=bytes.find("\r\n\r\n");
        if (!expected && split!=std::string::npos) {
            const auto start=bytes.find("\r\nContent-Length: "); Require(start!=std::string::npos && start<split,"Fixture HTTP relay length");
            expected=split+4+std::stoul(bytes.substr(start+18,bytes.find("\r\n",start+2)-start-18));
        }
        if (expected && bytes.size()>=expected) { Require(bytes.size()==expected,"Fixture relay extra bytes"); return bytes; }
    }
}
class DropReplyRelay {
    IO::Fd listener;
    unsigned int target;
    std::string command;
    std::atomic<bool> stopping{false};
    std::thread worker;
    std::function<void(const std::string &,const std::string &)> observer;
public:
    std::atomic<unsigned int> dropped{0};
    std::atomic<bool> hideOperations{false};
    std::atomic<bool> corruptStatusSession{false};
    std::string failure;
    DropReplyRelay(unsigned int port,unsigned int real,const std::string &drop,
                   const std::function<void(const std::string &,const std::string &)> &observe={})
        : listener(::socket(AF_INET,SOCK_STREAM,0)),target(real),command(drop),observer(observe) {
        sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
        Require(listener.value>=0 && !::bind(listener.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)) && !::listen(listener.value,8),"Fixture relay listen");
        ::fcntl(listener.value,F_SETFD,FD_CLOEXEC);
        worker=std::thread([this] {
            try {
                while (!stopping) {
                    pollfd p={listener.value,POLLIN,0}; const auto ready=::poll(&p,1,50); if (ready<=0) continue;
                    IO::Fd client(::accept(listener.value,nullptr,nullptr)); Require(client.value>=0,"Fixture relay accept");
                    timeval timeout={5,0}; ::setsockopt(client.value,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
                    const auto request=ReadHttp(client.value); IO::Fd remote(ConnectPort(target)); SendBytes(remote.value,request); auto response=ReadHttp(remote.value);
                    if(corruptStatusSession && request.compare(0,15,"GET /v1/status ")==0) {
                        const auto start=response.find("\r\n\r\n");Require(start!=std::string::npos,"Status relay lacks body");
                        const auto body=ParseStrictJson(response.substr(start+4));
                        const auto session=IO::String(body.At("lifecycle").At("session_id"));
                        Require(session.size()==64,"Status relay needs a bound cycle");
                        const auto replacement=session==std::string(64,'a') ? std::string(64,'b') : std::string(64,'a');
                        std::size_t position=start+4;
                        while((position=response.find(session,position))!=std::string::npos) {response.replace(position,64,replacement);position+=64;}
                    }
                    if (observer) observer(request,response);
                    bool drop=false;
                    if (!command.empty() && dropped==0 && request.compare(0,5,"POST ")==0) {
                        const auto body=request.substr(request.find("\r\n\r\n")+4);
                        if (IO::String(ParseStrictJson(body).At("command"))==command) { ++dropped; drop=true; }
                    }
                    if(hideOperations && request.compare(0,19,"GET /v1/operations/")==0)
                        SendBytes(client.value,"HTTP/1.1 404 Not Found\r\nContent-Type: application/json\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}");
                    else if (!drop) SendBytes(client.value,response); // 先确认真实服务接受，再故意丢客户端应答。
                }
            } catch (const std::exception &e) { failure=e.what(); }
        });
    }
    ~DropReplyRelay() { stopping=true; if (worker.joinable()) worker.join(); }
};
inline std::string ProcessIdentity(int pid) {
    char path[4096]; const auto name="/proc/"+std::to_string(pid); const auto n=::readlink((name+"/exe").c_str(),path,sizeof(path));
    if (n<=0) return "";
    std::ifstream file(name+"/stat"); std::string stat; std::getline(file,stat); const auto begin=stat.rfind(") "); if (begin==std::string::npos) return "";
    std::istringstream fields(stat.substr(begin+2)); std::string value;
    for (unsigned int i=0;i<20;++i) if (!(fields>>value)) return "";
    return std::string(path,std::size_t(n))+':'+value;
}
class Service {
    std::string executable,configPath,credentialPath;
    unsigned int generation=0;
    std::map<int,std::string> children;
    std::map<std::string,std::string> environment;
public:
    int pid=-1;
    std::string root,agent;
    unsigned int port;
    DataSupervisorOptions plan;
    SupervisorHttpOptions direct;
    Service(const std::string &exe,const std::string &directory,const std::string &id,unsigned int controlPort,const DataSupervisorOptions &receiver,
            const std::map<std::string,std::string> &childEnvironment={}) :
        executable(exe),configPath(directory+"/service.json"),credentialPath(directory+"/credential"),environment(childEnvironment),root(directory),agent(id),port(controlPort),plan(receiver) {
        IO::CreateDirectory(root); IO::WriteNew(credentialPath,IO::RandomSession()+"\n");
        const auto bytes="{\"schema\":1,\"kind\":\"getdaq.supervisor-service\",\"agent_id\":"+EncodeJsonString(agent)+
            ",\"controller_id\":\"native-remote-controller\",\"bind\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credentialPath)+
            ",\"receiver_plan\":"+IO::EncodeOptions(plan,GetDAQ::ConfigurationSha256(IO::ReadFile(plan.receiverExecutable,64*1024*1024)))+"}\n";
        IO::WriteNew(configPath,bytes); direct.address="127.0.0.1"; direct.port=port; direct.credentialFile=credentialPath; direct.timeoutMs=1500; Start();
    }
    ~Service() {
        try { Capture(); } catch (...) {}
        try { Stop(); } catch (...) { if (pid>0) { ::kill(pid,SIGKILL); ::waitpid(pid,nullptr,0); pid=-1; } }
        // 仅测试夹具回收明确记录的PID+可执行文件+启动时间；产品服务/适配器没有此自动策略。
        for (const auto &child:children) if (ProcessIdentity(child.first)==child.second) ::kill(child.first,SIGTERM);
    }
    void Start(const std::string &predecessor="") {
        IO::Fd log(::open((root+"/service-"+std::to_string(generation++)+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600)); Require(log.value>=0,"Fixture service log");
        posix_spawn_file_actions_t actions; Require(!::posix_spawn_file_actions_init(&actions),"Fixture service spawn actions");
        ::posix_spawn_file_actions_adddup2(&actions,log.value,STDOUT_FILENO); ::posix_spawn_file_actions_adddup2(&actions,log.value,STDERR_FILENO);
        std::vector<std::string> words={executable,"--config",configPath};
        if (!predecessor.empty()) {words.push_back("--predecessor-proof");words.push_back(predecessor);}
        std::vector<char *> argv; for (auto &word:words) argv.push_back(&word[0]); argv.push_back(nullptr);
        std::vector<std::string> env;
        for(char **entry=environ;*entry;++entry) {const std::string value=*entry;if(!environment.count(value.substr(0,value.find('='))))env.push_back(value);}
        for(const auto &entry:environment)env.push_back(entry.first+'='+entry.second);
        std::vector<char *> envp;for(auto &value:env)envp.push_back(&value[0]);envp.push_back(nullptr);
        pid_t child=0; const int result=::posix_spawn(&child,executable.c_str(),&actions,nullptr,argv.data(),envp.data()); ::posix_spawn_file_actions_destroy(&actions);
        Require(!result && child>0,"Spawn actual remote service"); pid=child;
        Wait([&] { int status; const auto exited=::waitpid(pid,&status,WNOHANG); Require(exited==0,"Actual remote service startup exited"); try { return SupervisorHttpClient(direct).Identity().code==200; } catch (...) { return false; } });
    }
    void ReplacePlan(const DataSupervisorOptions &next,const std::string &predecessor) {
        Stop();plan=next;configPath=root+"/next-service.json";
        IO::WriteNew(configPath,"{\"schema\":1,\"kind\":\"getdaq.supervisor-service\",\"agent_id\":"+EncodeJsonString(agent)+
            ",\"controller_id\":\"native-remote-controller\",\"bind\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credentialPath)+
            ",\"receiver_plan\":"+IO::EncodeOptions(plan,GetDAQ::ConfigurationSha256(IO::ReadFile(plan.receiverExecutable,64*1024*1024)))+"}\n");
        Start(predecessor);
    }
    JsonValue Status() const { const auto response=SupervisorHttpClient(direct).Status(); Require(response.code==200,"Fixture direct service status"); return ParseStrictJson(response.body); }
    JsonValue Operation(const std::string &id) const { const auto response=SupervisorHttpClient(direct).Operation(id); Require(response.code==200,"Fixture direct operation query"); return ParseStrictJson(response.body); }
    void Capture() {
        const auto data=DecodeSupervisorStatus(Status().At("lifecycle"));
        for (const auto &child:data.receivers) if (child.pid>0 && !children.count(child.pid)) { const auto identity=ProcessIdentity(child.pid); if (!identity.empty()) children[child.pid]=identity; }
    }
    void Stop() { if (pid>0) { ::kill(pid,SIGTERM); int code; Require(::waitpid(pid,&code,0)==pid && WIFEXITED(code) && WEXITSTATUS(code)==0,"Fixture service graceful exit"); pid=-1; } }
    void CrashAndRestart() { Capture(); Require(pid>0 && !::kill(pid,SIGKILL),"Fixture service crash"); Require(::waitpid(pid,nullptr,0)==pid,"Reap crashed fixture service"); pid=-1; Start(); }
    unsigned int Commands(const std::string &command) const {
        std::ifstream journal(plan.auditRoot+"/control/operations.jsonl"); std::string row; unsigned int count=0;
        while (std::getline(journal,row)) { const auto v=ParseStrictJson(row); if (IO::String(v.At("event"))=="accepted" && SupervisorCommandName(DecodeSupervisorControlRequest(IO::String(v.At("request"))).command)==command) ++count; }
        return count;
    }
};
}
#endif
