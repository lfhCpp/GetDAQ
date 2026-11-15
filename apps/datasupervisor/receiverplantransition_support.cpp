#include "receiverplantransition_p.h"
#include "configurationsha256.h"
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ { namespace ReceiverPlanTransitionInternal {
using namespace SupervisorIO;

void Require(bool value,const char *message) {
    if(!value)throw std::runtime_error(message);
}

namespace {
bool Stable(const struct stat &a,const struct stat &b) {
#ifdef __APPLE__
    const bool times=a.st_mtimespec.tv_sec==b.st_mtimespec.tv_sec && a.st_mtimespec.tv_nsec==b.st_mtimespec.tv_nsec && a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec && a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec;
#else
    const bool times=a.st_mtim.tv_sec==b.st_mtim.tv_sec && a.st_mtim.tv_nsec==b.st_mtim.tv_nsec && a.st_ctim.tv_sec==b.st_ctim.tv_sec && a.st_ctim.tv_nsec==b.st_ctim.tv_nsec;
#endif
    return Same(a,b) && a.st_size==b.st_size && a.st_mode==b.st_mode && a.st_uid==b.st_uid && times;
}
}

void EnsureAbsent(const std::string &path) {
    struct stat st;
    Require(::lstat(path.c_str(),&st)!=0 && errno==ENOENT,
            "Plan activation requires an absent new root/activation marker");
}

void SealMetadata(const std::string &file) {
    Fd fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    Require(fd.value>=0 && !::fchmod(fd.value,0400) && !::fsync(fd.value),
            "Cannot seal data plan transition metadata");
}

BoundFile::BoundFile(const std::string &file,std::size_t maximum,unsigned int mode) : path(file),
    fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)) {
    Require(fd.value>=0 && !::fstat(fd.value,&identity),"Cannot open bound plan transition file");
    Require(S_ISREG(identity.st_mode) && identity.st_uid==::geteuid() && identity.st_nlink==1 &&
        (identity.st_mode&07777)==mode,"Invalid plan transition file owner/type/permissions");
    bytes=ReadFile(path,maximum);Check();
}

void BoundFile::Check() const {
    struct stat held,named;
    Require(!::fstat(fd.value,&held) && !::lstat(path.c_str(),&named) && Stable(held,identity) && Stable(named,identity) &&
        held.st_nlink==1,"Bound plan transition file changed");
}

SupervisorPlanDocument ValidateNextPlan(const std::string &bytes,const DataSupervisorOptions &old,
                                        const std::string &executable,bool requireAuditParent) {
    const auto plan=DecodeSupervisorPlan(bytes);ValidateOptions(plan.options);
    Require(EncodeOptions(plan.options,plan.executableSha256)==bytes,"Successor receiver plan is not canonical");
    Require(plan.options.auditRoot!=old.auditRoot && plan.options.deploymentId!=old.deploymentId,
            "Successor plan requires a fresh deployment/root");
    const auto separator=plan.options.auditRoot.find_last_of('/');
    const auto parent=separator ? plan.options.auditRoot.substr(0,separator) : "/";
    char *resolved=nullptr;
    if(requireAuditParent) {
        Fd parentFd(::open(parent.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
        resolved=::realpath(parent.c_str(),nullptr);
        const bool canonical=resolved && parent==resolved;
        ::free(resolved);resolved=nullptr;
        Require(parentFd.value>=0 && canonical && !::access(parent.c_str(),W_OK|X_OK),
                "Successor audit parent must already exist at a writable canonical path");
    }
    resolved=::realpath(old.auditRoot.c_str(),nullptr);
    Require(resolved!=nullptr,"Cannot resolve original deployment root");
    const std::string originalRoot=resolved;::free(resolved);
    Require(plan.options.auditRoot.compare(0,old.auditRoot.size()+1,old.auditRoot+'/')!=0 &&
        old.auditRoot.compare(0,plan.options.auditRoot.size()+1,plan.options.auditRoot+'/')!=0 &&
        plan.options.auditRoot.compare(0,originalRoot.size()+1,originalRoot+'/')!=0 &&
        originalRoot.compare(0,plan.options.auditRoot.size()+1,plan.options.auditRoot+'/')!=0,
        "Plan generation audit roots must not contain one another");
    for(const auto &source:plan.options.sources) {Directory output(source.outputRoot);output.Check();}
    Require(plan.options.receiverExecutable==old.receiverExecutable && plan.executableSha256==executable &&
        plan.options.iceReceiverExecutable==old.iceReceiverExecutable && plan.options.iceReceiverSha256==old.iceReceiverSha256,
        "Plan transition cannot replace or introduce receiver executables");
    Require(plan.options.maxTotalQueueMiB<=old.maxTotalQueueMiB && plan.options.maxTotalFrameMiB<=old.maxTotalFrameMiB,
            "Successor plan exceeds original host memory policy");
    Require(ConfigurationSha256(ReadFile(plan.options.receiverExecutable,64*1024*1024))==executable,
            "Successor receiver executable changed");
    if(!plan.options.iceReceiverExecutable.empty())
        Require(ConfigurationSha256(ReadFile(plan.options.iceReceiverExecutable,64*1024*1024))==plan.options.iceReceiverSha256,
                "Successor ICE executable changed");
    return plan;
}

} }
