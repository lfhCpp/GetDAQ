#include "transferjobs.h"
#include "transferjobs_p.h"
#include "receivercatalog.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "planio.h"
#include "../../common/text/utf8.h"
#include <chrono>
#include <atomic>
#include <dirent.h>
#include <fcntl.h>
#include <iomanip>
#include <map>
#include <mutex>
#include <spawn.h>
#include <sstream>
#include <stdexcept>
#include <sys/file.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
extern char **environ;
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
void Require(bool value,const char *message) {if(!value)throw std::runtime_error(message);}
void Component(const std::string &s) {Require(!s.empty() && s.size()<=80 && s.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_-")==std::string::npos && s[0]!='_' && s[0]!='-',"Invalid transfer experiment/operation component");}
std::string Run(unsigned int run) {Require(run<unsigned(INT32_MAX),"Invalid transfer Run");std::ostringstream out;out<<"run_"<<std::setw(4)<<std::setfill('0')<<run;return out.str();}
std::string Policy(const TransferJobOptions &o) {
    return "{\"schema\":1,\"kind\":\"getdaq.transfer-policy\",\"agent_id\":"+EncodeJsonString(o.agentId)+",\"deployment_id\":"+EncodeJsonString(o.deploymentId)+",\"source_root\":"+EncodeJsonString(o.sourceRoot)+",\"backup_root\":"+EncodeJsonString(o.backupRoot)+",\"audit_root\":"+EncodeJsonString(o.auditRoot)+",\"executable\":"+EncodeJsonString(o.executable)+",\"executable_sha256\":"+EncodeJsonString(o.executableSha256)+"}\n";
}
void Atomic(const Directory &directory,const std::string &name,const std::string &bytes) {
    directory.Check();const auto pending=directory.Path()+'/'+name+".pending";WriteNew(pending,bytes);
    Require(!::rename(pending.c_str(),(directory.Path()+'/'+name).c_str()) && !::fsync(directory.Descriptor()),"Cannot persist transfer metadata");directory.Check();
}
}
TransferJobs::Impl::Impl(const TransferJobOptions &o)
    : options(o), source(o.sourceRoot), backup(o.backupRoot), root(o.auditRoot), lease(-1),
      policy(Policy(o)), active(false), reopenedPending(false), fault(false)
{
    ValidateTransferJobOptions(o);
    identity.agentId=o.agentId; identity.instanceId=RandomSession();
    identity.policySha256=ConfigurationSha256(policy); identity.sourceRoot=o.sourceRoot;
    identity.backupRoot=o.backupRoot;
    lease.value=::openat(root.Descriptor(),".getdaq-transfer-service.lock",
                         O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    Require(lease.value>=0 && !::fstat(lease.value,&leaseInfo) &&
            S_ISREG(leaseInfo.st_mode) && leaseInfo.st_uid==::geteuid() &&
            (leaseInfo.st_mode&0777)==0600 && !::flock(lease.value,LOCK_EX|LOCK_NB),
            "Transfer service root already owned/invalid");
    struct stat info;
    if(::fstatat(root.Descriptor(),"policy.json",&info,AT_SYMLINK_NOFOLLOW)) {
        Require(errno==ENOENT,"Cannot inspect transfer policy");
        WriteNew(root.Path()+"/policy.json",policy);
    }
    Require(ReadFile(root.Path()+"/policy.json",16384)==policy &&
            !::fstatat(root.Descriptor(),"policy.json",&policyInfo,AT_SYMLINK_NOFOLLOW),
            "Transfer immutable policy differs");
    Require(!::lstat(o.executable.c_str(),&executableInfo),"Bind transfer executable");
    Fd scan(::openat(root.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    DIR *entries=::fdopendir(scan.value);
    Require(entries,"Enumerate transfer jobs"); scan.value=-1;
    try {
        for(;;) {
            errno=0; const auto entry=::readdir(entries);
            if(!entry) { Require(!errno,"Transfer directory read failed"); break; }
            const std::string name=entry->d_name;
            if(name.substr(0,4)!="job-") continue;
            Component(name.substr(4));
            Require(jobs.size()<256,"Transfer job history exceeds 256 operations");
            Directory directory(root.Path()+'/'+name);
            auto operation=DecodeTransferOperation(ReadFile(directory.Path()+"/operation.json",128*1024));
            Require(operation.request.operationId==name.substr(4) &&
                    operation.request.policySha256==identity.policySha256,
                    "Reopened transfer job binding differs");
            CheckIntent(directory,operation);
            if(operation.phase=="accepted" || operation.phase=="running" || operation.phase=="verifying") {
                operation.phase="unknown";
                operation.message="Service reopened an unfinished copy; no PID adoption, kill or replay";
                operation.receipt.clear();
            }
            if(operation.phase=="unknown") reopenedPending=true;
            Require(jobs.emplace(operation.request.operationId,operation).second,"Duplicate transfer job");
        }
    } catch(...) { ::closedir(entries); throw; }
    ::closedir(entries);
    Check();
}

TransferJobs::Impl::~Impl() { if(worker.joinable()) worker.join(); }

void TransferJobs::Impl::Check()
{
    if(fault) throw std::runtime_error("Transfer storage/executable fault is latched");
    try {
        source.Check(); backup.Check(); root.Check(); struct stat current,held;
        Require(!::fstatat(root.Descriptor(),"policy.json",&current,AT_SYMLINK_NOFOLLOW) &&
                Same(current,policyInfo) && current.st_uid==::geteuid() &&
                (current.st_mode&0777)==0600 && ReadFile(root.Path()+"/policy.json",16384)==policy,
                "Transfer policy changed");
        Require(!::fstatat(root.Descriptor(),".getdaq-transfer-service.lock",&current,AT_SYMLINK_NOFOLLOW) &&
                !::fstat(lease.value,&held) && Same(current,leaseInfo) && Same(current,held) &&
                (current.st_mode&0777)==0600,"Transfer service lock replaced");
        Require(!::lstat(options.executable.c_str(),&current) && Same(current,executableInfo) &&
                current.st_mode==executableInfo.st_mode && current.st_size==executableInfo.st_size &&
                ConfigurationSha256(ReadFile(options.executable,64*1024*1024))==options.executableSha256,
                "Transfer executable changed");
    } catch(...) { fault=true; throw; }
}

std::string TransferJobs::Impl::Source(const std::string &experiment,unsigned int run) const
{ Component(experiment); const auto result=options.sourceRoot+'/'+experiment+'/'+Run(run)+'/'+options.deploymentId; Path(result); return result; }

void TransferJobs::Impl::CheckIntent(const Directory &directory,const TransferOperation &operation) const
{
    directory.Check(); struct stat info;
    Require(!::fstatat(directory.Descriptor(),"request.json",&info,AT_SYMLINK_NOFOLLOW) &&
            S_ISREG(info.st_mode) && info.st_uid==::geteuid() && (info.st_mode&0777)==0600 &&
            info.st_nlink==1 && ReadFile(directory.Path()+"/request.json",16384)==EncodeTransferRequest(operation.request),
            "Transfer original intent changed");
    Require(operation.source==Source(operation.request.experiment,operation.request.runNumber) &&
            operation.destination==options.backupRoot+'/'+operation.request.experiment+'/'+
            Run(operation.request.runNumber)+'/'+options.deploymentId+"-copy-"+operation.request.operationId,
            "Transfer job fixed paths differ");
    directory.Check();
}

std::string TransferJobs::Impl::Receipt(const std::string &experiment,unsigned int run) const
{ return InspectReceiverCatalog(Source(experiment,run)); }

void TransferJobs::Impl::Save(const Directory &directory,const TransferOperation &operation)
{
    try { CheckIntent(directory,operation); Atomic(directory,"operation.json",EncodeTransferOperation(operation)); }
    catch(...) { fault=true; throw; }
}

void TransferJobs::Impl::CheckVerificationIntent(const Directory &directory,const std::string &intent)
{
    try {
        directory.Check(); struct stat info;
        Require(!::fstatat(directory.Descriptor(),"request.json",&info,AT_SYMLINK_NOFOLLOW) &&
                S_ISREG(info.st_mode) && info.st_uid==::geteuid() && (info.st_mode&0777)==0600 &&
                info.st_nlink==1 && ReadFile(directory.Path()+"/request.json",16384)==intent,
                "Verification intent identity/permissions/content changed");
        directory.Check();
    } catch(...) { fault=true; throw; }
}

TransferJobs::TransferJobs(const TransferJobOptions &o) : fImpl(new Impl(o)) {}
TransferJobs::~TransferJobs() {}
TransferIdentity TransferJobs::Identity() const {std::lock_guard<std::mutex> lock(fImpl->mutex);return fImpl->identity;}
TransferOperation TransferJobs::Operation(const std::string &id) const {Component(id);std::lock_guard<std::mutex> lock(fImpl->mutex);const auto it=fImpl->jobs.find(id);if(it==fImpl->jobs.end())throw std::out_of_range("Transfer operation not found");return it->second;}
std::string TransferJobs::SourceReceipt(const std::string &experiment,unsigned int run) const {auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.submitMutex);p.Check();return p.Receipt(experiment,run);}
TransferOperation TransferJobs::Submit(const TransferRequest &request) {
    auto &p=*fImpl;std::unique_lock<std::mutex> submit(p.submitMutex,std::try_to_lock);Require(submit.owns_lock(),"Transfer submission busy; not queued");EncodeTransferRequest(request);const auto key=TransferRequestKey(request);
    {std::lock_guard<std::mutex> lock(p.mutex);const auto existing=p.jobs.find(request.operationId);if(existing!=p.jobs.end()){Require(existing->second.requestKey==key,"Transfer ID reused with changed request");return existing->second;}Require(!p.active,"A native transfer job is running; not queued");}
    p.Check();Require(!p.reopenedPending,"Unfinished reopened transfer requires manual reconciliation; no replay");Require(request.instanceId==p.identity.instanceId && request.policySha256==p.identity.policySha256,"Transfer service instance/policy changed");
    {std::lock_guard<std::mutex> lock(p.mutex);Require(p.jobs.size()<256,"Transfer job history full");}
    const auto receipt=ParseStrictJson(p.Receipt(request.experiment,request.runNumber));Require(String(receipt.At("request_key"))==request.sourceRequestKey,"Transfer selected source receipt changed");
    const auto source=p.Source(request.experiment,request.runNumber),parent=p.options.backupRoot+'/'+request.experiment;struct stat info;if(::lstat(parent.c_str(),&info)){Require(errno==ENOENT,"Inspect transfer backup parent");CreateDirectory(parent);}Directory experiment(parent);
    const auto run=parent+'/'+Run(request.runNumber);if(::lstat(run.c_str(),&info)){Require(errno==ENOENT,"Inspect transfer Run parent");CreateDirectory(run);}Directory runDirectory(run);
    TransferOperation operation;operation.request=request;operation.requestKey=key;operation.phase="accepted";operation.source=source;operation.destination=run+'/'+p.options.deploymentId+"-copy-"+request.operationId;Path(operation.destination);
    const auto job=p.root.Path()+"/job-"+request.operationId;CreateDirectory(job);Directory directory(job);WriteNew(job+"/request.json",EncodeTransferRequest(request));p.Save(directory,operation);
    if(p.worker.joinable())p.worker.join();
    {std::lock_guard<std::mutex> lock(p.mutex);p.jobs[request.operationId]=operation;p.active=true;}
    try {p.worker=std::thread([&p,operation,job]{p.Work(operation,job);});}catch(...){std::lock_guard<std::mutex> lock(p.mutex);p.active=false;p.jobs[request.operationId].phase="unknown";p.reopenedPending=true;throw;}
    return operation;
}
TransferOperation TransferJobs::Reconcile(const TransferReconciliationRequest &request){return Recover(request,false);}
TransferOperation TransferJobs::Resume(const TransferReconciliationRequest &request){return Recover(request,true);}
TransferOperation TransferJobs::Recover(const TransferReconciliationRequest &request,bool resume) {
    auto &p=*fImpl;std::unique_lock<std::mutex> submit(p.submitMutex,std::try_to_lock);
    Require(submit.owns_lock(),"Transfer reconciliation busy; not queued");
    const auto intent=resume ? EncodeTransferResumeRequest(request) : EncodeTransferReconciliationRequest(request);Component(request.verificationId);Component(request.operationId);
    p.Check();Require(request.instanceId==p.identity.instanceId && request.policySha256==p.identity.policySha256,"Verification service instance/policy changed");
    TransferOperation operation;
    {std::lock_guard<std::mutex> lock(p.mutex);const auto found=p.jobs.find(request.operationId);Require(found!=p.jobs.end(),"Original transfer operation not found");operation=found->second;}
    Require(operation.requestKey==request.originalRequestKey,"Verification original request key differs");
    Directory job(p.root.Path()+"/job-"+request.operationId);p.CheckIntent(job,operation);
    const auto directoryName=job.Path()+(resume ? "/resume-" : "/verify-")+request.verificationId;struct stat existing;
    if(!::lstat(directoryName.c_str(),&existing)) {
        Directory prior(directoryName);const auto priorIntent=ReadFile(prior.Path()+"/request.json",16384);
        p.CheckVerificationIntent(prior,priorIntent);
        Require(priorIntent==intent,"Verification ID reused with changed request");
        return Operation(request.operationId); // Observe only; never spawn the same verification twice.
    }
    Require(errno==ENOENT,"Cannot inspect verification intent");
    {std::lock_guard<std::mutex> lock(p.mutex);Require(!p.active,"A native copy or verification is running; not queued");}
    Require(operation.phase=="unknown" || (resume && operation.phase=="failed"),"Only an unresolved original transfer can be recovered");
    unsigned int records=0;Fd scan(::openat(job.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));DIR *entries=::fdopendir(scan.value);Require(entries,"Enumerate verification history");scan.value=-1;
    for(;;){errno=0;const auto entry=::readdir(entries);if(!entry){const int error=errno;::closedir(entries);Require(!error,"Cannot enumerate verification history");break;}const std::string name=entry->d_name;if(name.substr(0,7)=="verify-" || name.substr(0,7)=="resume-")++records;}
    Require(records<64,"Original transfer verification history full; preserve records");
    CreateDirectory(directoryName);Directory directory(directoryName);WriteNew(directory.Path()+"/request.json",intent);
    operation.phase="verifying";operation.receipt.clear();operation.message=resume ? "Explicit native continuation accepted for the original partial target" : "Explicit independent native verification accepted; original copy will not be replayed";p.Save(job,operation);
    if(p.worker.joinable())p.worker.join();
    {std::lock_guard<std::mutex> lock(p.mutex);p.jobs[request.operationId]=operation;p.active=true;}
    try{p.worker=std::thread([&p,operation,directoryName,intent,resume]{p.Work(operation,directoryName,true,intent,resume);});}
    catch(...){std::lock_guard<std::mutex> lock(p.mutex);p.active=false;p.jobs[request.operationId].phase="unknown";p.reopenedPending=true;throw;}
    return operation;
}
}
