#include "datasupervisor.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include "receivercatalog.h"
#include "catalogtransfer.h"
#include "controlprotocol.h"
#include <algorithm>
#include <arpa/inet.h>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <fstream>
#include <fcntl.h>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <set>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;
void Require(bool value,const std::string &message) { if (!value) throw std::runtime_error(message); }
void Reject(const std::function<void()> &action,const std::string &message) {
    try { action(); } catch (const std::exception &e) { Require(std::string(e.what()).find(message)!=std::string::npos,"Unexpected rejection: "+std::string(e.what())); return; }
    throw std::runtime_error("Expected rejection: "+message);
}
unsigned int Port() {
    // 避开本机默认TCP临时客户端端口范围，防止状态查询抢到尚未listen的数据端口。
    static unsigned int next=10000+(unsigned(::getpid())%160)*128;
    for (unsigned int attempt=0;attempt<128;++attempt) {
        const auto port=next++; Fd fd(::socket(AF_INET,SOCK_STREAM,0)); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
        Require(fd.value>=0,"Fixture port socket");
        if (!::bind(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a))) return port;
    }
    throw std::runtime_error("No available fixture port in bounded range");
}
int Connect(unsigned int port) {
    Fd fd(::socket(AF_INET,SOCK_STREAM,0)); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(port);
    Require(fd.value>=0 && !::connect(fd.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)),"Connect fixture sender");
    const int result=fd.value; fd.value=-1; return result;
}
std::string Frame(unsigned int event,bool little=false) {
    std::string f(4096,char(event)); const unsigned char header[]={6,0,0,64,7,0,1,4,0,2,0,4};
    for (std::size_t i=0;i<sizeof(header);++i) f[i]=char(header[i]);
    f[12]=f[13]=0; f[14]=3; f[15]=static_cast<char>(224); // 992 items
    for (unsigned int i=0;i<4;++i) f[22+i]=char((event>>(24-8*i))&255);
    f[26]=f[27]=0;
    if (little) {
        f[0]=static_cast<char>(134); f[1]=64; f[2]=f[3]=0;
        for (const auto offset:{5,8,10}) std::swap(f[offset],f[offset+1]);
        std::reverse(f.begin()+12,f.begin()+16); std::reverse(f.begin()+22,f.begin()+26);
    }
    return f;
}
void Send(int fd,const std::string &bytes) {
    std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::send(fd,bytes.data()+offset,std::min<std::size_t>(317,bytes.size()-offset),0);
        Require(n>0,"Send fixture frame bytes"); offset+=std::size_t(n);
    }
}
void WaitExit(int pid) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    for (;;) {
        siginfo_t info={}; Require(!::waitid(P_PID,id_t(pid),&info,WEXITED|WNOHANG|WNOWAIT),"Probe fixture-owned child");
        if (info.si_pid==pid) return;
        Require(std::chrono::steady_clock::now()<deadline,"Fixture receiver exit timeout"); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
bool ExitedForFixture(int pid) {
    siginfo_t info={};Require(!::waitid(P_PID,id_t(pid),&info,WEXITED|WNOHANG|WNOWAIT),"Inspect fixture-owned child");return info.si_pid==pid;
}
void WaitBytes(const DataSupervisorOptions &o,std::size_t i,std::uint64_t count) {
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
    while (Unsigned(ParseStrictJson(HttpStatus(o.sources[i].statusPort,1000)).At("received_bytes"))<count) {
        Require(std::chrono::steady_clock::now()<deadline,"Fixture receive timeout"); std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}
void Cleanup(DataSupervisor &s) {
    // 故障测试夹具自身回收子进程；生产管理器没有这一自动kill策略。
    for (const auto &c:s.Status().receivers) if (c.pid>0 && !c.reaped) {
        siginfo_t info={}; if (::waitid(P_PID,id_t(c.pid),&info,WEXITED|WNOHANG|WNOWAIT)) continue;
        if (!info.si_pid) ::kill(c.pid,SIGTERM);
        try { WaitExit(c.pid); } catch (...) { ::kill(c.pid,SIGKILL); }
        int result; while (::waitpid(c.pid,&result,0)<0 && errno==EINTR) {}
    }
}
DataSupervisorOptions Options(const std::string &exe,const std::string &root) {
    DataSupervisorOptions o; o.auditRoot=root+"/deployment"; o.receiverExecutable=exe; o.deploymentId="fixture-only";
    o.configurationSha256=std::string(64,'a'); o.artifactSha256=std::string(64,'b');
    o.startupTimeoutMs=2000; o.finishTimeoutMs=1200; o.statusTimeoutMs=500;
    CreateDirectory(root+"/outputs"); std::set<unsigned int> ports;
    const auto port=[&] { unsigned int p; do { p=Port(); } while (!ports.insert(p).second); return p; };
    for (unsigned int i=0;i<2;++i) {
        ReceiverSourcePlan s; s.id="C"+std::to_string(i); s.outputRoot=root+"/outputs"; s.bindAddress="127.0.0.1"; s.dataPort=port(); s.statusPort=port();
        s.receiverAddress="127.0.0.1:"+std::to_string(s.dataPort); s.queueMiB=s.maxFrameMiB=1; o.sources.push_back(s);
    }
    return o;
}
void Cycle(DataSupervisor &s,const DataSupervisorOptions &o,const std::string &root,unsigned int cycle,bool empty=false) {
    s.Start(root+"/start-"+std::to_string(cycle)); s.VerifyReady(); const auto started=s.Status();
    Require(started.state==DataSupervisorStatus::Ready && started.receivers.size()==2,"Actual receivers not ready");
    for (const auto &source:o.sources) Require(Unsigned(ParseStrictJson(HttpStatus(source.statusPort,1000)).At("connections"))==0,"Readiness consumed a data connection");
    std::vector<std::string> expected(2);
    for (unsigned int i=0;i<2;++i) {
        Fd socket(Connect(o.sources[i].dataPort));
        if (!empty) for (unsigned int e=1;e<=12;++e) { const auto frame=Frame(e,i==1); expected[i]+=frame; Send(socket.value,frame); }
        ::shutdown(socket.value,SHUT_WR);
    }
    s.Finish(root+"/finish-"+std::to_string(cycle)); s.VerifyStopped(); const auto final=s.Status();
    Require(final.state==DataSupervisorStatus::Stopped && !final.needsReconciliation,"Actual EOF/seal did not commit stopped");
    for (unsigned int i=0;i<2;++i) {
        const auto &c=final.receivers[i]; Require(c.pid==started.receivers[i].pid && c.reaped && c.exitCode==0 && c.sealVerified && c.receivedBytes==expected[i].size() && c.frames==(empty ? 0 : 12),"Final actual receiver identity/count differs");
        const auto seal=ParseStrictJson(ReadFile(c.outputDirectory+"/.getdaq-receiver-seal.json",65536));
        Require(Boolean(seal.At("sender_eof_confirmed")) && String(seal.At("session_id"))==final.sessionId,"Seal missing actual EOF/session");
        std::string actual; for (const auto &file:seal.At("files").items) actual+=ReadFile(c.outputDirectory+'/'+String(file.At("name")),1024*1024);
        Require(actual==expected[i],"Actual raw data differs from independently generated frames");
    }
}
void Run(DataSupervisor &s,const DataSupervisorOptions &o,const std::string &root,const std::string &scenario) {
    s.Validate(); s.VerifyStopped();
    const auto fresh=s.ObserveReceivers();
    Require(!fresh.busy && fresh.receivers.empty() && fresh.lifecycle.state==DataSupervisorStatus::Fresh,"Fresh observation invented a process");
    const bool retire=scenario.find("retire-")==0;
    if(scenario=="abort-failed" || scenario=="abort-failed-fdt" || scenario=="abort-failed-zbuf" || retire) {
        s.Start(root+"/start");const auto before=s.Status();
        Reject([&] {s.AbortFailedSession(root+"/healthy-abort",before.sessionId);},"healthy");
        const auto wrong=before.sessionId==std::string(64,'a') ? std::string(64,'b') : std::string(64,'a');
        Reject([&] {s.AbortFailedSession(root+"/wrong-session",wrong);},"same retained");
        if(retire) Reject([&] {s.RetireFailedSession(root+"/before-abort",before.sessionId);},"completed failed-session abort");
        Require(!::kill(before.receivers[1].pid,0),"Rejected failed-session abort signalled a healthy child");
        Fd sender(Connect(o.sources[1].dataPort));const auto frame=Frame(1);Send(sender.value,frame);WaitBytes(o,1,frame.size());
        Require(!::kill(before.receivers[0].pid,SIGKILL),"Inject fixture-owned receiver failure");WaitExit(before.receivers[0].pid);
        s.AbortFailedSession(root+"/abort-failed",before.sessionId);const auto after=s.Status();
        Require(after.state==DataSupervisorStatus::Aborted && after.needsReconciliation && after.sessionId==before.sessionId,"Failed-session cleanup cleared reconciliation or changed session");
        for(std::size_t i=0;i<after.receivers.size();++i)
            Require(after.receivers[i].pid==before.receivers[i].pid && after.receivers[i].reaped && !after.receivers[i].sealVerified,"Abort adopted another process or invented a seal");
        const auto directory=after.receivers[1].outputDirectory;std::string actual;
        std::unique_ptr<DIR,int(*)(DIR *)> files(::opendir(directory.c_str()),::closedir);Require(bool(files),"Open owned test output directory");
        while(const auto entry=::readdir(files.get())) {const std::string name=entry->d_name;if(name.size()>5 && name.substr(name.size()-5)==".graw") actual+=ReadFile(directory+'/'+name,65536);}
        Require(actual==frame,"Failed-session cleanup lost retained raw data");
        struct stat receipt;Require(::lstat((directory+"/.getdaq-receiver-seal.json").c_str(),&receipt)!=0 && errno==ENOENT,"Failed-session abort fabricated completion receipt");
        Reject([&] {s.AbortFailedSession(root+"/repeat-abort",before.sessionId);},"prior abort attempt");
        Reject([&] {s.Start(root+"/automatic-restart");},"reconciliation");
        WriteNew(root+"/abort-failed-result.json",EncodeSupervisorStatus(after));
        if(retire) {
            Reject([&] {s.RetireFailedSession(root+"/wrong-retire",wrong);},"completed failed-session abort");
            Reject([&] {s.RetireFailedSession(root+"/wrong-parent",before.sessionId,"other-parent");},"original parent binding");
            if(scenario=="retire-abort-audit") {
                const auto proof=root+"/abort-failed/data-supervisor.jsonl",bytes=ReadFile(proof,65536);
                std::ofstream file(root+"/abort-failed/data-supervisor.jsonl",std::ios::app);file<<" ";file.close();
                Reject([&] {s.RetireFailedSession(root+"/retire",before.sessionId);},"audit identity/content");
                Require(s.Status().needsReconciliation,"Changed abort audit cleared reconciliation");
                {std::ofstream restore(proof,std::ios::trunc);restore<<bytes;}
                Reject([&] {s.RetireFailedSession(root+"/restored-retire",before.sessionId);},"storage fault is latched");return;
            }
            Fd lease;
            if(scenario=="retire-lock") {
                lease.value=::open((directory+"/.getdaq-receiver.lock").c_str(),O_RDWR|O_NOFOLLOW|O_CLOEXEC);
                Require(lease.value>=0 && !::flock(lease.value,LOCK_EX|LOCK_NB),"Fixture cannot acquire original child lock");
                Reject([&] {s.RetireFailedSession(root+"/locked-retire",before.sessionId);},"still held");
                Require(s.Status().state==DataSupervisorStatus::Aborted && s.Status().needsReconciliation,"Lock refusal cleared data hold");
                ::flock(lease.value,LOCK_UN);
            }
            s.RetireFailedSession(root+"/retire",before.sessionId);s.Validate();s.VerifyStopped();
            const auto retired=DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(s.Status())));
            Require(retired.state==DataSupervisorStatus::Retired && !retired.needsReconciliation && retired.sessionId==before.sessionId,"Retirement erased original cycle binding");
            Require(retired.receivers.size()==before.receivers.size(),"Retirement erased failed receiver history");
            for(std::size_t i=0;i<retired.receivers.size();++i)
                Require(retired.receivers[i].pid==before.receivers[i].pid && retired.receivers[i].reaped && !retired.receivers[i].sealVerified,"Retirement invented a seal or adopted a PID");
            WriteNew(root+"/retired-result.json",EncodeSupervisorStatus(retired));
            Reject([&] {s.RetireFailedSession(root+"/retire-again",before.sessionId);},"completed failed-session abort");
            if(scenario=="retire-proof") {
                const auto proof=root+"/retire/data-supervisor.jsonl", bytes=ReadFile(proof,65536);
                {std::ofstream file(proof,std::ios::app);file<<" ";}
                Reject([&] {s.Start(root+"/proof-changed-start");},"audit identity/content");
                {std::ofstream file(proof,std::ios::trunc);file<<bytes;}
                Reject([&] {s.Start(root+"/proof-restored-start");},"reconciliation");return;
            }
            Cycle(s,o,root,2);Require(s.Status().sessionId!=before.sessionId,"New cycle reused failed session");
            std::string preserved;std::unique_ptr<DIR,int(*)(DIR *)> scan(::opendir(directory.c_str()),::closedir);
            Require(bool(scan),"Cannot inspect preserved failed data");
            while(auto *entry=::readdir(scan.get())) {const std::string name=entry->d_name;if(name.size()>=5 && name.substr(name.size()-5)==".graw") preserved+=ReadFile(directory+"/"+name,8192);}
            Require(preserved==frame,"New cycle changed the original failed raw data");
        }
        return;
    }
    if (scenario=="fdt-unused" || scenario=="fdt-mixed-unused") {
        s.Start(root+"/start");const auto before=s.Status();
        Reject([&] {s.Finish(root+"/unknown",{"absent"});},"existing FDT");
        Reject([&] {s.Finish(root+"/duplicate",{"C0","C0"});},"Duplicate");
        if (scenario!="fdt-unused") {
            Fd socket(Connect(o.sources[0].dataPort));Send(socket.value,Frame(1));WaitBytes(o,0,4096);
            Reject([&] {s.Finish(root+"/connected",{"C0"});},"already accepted");
            Require(s.Status().state==DataSupervisorStatus::Ready && !s.Status().needsReconciliation,"Unused preflight changed cycle/gate");
            ::shutdown(socket.value,SHUT_WR);
        }
        s.Finish(root+"/finish",scenario=="fdt-unused" ? std::vector<std::string>{"C0","C1"} : std::vector<std::string>{"C1"});
        s.VerifyStopped();const auto stopped=DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(s.Status())));
        for (std::size_t i=0;i<stopped.receivers.size();++i) {
            const auto &r=stopped.receivers[i];const bool unused=scenario=="fdt-unused" || i==1;
            const auto seal=ParseStrictJson(ReadFile(r.outputDirectory+"/.getdaq-receiver-seal.json",65536));
            Require(r.pid==before.receivers[i].pid && r.reaped && r.exitCode==0 && r.sealVerified && r.sourceUnusedConfirmed==unused,"Unused source process/session proof differs");
            Require(Boolean(seal.At("sender_eof_confirmed"))==!unused && Unsigned(seal.At("connections"))==(unused ? 0 : 1),"Unused source invented EOF/connection");
            Require(r.receivedBytes==(unused ? 0 : 4096) && r.frames==(unused ? 0 : 1),"Unused source lost mixed data counters");
            if (unused) Require(Boolean(seal.At("source_unused_confirmed")) && seal.At("files").items.empty(),"Unused seal is not explicitly empty");
        }
        const auto result=s.Catalog(root+"/catalog-audit","unused-source-catalog","Exp",1,stopped.sessionId);
        Require(Boolean(ParseStrictJson(result).At("complete")),"Unused/mixed-source catalogue failed");
        WriteNew(root+"/unused-source-result.json",EncodeSupervisorStatus(stopped));return;
    }
    if(scenario=="fdt-cycle" || scenario=="fdt-active-finish" || scenario=="fdt-empty-finish") {
        s.Start(root+"/start");s.VerifyReady();const auto started=s.Status();
        for(std::size_t i=0;i<o.sources.size();++i) {
            const auto state=ParseStrictJson(HttpStatus(o.sources[i].statusPort,1000));
            Require(String(state.At("input_protocol"))=="FDT" && String(state.At("cycle_control"))=="stdin-socket-v1" && !Boolean(state.At("once")),"FDT actual cycle mode differs");
        }
        if(scenario=="fdt-empty-finish") {Reject([&] {s.Finish(root+"/finish");},"abnormal exit");for(const auto &r:s.Status().receivers) Require(!r.sealVerified,"Empty cycle fabricated EOF seal");return;}
        if(scenario=="fdt-active-finish") {
            Fd first(Connect(o.sources[0].dataPort)),second(Connect(o.sources[1].dataPort));
            Send(first.value,std::string(4,'\0')+Frame(1));Send(second.value,std::string(4,'\0')+Frame(1));
            WaitBytes(o,0,4096);WaitBytes(o,1,4096);
            auto ending=std::async(std::launch::async,[&] {s.Finish(root+"/finish");});
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            Require(ending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready,"Cycle request fabricated sender EOF");
            for(const auto &r:started.receivers) Require(!ExitedForFixture(r.pid),"Active cycle exited before sender EOF");
            Send(first.value,std::string(4,char(255)));Send(second.value,std::string(4,char(255)));
            ::shutdown(first.value,SHUT_WR);::shutdown(second.value,SHUT_WR);ending.get();s.VerifyStopped();return;
        }
        for(unsigned int segment=0;segment<3;++segment) {
            for(std::size_t i=0;i<o.sources.size();++i) {
                Fd socket(Connect(o.sources[i].dataPort));Send(socket.value,std::string(4,'\0')+Frame(segment+1,i==1)+std::string(4,char(255)));::shutdown(socket.value,SHUT_WR);
            }
            const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3);
            for(std::size_t i=0;i<o.sources.size();++i) for(;;) {
                const auto state=ParseStrictJson(HttpStatus(o.sources[i].statusPort,1000));
                if(!Boolean(state.At("connected")) && Unsigned(state.At("connections"))==segment+1 && Boolean(state.At("sender_eof_confirmed"))) break;
                Require(std::chrono::steady_clock::now()<deadline,"FDT EOF observation timeout");std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            s.VerifyReady();Require(s.Status().sessionId==started.sessionId,"Intermediate EOF changed managed session");
            for(const auto &r:started.receivers) {siginfo_t info={};Require(!::waitid(P_PID,id_t(r.pid),&info,WEXITED|WNOHANG|WNOWAIT) && !info.si_pid,"Intermediate FDT EOF ended receiver");}
        }
        s.Finish(root+"/finish");s.VerifyStopped();const auto stopped=s.Status();
        for(std::size_t i=0;i<stopped.receivers.size();++i) {
            const auto &r=stopped.receivers[i];Require(r.pid==started.receivers[i].pid && r.sessionId==started.sessionId && r.sealVerified && r.frames==3 && r.receivedBytes==3*4096,"FDT multi-EOF final proof differs");
            const auto seal=ParseStrictJson(ReadFile(r.outputDirectory+"/.getdaq-receiver-seal.json",65536));Require(Unsigned(seal.At("connections"))==3,"FDT seal lost segment count");
            std::map<unsigned long long,std::string> ordered;
            for(const auto &file:seal.At("files").items) {
                const auto name=String(file.At("name"));const auto underscore=name.rfind('_');Require(underscore!=std::string::npos,"FDT native filename sequence missing");
                Require(ordered.emplace(std::stoull(name.substr(underscore+1,name.size()-underscore-6)),name).second,"Duplicate FDT file sequence");
            }
            Require(ordered.size()==3 && ordered.begin()->first==0 && ordered.rbegin()->first==2,"FDT file sequence differs");
            std::string actual;for(const auto &file:ordered) actual+=ReadFile(r.outputDirectory+'/'+file.second,65536);
            Require(actual==Frame(1,i==1)+Frame(2,i==1)+Frame(3,i==1),"FDT controls entered raw data or segment bytes lost");
        }
        const auto catalog=s.Catalog(root+"/catalog-audit","fdt-cycle-catalog","Exp",1,stopped.sessionId);
        Require(Boolean(ParseStrictJson(catalog).At("complete")),"FDT multi-EOF catalog did not complete");
        WriteNew(root+"/fdt-cycle-result.json",EncodeSupervisorStatus(stopped));return;
    }
    if (scenario=="observe-clean" || scenario=="observe-error" || scenario=="observe-signal" || scenario=="observe-lost" || scenario=="observe-byte-storage") {
        s.Start(root+"/start"); const auto before=s.Status(); const auto running=s.ObserveReceivers();
        Require(!running.busy && running.receivers.size()==2,"Live observation missing receivers");
        for (const auto &r:running.receivers) Require(r.state=="running" && r.exitCode==-1 && !r.signal,"Live receiver falsely exited");
        const auto initialEvents=s.ObserveAcquisitionEvents(0);
        Require(initialEvents.available==(scenario!="observe-byte-storage") && initialEvents.sourceId==o.sources[0].id && initialEvents.sessionId==before.sessionId &&
                initialEvents.pid==before.receivers[0].pid && initialEvents.acquisitionEvents==0 && initialEvents.observedAtUnixMs>0,
                "Targeted receiver event observation lost identity, availability or initial count");
        Reject([&] {s.ObserveAcquisitionEvents(o.sources.size());},"outside the fixed plan");
        if (scenario=="observe-byte-storage") return;
        const auto journal=ReadFile(o.auditRoot+"/supervisor.jsonl",1024*1024);
        if (scenario=="observe-signal") { Require(!::kill(before.receivers[0].pid,SIGKILL),"Signal fixture receiver"); WaitExit(before.receivers[0].pid); }
        else {
            Fd socket(Connect(o.sources[0].dataPort)); Send(socket.value,Frame(1));
            if (scenario=="observe-error") Send(socket.value,Frame(2).substr(0,100));
            WaitBytes(o,0,Frame(1).size());
            const auto eventCount=s.ObserveAcquisitionEvents(0);
            Require(eventCount.available && eventCount.acquisitionEvents==1 && eventCount.sessionId==before.sessionId,
                    "Targeted receiver event observation differs after one complete frame");
            ::shutdown(socket.value,SHUT_WR); WaitExit(before.receivers[0].pid);
        }
        if (scenario=="observe-lost") { int code=0; Require(::waitpid(before.receivers[0].pid,&code,0)==before.receivers[0].pid,"External fixture reaper"); }
        for (unsigned int repeat=0;repeat<3;++repeat) {
            const auto observed=s.ObserveReceivers(); const auto decoded=DecodeSupervisorObservation(ParseStrictJson(EncodeSupervisorObservation(observed)));
            const auto &r=decoded.receivers.at(0);
            Require(!decoded.busy && decoded.observedAtUnixMs>0 && decoded.receivers[1].state=="running","Observation lost running peer");
            if (scenario=="observe-lost") Require(r.state=="unavailable" && r.exitCode==-1 && !r.message.empty(),"Lost ownership guessed PID state");
            else {
                Require(r.state=="exited" && r.exitCode==(scenario=="observe-clean" ? 0 : scenario=="observe-error" ? 1 : 128+SIGKILL),"Observed actual exit differs");
                Require(r.signal==(scenario=="observe-signal" ? SIGKILL : 0),"Observed signal differs");
                WaitExit(r.pid); // 重复观察没有回收：Finish仍拥有真实wait状态。
            }
            Require(EncodeSupervisorStatus(s.Status())==EncodeSupervisorStatus(before),"Read-only observation changed lifecycle");
            Require(ReadFile(o.auditRoot+"/supervisor.jsonl",1024*1024)==journal,"Read-only observation wrote journal");
        }
        { Fd peer(Connect(o.sources[1].dataPort)); Send(peer.value,Frame(1)); ::shutdown(peer.value,SHUT_WR); }
        if (scenario=="observe-clean") {
            s.Finish(root+"/finish"); s.VerifyStopped(); const auto stopped=s.ObserveReceivers();
            for (const auto &r:stopped.receivers) Require(r.state=="reaped" && r.exitCode==0,"Finished child observation differs");
            for (const auto &r:stopped.lifecycle.receivers) Require(r.sealVerified,"Observation bypassed actual seal verification");
        } else Reject([&] {s.Finish(root+"/finish");},scenario=="observe-lost" ? "ownership/status unavailable" : "abnormal exit");
        WriteNew(root+"/observation.json",EncodeSupervisorObservation(s.ObserveReceivers())); return;
    }
    if (scenario=="catalog-replace") {
        Cycle(s,o,root,1); const auto first=s.Status();
        const auto initial=s.Catalog(root+"/catalog-audit","catalog-initial","Exp",1,first.sessionId);
        const auto initialReceipt=ParseStrictJson(initial); const auto target=String(initialReceipt.At("directory"));
        std::vector<std::set<std::string>> oldNames(first.receivers.size());
        for(std::size_t i=0;i<first.receivers.size();++i) {
            const auto seal=ParseStrictJson(ReadFile(first.receivers[i].outputDirectory+"/.getdaq-receiver-seal.json",65536));
            for(const auto &file:seal.At("files").items) oldNames[i].insert(String(file.At("name")));
        }
        Cycle(s,o,root,2); const auto second=s.Status();
        const auto replaced=s.Catalog(root+"/catalog-replace-audit","catalog-replace","Exp",1,second.sessionId,true);
        Require(String(ParseStrictJson(replaced).At("directory"))==target && InspectReceiverCatalog(target)==replaced,
            "Replacement did not publish the merged catalog at the original Run path");
        const auto manifest=ParseStrictJson(ReadFile(target+"/manifest.json",16*1024*1024));
        Require(Unsigned(manifest.At("schema"))==2,"Replacement catalog did not use schema 2");
        for(std::size_t i=0;i<oldNames.size();++i) {
            const auto &source=manifest.At("sources").items.at(i);
            std::set<std::string> retained;
            for(const auto &file:source.At("retained_files").items) retained.insert(String(file.At("name")));
            for(const auto &name:oldNames[i]) {
                struct stat archived;
                Require(retained.count(name) && !::lstat((target+"/source_"+std::to_string(i)+"/"+name).c_str(),&archived) &&
                    S_ISREG(archived.st_mode),"Replacement discarded a nonmatching old raw");
            }
        }
        WriteNew(root+"/replacement-catalog-result.json",replaced);
        return;
    }
    if (scenario=="catalog" || scenario=="catalog-copy") {
        Cycle(s,o,root,1); const auto first=s.Status();
        Reject([&] { s.Catalog(root+"/wrong-catalog","wrong-session","Exp",1,std::string(64,'0')); },"current Stopped");
        const auto result=s.Catalog(root+"/catalog-audit","catalog-1","Exp",1,first.sessionId);
        const auto receipt=ParseStrictJson(result); const auto directory=String(receipt.At("directory"));
        Require(InspectReceiverCatalog(directory)==result && Boolean(receipt.At("complete")) && String(receipt.At("raw_payload_bytes_read"))=="0","Actual receiver catalog proof differs");
        WriteNew(root+"/catalog-result.json",result); WriteNew(root+"/stopped-before-catalog.json",EncodeSupervisorStatus(first)); s.VerifyStopped();
        for (std::size_t i=0;i<first.receivers.size();++i) {
            const auto seal=ParseStrictJson(ReadFile(first.receivers[i].outputDirectory+"/.getdaq-receiver-seal.json",65536));
            for (const auto &file:seal.At("files").items) {
                const auto name=String(file.At("name")); struct stat source,archived;
                Require(!::lstat((first.receivers[i].outputDirectory+'/'+name).c_str(),&source) && !::lstat((directory+"/source_"+std::to_string(i)+'/'+name).c_str(),&archived) &&
                    Same(source,archived) && source.st_nlink==2,"Actual catalog moved/copied data or lost original lookup");
            }
        }
        if(scenario=="catalog-copy") {
            const auto copied=root+"/copied-catalog";const auto transfer=CopyReceiverCatalog(directory,copied);
            Require(VerifyTransferredCatalog(copied)==transfer,"Actual receiver copied dataset verification differs");
            const auto decoded=ParseStrictJson(transfer);std::uint64_t bytes=0;for(const auto &receiver:first.receivers)bytes+=receiver.receivedBytes;
            Require(Unsigned(decoded.At("copied_bytes"),true)==bytes,"Actual receiver transfer counters differ");
            WriteNew(root+"/transfer-result.json",transfer);
        }
        Cycle(s,o,root,2); Require(s.Status().sessionId!=first.sessionId,"Catalog prevented a new independent receiver cycle");
        Require(InspectReceiverCatalog(directory)==result,"Next receiver cycle changed the old Run catalog"); return;
    }
    if (scenario=="normal" || scenario=="empty") {
        Cycle(s,o,root,1,scenario=="empty"); const auto first=s.Status(); Cycle(s,o,root,2,scenario=="empty");
        Require(s.Status().sessionId!=first.sessionId && s.Status().receivers[0].outputDirectory!=first.receivers[0].outputDirectory,"Second cycle reused data identity/directory"); return;
    }
    if (scenario=="storage" || scenario=="journal" || scenario=="lease") {
        const auto path=o.auditRoot+(scenario=="storage" ? "/plan.json" : scenario=="journal" ? "/supervisor.jsonl" : "/.supervisor.lock");
        const auto original=ReadFile(path,1024*1024);
        if (scenario=="lease") { Require(!::rename(path.c_str(),(path+".old").c_str()),"Replace deployment lock"); WriteNew(path,""); }
        else { std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary); file.put('!'); file.flush(); Require(bool(file),"Inject same-length corruption"); }
        Reject([&] { s.Start(root+"/start"); },scenario=="storage" ? "plan" : scenario=="journal" ? "journal" : "lease");
        Require(s.Status().needsReconciliation && s.Status().receivers.empty(),"Storage corruption launched receiver/cleared gate");
        if (scenario!="lease") { std::ofstream file(path,std::ios::binary|std::ios::trunc); file<<original; file.close(); }
        Reject([&] { s.Validate(); },"reconciliation"); return;
    }
    if (scenario=="partial-start" || scenario=="abort-connected" || scenario=="fdt-partial-start") {
        Fd occupied(::socket(AF_INET,SOCK_STREAM,0)); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(o.sources[1].dataPort);
        Require(occupied.value>=0 && !::bind(occupied.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)) && !::listen(occupied.value,1),"Occupy second receiver data port");
        Reject([&] { s.Start(root+"/start"); },"exited during startup"); const auto failed=s.Status();
        Require(failed.state==DataSupervisorStatus::Failed && failed.receivers.size()==2 && failed.receivers[0].pid>0,"Partial startup evidence lost");
        Reject([&] { s.Start(root+"/retry"); },"reconciliation");
        if (scenario=="abort-connected") {
            Fd socket(Connect(o.sources[0].dataPort)); Send(socket.value,Frame(1)); WaitBytes(o,0,4096);
            Reject([&] { s.AbortStartup(root+"/abort"); },"accepted data/connection");
            Require(!::kill(failed.receivers[0].pid,0),"Rejected abort still terminated data receiver"); return;
        }
        s.AbortStartup(root+"/abort"); Require(s.Status().state==DataSupervisorStatus::Aborted && s.Status().needsReconciliation,"Explicit empty abort cleared failure gate");
        for (const auto &c:s.Status().receivers) Require(c.reaped && !c.sealVerified,"Startup abort falsely proved acquisition seal");
        Reject([&] { s.AbortStartup(root+"/abort-repeat"); },"prior abort attempt"); return;
    }
    s.Start(root+"/start"); const auto children=s.Status().receivers;
    if (scenario=="timeout") {
        Fd socket(Connect(o.sources[0].dataPort)); Send(socket.value,Frame(1)); WaitBytes(o,0,4096);
        auto finishing=std::async(std::launch::async,[&] { Reject([&] { s.Finish(root+"/finish"); },"timed out"); });
        for (unsigned int i=0;i<100 && s.Status().state!=DataSupervisorStatus::Finishing;++i) std::this_thread::sleep_for(std::chrono::milliseconds(2));
        const auto before=std::chrono::steady_clock::now(); s.Status(); Require(std::chrono::steady_clock::now()-before<std::chrono::milliseconds(100),"Snapshot blocked on process wait");
        const auto observeStart=std::chrono::steady_clock::now(); const auto busy=s.ObserveReceivers();
        Require(busy.busy && busy.receivers.empty() && std::chrono::steady_clock::now()-observeStart<std::chrono::milliseconds(100),"Process observation blocked on Finish");
        DecodeSupervisorObservation(ParseStrictJson(EncodeSupervisorObservation(busy)));
        Reject([&] { s.Start(root+"/concurrent"); },"already running"); finishing.get();
        for (const auto &c:children) Require(!::kill(c.pid,0),"Finish timeout sent an implicit signal");
        Reject([&] { s.AbortStartup(root+"/wrong-abort"); },"failed startup"); return;
    }
    if (scenario=="signal-no-eof") {
        Require(!::kill(children[0].pid,SIGTERM),"Fixture signal empty receiver"); WaitExit(children[0].pid);
        Fd other(Connect(o.sources[1].dataPort)); ::shutdown(other.value,SHUT_WR);
        Reject([&] { s.Finish(root+"/finish"); },"EOF/drain/seal");
        Require(!s.Status().receivers[0].sealVerified && s.Status().receivers[1].sealVerified,"Unconnected signal exit fabricated EOF or blocked independent seal"); return;
    }
    for (unsigned int i=0;i<2;++i) {
        Fd socket(Connect(o.sources[i].dataPort)); Send(socket.value,Frame(1));
        if (scenario=="truncated" && i==0) Send(socket.value,Frame(2).substr(0,100));
        ::shutdown(socket.value,SHUT_WR); WaitExit(children[i].pid);
    }
    if (scenario=="truncated") {
        Reject([&] { s.Finish(root+"/finish"); },"abnormal exit"); Require(!s.Status().receivers[0].sealVerified && s.Status().receivers[1].sealVerified,"Truncated source masked or independent source not sealed");
        struct stat st; Require(::lstat((children[0].outputDirectory+"/.getdaq-receiver-seal.json").c_str(),&st)!=0 && errno==ENOENT,"Truncated source created complete receipt");
    } else {
        const auto sealPath=children[0].outputDirectory+"/.getdaq-receiver-seal.json";
        if (scenario=="identity") {
            auto bytes=ReadFile(sealPath,65536); const auto at=bytes.find(o.configurationSha256); Require(at!=std::string::npos,"Find seal configuration identity"); bytes[at]='c';
            std::ofstream file(sealPath,std::ios::binary|std::ios::trunc); file<<bytes; file.close();
        } else if (scenario=="file-set") WriteNew(children[0].outputDirectory+"/extra.graw","rogue");
        else throw std::runtime_error("Unknown supervisor scenario");
        Reject([&] { s.Finish(root+"/finish"); },scenario=="identity" ? "identity differs" : "file set differ");
        Require(!s.Status().receivers[0].sealVerified && s.Status().receivers[1].sealVerified,"Invalid seal/source still accepted");
    }
    Reject([&] { s.Start(root+"/retry"); },"reconciliation");
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN);
    try {
        Require(argc==4,"Pass receiver executable, new evidence root, scenario"); const std::string root=argv[2],scenario=argv[3]; CreateDirectory(root);
        auto options=Options(argv[1],root);
        if (scenario=="catalog" || scenario=="catalog-copy" || scenario=="catalog-replace" || scenario=="get-naming") { options.catalogRoot=root+"/catalog"; CreateDirectory(options.catalogRoot); }
        if (scenario=="get-naming") for(auto &source:options.sources) source.fileNaming="get";
        if (scenario=="observe-byte-storage") options.sources[0].storageMode="byte-storage";
        if (scenario=="naming-identity") options.sources[0].fileNaming="get";
        if (scenario=="abort-failed-fdt") for(auto &source:options.sources) source.inputProtocol="FDT";
        if (scenario=="zbuf-managed" || scenario=="protocol-identity" || scenario=="abort-failed-zbuf") for(auto &source:options.sources) source.inputProtocol="ZBUF";
        if (scenario=="zbuf-managed") { options.catalogRoot=root+"/catalog";CreateDirectory(options.catalogRoot); }
        if(scenario.find("fdt-")==0) {
            for(auto &source:options.sources) source.inputProtocol="FDT";
            options.catalogRoot=root+"/catalog";CreateDirectory(options.catalogRoot);
        }
        if (scenario=="disk-reserve") {
            options.sources[0].minFreeMiB=1048576;
            DataSupervisor s(options);
            Reject([&] { s.Start(root+"/start"); },"configured minimum free-space reserve");
            Require(s.Status().state==DataSupervisorStatus::Fresh && s.Status().receivers.empty(),"Disk preflight changed the fresh supervisor state");
            struct stat st; Require(::lstat((root+"/start").c_str(),&st)!=0 && errno==ENOENT,"Disk preflight created a start-operation directory");
            Require(ReadFile(options.auditRoot+"/supervisor.jsonl",65536).find("\"event\":\"start-intent\"")==std::string::npos,"Disk preflight published a start intent");
            DIR *directory=::opendir((root+"/outputs").c_str()); Require(directory!=nullptr,"Open fixture output root");
            bool hasRunDirectory=false;
            while (auto *entry=::readdir(directory)) {
                const std::string name=entry->d_name;
                if (name!="." && name!="..") hasRunDirectory=true;
            }
            ::closedir(directory); Require(!hasRunDirectory,"Disk preflight created a receiver run directory");
        } else if (scenario=="validation") {
            auto invalid=options; invalid.maxTotalQueueMiB=1;
            Reject([&] { DataSupervisor wrong(invalid); },"aggregate");
            invalid=options; invalid.sources[1].dataPort=invalid.sources[0].dataPort; invalid.sources[1].receiverAddress=invalid.sources[0].receiverAddress;
            Reject([&] { DataSupervisor wrong(invalid); },"port");
            invalid=options; invalid.sources[0].receiverAddress="192.0.2.1:"+std::to_string(invalid.sources[0].dataPort);
            Reject([&] { DataSupervisor wrong(invalid); },"explicit IPv4");
            struct stat st; Require(::lstat(options.auditRoot.c_str(),&st)!=0 && errno==ENOENT,"Invalid plan created a persistent deployment");
        } else if (scenario=="reopen") {
            { DataSupervisor s(options); Reject([&] { DataSupervisor duplicate(options); },"locked"); Cycle(s,options,root,1); }
            DataSupervisor s(options); Require(s.Status().needsReconciliation && s.Status().receivers.empty(),"Reopen implicitly adopted/restarted old PIDs"); Reject([&] { s.Start(root+"/restart"); },"reconciliation");
        } else {
            DataSupervisor s(options);
            try {
                if(scenario=="naming-identity") {
                    Reject([&] {s.Start(root+"/start");},"file naming differs");
                    Require(s.Status().state==DataSupervisorStatus::Failed,"Naming mismatch did not keep failure gate");
                    Reject([&] {s.Start(root+"/retry");},"reconciliation");
                } else if(scenario=="protocol-identity") {
                    Reject([&] {s.Start(root+"/start");},"input protocol differs");
                    Require(s.Status().state==DataSupervisorStatus::Failed,"Protocol mismatch lost failure gate");
                    Reject([&] {s.Start(root+"/retry");},"reconciliation");
                } else Run(s,options,root,(scenario=="get-naming" || scenario=="zbuf-managed") ? "catalog" : scenario);
            } catch (...) { Cleanup(s); throw; }
            Cleanup(s);
        }
        const bool actual=scenario!="validation" && scenario!="disk-reserve" && scenario!="storage" && scenario!="journal" && scenario!="lease";
        WriteNew(root+"/fixture-result.json","{\"scenario\":"+EncodeJsonString(scenario)+",\"passed\":true,\"actual_receiver_processes\":"+(actual ? "true" : "false")+",\"real_hardware\":false,\"throughput_benchmark\":false}\n");
        std::cout<<"PASS supervisor "<<scenario<<": actual native receiver lifecycle; synthetic local TCP, no real hardware/throughput proof\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
