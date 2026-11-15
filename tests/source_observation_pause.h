#ifndef GETDAQ_TEST_SOURCE_OBSERVATION_PAUSE_H
#define GETDAQ_TEST_SOURCE_OBSERVATION_PAUSE_H
#include <cerrno>
#include <csignal>
#include <future>
#include <functional>
#include <string>
#include <poll.h>
#include <pthread.h>
#include <thread>
#include <unistd.h>
namespace GetDAQReceiverFixture {
namespace ObservationPauseDetail {
static volatile sig_atomic_t entered=-1,released=-1;
inline void Handler(int) {
    const int saved=errno;const char marker='P';char resume;
    ssize_t result;
    do {result=::write(int(entered),&marker,1);}while(result<0 && errno==EINTR);
    do {result=::read(int(released),&resume,1);}while(result<0 && errno==EINTR);
    errno=saved;
}
}
// Pause only the observation caller after its RPC has entered the servant,
// like a debugger breakpoint. Handler uses only async-signal-safe read/write.
// This keeps captured handles alive across slow, real Reset without changing
// the production query timeout or inserting callbacks into production code.
class ObservationPause {
    int fEntered[2]={-1,-1},fReleased[2]={-1,-1};
    struct sigaction fPrevious={};
    bool fInstalled=false,fResumed=false;
    std::promise<std::string> fPromise;
    std::future<std::string> fResult;
    std::thread fWorker;
    void Close() {
        for(auto pair:{fEntered,fReleased})for(int i=0;i<2;++i)if(pair[i]>=0){::close(pair[i]);pair[i]=-1;}
    }
public:
    explicit ObservationPause(const std::function<std::string()> &read):fResult(fPromise.get_future()) {
        Require(ObservationPauseDetail::entered<0,"Only one observation pause gate per process");
        if(::pipe(fEntered) || ::pipe(fReleased)){Close();throw std::runtime_error("Create observer pause pipes");}
        struct sigaction action={};action.sa_handler=ObservationPauseDetail::Handler;::sigemptyset(&action.sa_mask);
        ObservationPauseDetail::entered=fEntered[1];ObservationPauseDetail::released=fReleased[0];
        if(::sigaction(SIGUSR1,&action,&fPrevious)) {ObservationPauseDetail::entered=ObservationPauseDetail::released=-1;Close();throw std::runtime_error("Install observer pause handler");}
        fInstalled=true;
        try {fWorker=std::thread([this,read] {try {fPromise.set_value(read());}catch(...) {fPromise.set_exception(std::current_exception());}});}
        catch(...) {::sigaction(SIGUSR1,&fPrevious,nullptr);ObservationPauseDetail::entered=ObservationPauseDetail::released=-1;Close();throw;}
    }
    void Suspend() {
        Require(::pthread_kill(fWorker.native_handle(),SIGUSR1)==0,"Pause observation caller");
        struct pollfd descriptor={fEntered[0],POLLIN,0};int ready;
        do {ready=::poll(&descriptor,1,2000);}while(ready<0 && errno==EINTR);
        char marker;Require(ready==1 && (descriptor.revents&POLLIN) && ::read(fEntered[0],&marker,1)==1 && marker=='P',"Observation caller did not pause");
    }
    void Resume() {
        if(fResumed)return;fResumed=true;const char marker='R';ssize_t result;
        do {result=::write(fReleased[1],&marker,1);}while(result<0 && errno==EINTR);
    }
    std::string Result() {Resume();if(fWorker.joinable())fWorker.join();return fResult.get();}
    bool Pending() const {return fResult.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready;}
    ~ObservationPause() {
        Resume();if(fWorker.joinable())fWorker.join();
        if(fInstalled)::sigaction(SIGUSR1,&fPrevious,nullptr);
        ObservationPauseDetail::entered=ObservationPauseDetail::released=-1;Close();
    }
};
}
#endif
