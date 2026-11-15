#include "receiverprocessposix_p.h"

#include <cerrno>
#include <csignal>
#include <fcntl.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

extern char **environ;

namespace GetDAQ { namespace ReceiverProcessInternal {
using namespace SupervisorIO;

int OpenLog(const std::string &path) {
    WriteNew(path,""); const auto fd=::open(path.c_str(),O_WRONLY|O_NOFOLLOW|O_CLOEXEC);
    if (fd<0) throw std::runtime_error("Cannot open receiver process log");
    return fd;
}
struct SpawnActions {
    posix_spawn_file_actions_t actions;
    posix_spawnattr_t attributes;
    SpawnActions() {
        if (::posix_spawn_file_actions_init(&actions)) throw std::runtime_error("Cannot initialize spawn file actions");
        if (::posix_spawnattr_init(&attributes)) { ::posix_spawn_file_actions_destroy(&actions); throw std::runtime_error("Cannot initialize spawn attributes"); }
    }
    ~SpawnActions() { ::posix_spawnattr_destroy(&attributes); ::posix_spawn_file_actions_destroy(&actions); }
};
void CycleControl(Fd &reader,Fd &writer) {
    int pair[2]={-1,-1};
    if (::socketpair(AF_UNIX,SOCK_STREAM,0,pair)) throw std::runtime_error("Cannot create private receiver cycle control");
    reader.value=pair[0]; writer.value=pair[1];
    for (const int fd:pair) {
        const int flags=::fcntl(fd,F_GETFL),descriptor=::fcntl(fd,F_GETFD);
        if (flags<0 || descriptor<0 || ::fcntl(fd,F_SETFL,flags|O_NONBLOCK) || ::fcntl(fd,F_SETFD,descriptor|FD_CLOEXEC))
            throw std::runtime_error("Cannot protect private receiver cycle control");
    }
#ifdef SO_NOSIGPIPE
    const int enabled=1;
    if (::setsockopt(writer.value,SOL_SOCKET,SO_NOSIGPIPE,&enabled,sizeof(enabled))) throw std::runtime_error("Cannot suppress cycle control SIGPIPE");
#endif
}
void SendCycleControl(int fd,char request) {
#ifdef MSG_NOSIGNAL
    const int flags=MSG_NOSIGNAL;
#elif defined(SO_NOSIGPIPE)
    const int flags=0;
#else
#error "Private cycle control requires MSG_NOSIGNAL or SO_NOSIGPIPE"
#endif
    ssize_t sent;
    do {sent=::send(fd,&request,1,flags);} while(sent<0 && errno==EINTR);
    if (sent!=1) throw std::runtime_error("Private receiver cycle finish request failed; no retry");
}
pid_t Spawn(const std::string &executable,const std::vector<std::string> &args,const std::string &log,int input) {
    Fd out(OpenLog(log+".stdout")),error(OpenLog(log+".stderr")); SpawnActions spawn;
    sigset_t mask,defaults; ::sigemptyset(&mask); ::sigemptyset(&defaults); ::sigaddset(&defaults,SIGINT); ::sigaddset(&defaults,SIGTERM); ::sigaddset(&defaults,SIGPIPE);
    const int standardInput=input<0 ? ::posix_spawn_file_actions_addopen(&spawn.actions,STDIN_FILENO,"/dev/null",O_RDONLY,0) :
        ::posix_spawn_file_actions_adddup2(&spawn.actions,input,STDIN_FILENO);
    if (standardInput ||
        ::posix_spawn_file_actions_adddup2(&spawn.actions,out.value,STDOUT_FILENO) || ::posix_spawn_file_actions_adddup2(&spawn.actions,error.value,STDERR_FILENO) ||
        ::posix_spawnattr_setsigmask(&spawn.attributes,&mask) || ::posix_spawnattr_setsigdefault(&spawn.attributes,&defaults) ||
        ::posix_spawnattr_setflags(&spawn.attributes,POSIX_SPAWN_SETSIGMASK|POSIX_SPAWN_SETSIGDEF)) throw std::runtime_error("Cannot configure safe receiver spawn");
    std::vector<char *> argv; for (const auto &a:args) argv.push_back(const_cast<char *>(a.c_str())); argv.push_back(nullptr);
    pid_t pid=0; const auto result=::posix_spawn(&pid,executable.c_str(),&spawn.actions,&spawn.attributes,argv.data(),environ);
    if (result || pid<=0) throw std::runtime_error("Receiver posix_spawn failed; no retry");
    return pid;
}
bool Exited(pid_t pid) {
    siginfo_t info={};
    for (;;) {
        if (::waitid(P_PID,id_t(pid),&info,WEXITED|WNOHANG|WNOWAIT)==0) return info.si_pid==pid;
        if (errno!=EINTR) throw std::runtime_error("Receiver child ownership/status unavailable; do not signal a reused PID");
    }
}
int Reap(pid_t pid) {
    int result=0; pid_t returned;
    do { returned=::waitpid(pid,&result,WNOHANG); } while (returned<0 && errno==EINTR);
    if (returned!=pid) throw std::runtime_error("Cannot reap the expected receiver child");
    return WIFEXITED(result) ? WEXITSTATUS(result) : WIFSIGNALED(result) ? 128+WTERMSIG(result) : 255;
}
ReceiverProcessObservation Observe(const ManagedReceiverStatus &child) {
    ReceiverProcessObservation result; result.id=child.id; result.pid=child.pid;
    if (child.reaped) { result.state="reaped"; result.exitCode=child.exitCode; return result; }
    if (child.pid<=0) { result.state="not-started"; return result; }
    siginfo_t info={};
    // 有界重试，不在Qt事件循环中无限等待信号风暴。
    for (unsigned int attempt=0;attempt<3;++attempt) {
        if (::waitid(P_PID,id_t(child.pid),&info,WEXITED|WNOHANG|WNOWAIT)==0) {
            if (!info.si_pid) { result.state="running"; return result; }
            if (info.si_pid==child.pid && info.si_code==CLD_EXITED) {
                result.state="exited"; result.exitCode=info.si_status; return result;
            }
            if (info.si_pid==child.pid && (info.si_code==CLD_KILLED || info.si_code==CLD_DUMPED)) {
                result.state="exited"; result.signal=info.si_status; result.exitCode=128+info.si_status; return result;
            }
            result.state="unavailable"; result.message="Unexpected owned child wait status"; return result;
        }
        const int error=errno;
        if (error==EINTR) continue;
        result.state="unavailable";
        result.message=error==ECHILD ? "Child ownership unavailable; PID is not adopted or probed" : "Owned child status query failed";
        return result;
    }
    result.state="unavailable"; result.message="Owned child status query interrupted"; return result;
}

} } // namespace GetDAQ::ReceiverProcessInternal
