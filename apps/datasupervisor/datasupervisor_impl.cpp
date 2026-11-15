#include "datasupervisor_p.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <dirent.h>
#include <fcntl.h>
#include <stdexcept>
#include <sys/file.h>
#include <sys/statvfs.h>
#include <thread>
#include <unistd.h>

namespace GetDAQ {
namespace DataSupervisorInternal {
using Clock=std::chrono::steady_clock;

void CheckOutputReserve(const ReceiverSourcePlan &source) {
    if (!source.minFreeMiB) return;
    struct statvfs space;
    if (::statvfs(source.outputRoot.c_str(),&space))
        throw std::runtime_error("Cannot inspect output volume before receiver startup: "+source.outputRoot);
    const std::uint64_t unit=std::uint64_t(space.f_frsize ? space.f_frsize : space.f_bsize);
    const std::uint64_t blocks=std::uint64_t(space.f_bavail);
    const std::uint64_t available=unit && blocks>UINT64_MAX/unit ? UINT64_MAX : blocks*unit;
    const std::uint64_t configured=source.minFreeMiB*1024ull*1024ull;
    const std::uint64_t reserve=configured>UINT64_MAX-2ull*1024ull*1024ull ? UINT64_MAX : configured+2ull*1024ull*1024ull;
    if (available<reserve)
        throw std::runtime_error("Output volume is below the configured minimum free-space reserve for source "+source.id);
}
void Append(int fd,const std::string &bytes) {
    std::size_t offset=0;
    while (offset<bytes.size()) {
        const auto n=::write(fd,bytes.data()+offset,bytes.size()-offset);
        if (n<0 && errno==EINTR) continue;
        if (n<=0) throw std::runtime_error("Cannot append supervisor audit");
        offset+=std::size_t(n);
    }
    if (::fsync(fd)) throw std::runtime_error("Cannot fsync supervisor audit");
}
Audit::Audit(const std::string &p) : fd(-1),directory((CreateDirectory(p),p)),path(p+"/data-supervisor.jsonl") {
    WriteNew(path,"");
    fd.value=::open(path.c_str(),O_WRONLY|O_APPEND|O_NOFOLLOW|O_CLOEXEC);
    if (fd.value<0 || ::fstat(fd.value,&identity)) throw std::runtime_error("Cannot open supervisor operation audit");
}
void Audit::Check() const {
    directory.Check(); struct stat st,held;
    if (::lstat(path.c_str(),&st) || ::fstat(fd.value,&held) || !Same(st,identity) || !Same(st,held) ||
        !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || ReadFile(path,1024*1024)!=content)
        throw std::runtime_error("Supervisor operation audit identity/content changed");
}
void Audit::Row(const std::string &row) {
    Check(); if (content.size()+row.size()+1>1024*1024) throw std::runtime_error("Supervisor operation audit budget exhausted");
    Append(fd.value,row+'\n'); content+=row+'\n'; Check();
}
}
using namespace DataSupervisorInternal;
using namespace SupervisorIO;
using namespace ReceiverProcessInternal;

DataSupervisor::Impl::Impl(const DataSupervisorOptions &o) : options(o),lease(-1),journal(-1) {
        ValidateOptions(o);
        for (const auto &s:o.sources) if (!outputRoots.count(s.outputRoot)) outputRoots[s.outputRoot].reset(new Directory(s.outputRoot));
        if (::lstat(o.receiverExecutable.c_str(),&executableIdentity) || !S_ISREG(executableIdentity.st_mode) || !(executableIdentity.st_mode&0111) || (executableIdentity.st_mode&0022))
            throw std::invalid_argument("Supervisor executable must be a regular executable without group/other write permission");
        if (!o.iceReceiverExecutable.empty()) {
            if (::lstat(o.iceReceiverExecutable.c_str(),&iceExecutableIdentity) || !S_ISREG(iceExecutableIdentity.st_mode) || !(iceExecutableIdentity.st_mode&0111) || (iceExecutableIdentity.st_mode&0022) ||
                ConfigurationSha256(ReadFile(o.iceReceiverExecutable,64*1024*1024))!=o.iceReceiverSha256) throw std::invalid_argument("Supervisor ICE executable identity/SHA differs");
        }
        executableHash=ConfigurationSha256(ReadFile(o.receiverExecutable,64*1024*1024)); manifest=EncodeOptions(o,executableHash); planHash=ConfigurationSha256(manifest);
        struct stat st; const bool exists=::lstat(o.auditRoot.c_str(),&st)==0;
        if (!exists && errno!=ENOENT) throw std::runtime_error("Cannot inspect supervisor root");
        if (!exists) CreateDirectory(o.auditRoot);
        root.reset(new Directory(o.auditRoot));
        lease.value=::open((o.auditRoot+"/.supervisor.lock").c_str(),O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
        if (lease.value<0 || ::fstat(lease.value,&leaseIdentity) || !S_ISREG(leaseIdentity.st_mode) ||
            leaseIdentity.st_uid!=::geteuid() || (leaseIdentity.st_mode&0777)!=0600 || ::flock(lease.value,LOCK_EX|LOCK_NB)) throw std::runtime_error("Supervisor deployment is already locked/invalid");
        if (::fsync(root->Descriptor())) throw std::runtime_error("Cannot sync supervisor lease directory");
        if (!exists) { WriteNew(o.auditRoot+"/plan.json",manifest); WriteNew(o.auditRoot+"/supervisor.jsonl",""); }
        if (ReadFile(o.auditRoot+"/plan.json",1024*1024)!=manifest) throw std::runtime_error("Supervisor immutable plan differs; preserve old deployment");
        if (::lstat((o.auditRoot+"/plan.json").c_str(),&manifestIdentity)) throw std::runtime_error("Cannot bind supervisor plan identity");
        journal.value=::open((o.auditRoot+"/supervisor.jsonl").c_str(),O_RDWR|O_APPEND|O_NOFOLLOW|O_CLOEXEC);
        if (journal.value<0 || ::fstat(journal.value,&journalIdentity) || !S_ISREG(journalIdentity.st_mode)) throw std::runtime_error("Cannot open supervisor journal");
        const auto old=ReadFile(o.auditRoot+"/supervisor.jsonl",64*1024*1024); journalBytes=old.size(); journalContent=old;
        if (!old.empty() && old.back()!='\n') throw std::runtime_error("Truncated supervisor journal; do not repair/replay");
        std::size_t begin=0;
        while (begin<old.size()) {
            const auto end=old.find('\n',begin); const auto row=ParseStrictJson(old.substr(begin,end-begin));
            if (Unsigned(row.At("schema"))!=1 || String(row.At("plan_sha256"))!=planHash || Unsigned(row.At("sequence"))!=sequence++)
                throw std::runtime_error("Invalid supervisor journal scope/sequence");
            begin=end+1;
        }
        snapshot.planSha256=planHash; snapshot.needsReconciliation=exists;
        if (exists) { snapshot.state=DataSupervisorStatus::Failed; snapshot.message="Supervisor startup requires actual process/data reconciliation; no automatic restart"; }
        Record(exists ? "opened-reconciliation-required" : "created","\"receiver_executable_sha256\":"+EncodeJsonString(executableHash));
    }

DataSupervisor::Impl::~Impl() {
        // 只回收已经终止的自有子进程；仍在运行的接收器不发信号、不改日志。
        for (const auto &c:children) if (c->status.pid>0 && !c->status.reaped) try { if (Exited(c->status.pid)) Reap(c->status.pid); } catch (...) {}
    }

void DataSupervisor::Impl::Publish(DataSupervisorStatus::State state,const std::string &message) {
        std::lock_guard<std::mutex> lock(statusMutex); snapshot.state=state; snapshot.message=message; snapshot.sessionId=session; snapshot.receivers.clear();
        for (const auto &c:children) snapshot.receivers.push_back(c->status);
    }

void DataSupervisor::Impl::CheckStore() const {
        if (storageFault) throw std::runtime_error("Supervisor storage fault is latched; requires reconciliation");
        try {
        root->Check(); for (const auto &item:outputRoots) item.second->Check();
        struct stat st,held;
        if (::lstat((options.auditRoot+"/.supervisor.lock").c_str(),&st) || ::fstat(lease.value,&held) || !Same(st,leaseIdentity) || !Same(st,held) ||
            !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600) throw std::runtime_error("Supervisor deployment lease identity changed");
        if (::lstat(options.receiverExecutable.c_str(),&st) || !Same(st,executableIdentity) || !S_ISREG(st.st_mode) ||
            st.st_mode!=executableIdentity.st_mode || st.st_uid!=executableIdentity.st_uid || ConfigurationSha256(ReadFile(options.receiverExecutable,64*1024*1024))!=executableHash)
            throw std::runtime_error("Supervisor executable identity/content changed");
        if (!options.iceReceiverExecutable.empty() && (::lstat(options.iceReceiverExecutable.c_str(),&st) || !Same(st,iceExecutableIdentity) || !S_ISREG(st.st_mode) ||
            st.st_mode!=iceExecutableIdentity.st_mode || st.st_uid!=iceExecutableIdentity.st_uid || ConfigurationSha256(ReadFile(options.iceReceiverExecutable,64*1024*1024))!=options.iceReceiverSha256))
            throw std::runtime_error("Supervisor ICE executable identity/content changed");
        if (::lstat((options.auditRoot+"/plan.json").c_str(),&st) || !Same(st,manifestIdentity) || !S_ISREG(st.st_mode) ||
            st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 || ReadFile(options.auditRoot+"/plan.json",1024*1024)!=manifest)
            throw std::runtime_error("Supervisor immutable plan identity/content changed");
        if (::lstat((options.auditRoot+"/supervisor.jsonl").c_str(),&st) || ::fstat(journal.value,&held) || !Same(st,journalIdentity) || !Same(st,held) ||
            !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || (st.st_mode&0777)!=0600 ||
            st.st_size<0 || std::uint64_t(st.st_size)!=journalBytes || ReadFile(options.auditRoot+"/supervisor.jsonl",64*1024*1024)!=journalContent)
            throw std::runtime_error("Supervisor journal identity/content changed");
        } catch (...) {
            storageFault=true; std::lock_guard<std::mutex> lock(statusMutex); snapshot.needsReconciliation=true;
            snapshot.state=DataSupervisorStatus::Failed; snapshot.message="Supervisor storage fault; restoration does not clear reconciliation"; throw;
        }
    }

void DataSupervisor::Impl::Record(const std::string &event,const std::string &fields) {
        CheckStore();
        const auto row="{\"schema\":1,\"sequence\":"+std::to_string(sequence)+",\"event\":"+EncodeJsonString(event)+
            ",\"plan_sha256\":"+EncodeJsonString(planHash)+",\"session_id\":"+EncodeJsonString(session)+(fields.empty() ? "" : ','+fields)+"}\n";
        if (journalBytes+row.size()>64*1024*1024) throw std::runtime_error("Supervisor journal budget exhausted; no next process action");
        Append(journal.value,row); journalBytes+=row.size(); journalContent+=row; ++sequence;
        CheckStore();
    }

void DataSupervisor::Impl::Eligible() const { if (snapshot.needsReconciliation || snapshot.state==DataSupervisorStatus::Failed || snapshot.state==DataSupervisorStatus::Aborted) throw std::runtime_error("Supervisor requires reconciliation; no automatic cleanup/restart"); }

bool DataSupervisor::Impl::CanResumeCatalog(const std::string &replacementId,const std::string &experiment,unsigned int run,const std::string &expectedSession,const std::string &phase) const {
        if (!catalogFailureRecoverable || catalogFailureResumeAttempted || storageFault || replacementId.empty() ||
            replacementId!=catalogFailureReplacementId || experiment!=catalogFailureExperiment || run!=catalogFailureRun ||
            expectedSession!=catalogFailureSession || phase!=catalogFailurePhase || session!=expectedSession) return false;
        DataSupervisorStatus current;
        { std::lock_guard<std::mutex> lock(statusMutex); current=snapshot; }
        if (current.state!=DataSupervisorStatus::Failed || !current.needsReconciliation || current.sessionId!=expectedSession ||
            children.empty() || current.receivers.size()!=children.size()) return false;
        for (const auto &owned:children) {
            const auto &child=*owned;
            if (!child.status.reaped || child.status.pid<=0 || child.status.exitCode!=0 || !child.status.sealVerified ||
                child.status.sessionId!=expectedSession) return false;
            child.directory->Check();
        }
        try { CheckStore(); } catch (...) { return false; }
        return true;
    }

void DataSupervisor::Impl::CheckRetainedStoppedChildren() const {
        for (const auto &owned:children) {
            const auto &c=*owned;
            if (!c.status.reaped || c.status.pid<=0 || !c.lockBound || c.status.sessionId!=session)
                throw std::runtime_error("Failed-session retirement requires all original children reaped");
            c.directory->Check();
            Fd fd(::openat(c.directory->Descriptor(),".getdaq-receiver.lock",O_RDWR|O_NOFOLLOW|O_CLOEXEC));
            struct stat st;
            if (fd.value<0 || ::fstat(fd.value,&st) || !S_ISREG(st.st_mode) || !Same(st,c.lockIdentity) ||
                st.st_uid!=::geteuid() || (st.st_mode&0022) || ::flock(fd.value,LOCK_EX|LOCK_NB))
                throw std::runtime_error("Original receiver output lock changed or is still held");
        }
    }

void DataSupervisor::Impl::CheckRetirement() const {
        if (snapshot.state!=DataSupervisorStatus::Retired) return;
        try {
        if (!failedAbortComplete || !failedAbortAudit || !retirementAudit)
            throw std::runtime_error("Missing original failed-session retirement proof");
        failedAbortAudit->Check(); retirementAudit->Check(); CheckRetainedStoppedChildren();
        } catch (...) {
            storageFault=true;
            std::lock_guard<std::mutex> state(statusMutex); snapshot.needsReconciliation=true;
            snapshot.state=DataSupervisorStatus::Failed; snapshot.message="Original retirement proof changed; restoration does not clear the hold";
            throw;
        }
    }


}
