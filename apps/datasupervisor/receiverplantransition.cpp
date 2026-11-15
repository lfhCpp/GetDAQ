#include "receiverplantransition.h"
#include "receiverplantransition_p.h"
#include "supervisorio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>
namespace GetDAQ {
using ReceiverPlanTransitionInternal::BoundFile;
namespace {
using namespace SupervisorIO;
void Require(bool value,const char *message) {if (!value) throw std::runtime_error(message);}

}
class SupervisorNextPlan::Impl {
public:
    Directory directory;
    BoundFile file;
    SupervisorPlanDocument plan;
    mutable std::atomic<bool> faulted{false};
    Impl(const std::string &path,const std::string &sha,const DataSupervisorOptions &old,const std::string &executable) :
        directory(path.substr(0,path.find_last_of('/'))),file(path,1024*1024,0400) {
        Path(path);Digest(sha);Require(ConfigurationSha256(file.Bytes())==sha,"Reviewed successor plan SHA differs");
        plan=ReceiverPlanTransitionInternal::ValidateNextPlan(file.Bytes(),old,executable);ReceiverPlanTransitionInternal::EnsureAbsent(plan.options.auditRoot);Check();
    }
    void Check() const {
        if (faulted) throw std::runtime_error("Reviewed successor plan fault remains latched");
        try {directory.Check();file.Check();ReceiverPlanTransitionInternal::EnsureAbsent(plan.options.auditRoot);} catch (...) {faulted=true;throw;}
    }
};
SupervisorNextPlan::SupervisorNextPlan(const std::string &file,const std::string &sha,const DataSupervisorOptions &old,const std::string &executable) : fImpl(new Impl(file,sha,old,executable)) {}
SupervisorNextPlan::~SupervisorNextPlan() {}
void SupervisorNextPlan::Check() const {fImpl->Check();}
std::string SupervisorNextPlan::Bytes() const {Check();return fImpl->file.Bytes();}
SupervisorPlanDocument SupervisorNextPlan::Document() const {Check();return fImpl->plan;}

std::string StageScanSuccessorPlan(const std::string &source,const std::string &target,const DataSupervisorOptions &old,const std::string &executable) {
    Path(source);
    return StageScanSuccessorPlanBytes(ReadFile(source,1024*1024),target,old,executable);
}
void PreflightScanSuccessorPlanBytes(const std::string &bytes,const std::string &target,const DataSupervisorOptions &old,const std::string &executable) {
    Path(target);Require(bytes.size()<=1024*1024,"Successor plan exceeds its 1 MiB bound");
    const auto plan=DecodeSupervisorPlan(bytes);ValidateOptions(plan.options);
    Require(EncodeOptions(plan.options,plan.executableSha256)==bytes,"Successor receiver plan is not canonical");
    const std::string prefix=old.auditRoot+".scan-";
    Require(plan.options.auditRoot.compare(0,prefix.size(),prefix)==0,"Scan successor audit root must extend the original receiver root");
    const auto pointAt=plan.options.auditRoot.find("/point-",prefix.size());
    Require(pointAt!=std::string::npos && pointAt+10==plan.options.auditRoot.size(),"Invalid scan successor audit root");
    const std::string executionId=plan.options.auditRoot.substr(prefix.size(),pointAt-prefix.size());
    const std::string point=plan.options.auditRoot.substr(pointAt+7);
    Text(executionId,96);Require(executionId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,"Invalid scan execution identifier in successor audit root");
    Require(point.size()==3 && point.find_first_not_of("0123456789")==std::string::npos,"Invalid scan point in successor audit root");
    Require(plan.options.deploymentId=="scan-"+ConfigurationSha256(executionId).substr(0,24)+"-point-"+point,"Scan successor deployment identity does not match audit root");
    (void)ReceiverPlanTransitionInternal::ValidateNextPlan(bytes,old,executable,false);
    const std::string scanParent=plan.options.auditRoot.substr(0,pointAt);
    const std::string oldParent=old.auditRoot.substr(0,old.auditRoot.find_last_of('/'));
    const std::string oldLeaf=old.auditRoot.substr(old.auditRoot.find_last_of('/')+1);
    Require(scanParent==oldParent+"/"+oldLeaf+".scan-"+executionId,"Scan audit parent is outside the original receiver root parent");
    ReceiverPlanTransitionInternal::EnsureAbsent(target);Directory targetDirectory(target.substr(0,target.find_last_of('/')));
    const std::string parentPath=oldParent.empty()?"/":oldParent;
    char *resolved=::realpath(parentPath.c_str(),nullptr);const bool canonical=resolved && parentPath==resolved;::free(resolved);
    Fd base(::open(parentPath.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat baseIdentity,namedBase;
    Require(canonical && base.value>=0 && !::fstat(base.value,&baseIdentity) && !::lstat(parentPath.c_str(),&namedBase) &&
        Same(baseIdentity,namedBase) && S_ISDIR(baseIdentity.st_mode) && baseIdentity.st_uid==::geteuid(),
        "Original receiver audit parent must be canonical and owned by the service user");
    const std::string scanLeaf=oldLeaf+".scan-"+executionId;
    Fd scan(::openat(base.value,scanLeaf.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    if(scan.value>=0) {
        struct stat held,named;
        Require(!::fstat(scan.value,&held) && !::fstatat(base.value,scanLeaf.c_str(),&named,AT_SYMLINK_NOFOLLOW) && Same(held,named) &&
            S_ISDIR(held.st_mode) && held.st_uid==::geteuid() && (held.st_mode&07777)==0700,
            "Existing scan successor audit parent has unsafe identity or permissions");
    } else Require(errno==ENOENT,"Cannot inspect scan successor audit parent");
}
std::string StageScanSuccessorPlanBytes(const std::string &bytes,const std::string &target,const DataSupervisorOptions &old,const std::string &executable) {
    PreflightScanSuccessorPlanBytes(bytes,target,old,executable);
    const auto plan=DecodeSupervisorPlan(bytes);ValidateOptions(plan.options);
    Require(EncodeOptions(plan.options,plan.executableSha256)==bytes,"Successor receiver plan is not canonical");
    const std::string prefix=old.auditRoot+".scan-";
    Require(plan.options.auditRoot.compare(0,prefix.size(),prefix)==0,"Scan successor audit root must extend the original receiver root");
    const auto pointAt=plan.options.auditRoot.find("/point-",prefix.size());
    Require(pointAt!=std::string::npos && pointAt+10==plan.options.auditRoot.size(),"Invalid scan successor audit root");
    const std::string executionId=plan.options.auditRoot.substr(prefix.size(),pointAt-prefix.size());
    const std::string point=plan.options.auditRoot.substr(pointAt+7);
    Text(executionId,96);Require(executionId.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-")==std::string::npos,
        "Invalid scan execution identifier in successor audit root");
    Require(point.size()==3 && point.find_first_not_of("0123456789")==std::string::npos,"Invalid scan point in successor audit root");
    Require(plan.options.deploymentId=="scan-"+ConfigurationSha256(executionId).substr(0,24)+"-point-"+point,
        "Scan successor deployment identity does not match audit root");
    (void)ReceiverPlanTransitionInternal::ValidateNextPlan(bytes,old,executable,false);
    const std::string scanParent=plan.options.auditRoot.substr(0,pointAt);
    const std::string oldParent=old.auditRoot.substr(0,old.auditRoot.find_last_of('/'));
    const std::string oldLeaf=old.auditRoot.substr(old.auditRoot.find_last_of('/')+1);
    Require(scanParent==oldParent+"/"+oldLeaf+".scan-"+executionId,"Scan audit parent is outside the original receiver root parent");

    ReceiverPlanTransitionInternal::EnsureAbsent(target);
    const std::string targetParent=target.substr(0,target.find_last_of('/'));
    Directory targetDirectory(targetParent);
    const std::string parentPath=oldParent.empty()?"/":oldParent;
    char *resolved=::realpath(parentPath.c_str(),nullptr);const bool canonical=resolved && parentPath==resolved;::free(resolved);
    Fd base(::open(parentPath.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat baseIdentity,namedBase;
    Require(canonical && base.value>=0 && !::fstat(base.value,&baseIdentity) && !::lstat(parentPath.c_str(),&namedBase) &&
        Same(baseIdentity,namedBase) && S_ISDIR(baseIdentity.st_mode) && baseIdentity.st_uid==::geteuid(),
        "Original receiver audit parent must be canonical and owned by the service user");
    const std::string scanLeaf=oldLeaf+".scan-"+executionId;
    if(::mkdirat(base.value,scanLeaf.c_str(),0700) && errno!=EEXIST)throw std::runtime_error("Cannot create scan successor audit parent");
    Fd scan(::openat(base.value,scanLeaf.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));struct stat scanIdentity,namedScan;
    Require(scan.value>=0 && !::fstat(scan.value,&scanIdentity) && !::fstatat(base.value,scanLeaf.c_str(),&namedScan,AT_SYMLINK_NOFOLLOW) &&
        Same(scanIdentity,namedScan) && S_ISDIR(scanIdentity.st_mode) && scanIdentity.st_uid==::geteuid() &&
        (scanIdentity.st_mode&07777)==0700,"Scan successor audit parent has unsafe identity or permissions");
    if(::fsync(base.value))throw std::runtime_error("Cannot sync scan successor audit parent");
    Directory scanDirectory(scanParent);
    WriteNew(target,bytes);
    Fd staged(::open(target.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    Require(staged.value>=0 && !::fchmod(staged.value,0400) && !::fsync(staged.value) && !::fsync(targetDirectory.Descriptor()),
        "Cannot seal staged scan successor plan");
    const std::string sha=ConfigurationSha256(bytes);
    SupervisorNextPlan reviewed(target,sha,old,executable);reviewed.Check();
    scanDirectory.Check();targetDirectory.Check();
    return "{\"schema\":1,\"kind\":\"getdaq.staged-scan-successor-plan\",\"plan_file\":"+EncodeJsonString(target)+
        ",\"plan_sha256\":"+EncodeJsonString(sha)+",\"audit_parent\":"+EncodeJsonString(scanParent)+
        ",\"staged\":true,\"plan_retired\":false,\"receiver_started\":false}\n";
}


}
