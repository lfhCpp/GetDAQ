#include "eccsuccessorplan.h"
#include "eccprotocol.h"
#include "fixedeccconfiguration.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include "supervisorio.h"
#include <atomic>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <mutex>
#include <stdexcept>
#include <sys/file.h>
#include <unistd.h>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
void Missing(const std::string &file) {
    struct stat st;
    if (::lstat(file.c_str(),&st)==0 || errno!=ENOENT)
        throw std::runtime_error("Successor startup requires absent audit root/activation marker");
}
std::string Parent(const std::string &file) {
    Path(file);
    const auto parent=file.substr(0,file.find_last_of('/'));
    char *resolved=::realpath(parent.c_str(),nullptr);
    const bool canonical=resolved && parent==resolved;
    ::free(resolved);
    if (!canonical) throw std::invalid_argument("Successor plan requires an existing canonical private parent directory");
    return parent;
}
void Seal(const std::string &file) {
    Fd fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    if (fd.value<0 || ::fchmod(fd.value,0400) || ::fsync(fd.value))
        throw std::runtime_error("Cannot seal successor plan file");
}
void Identity(int fd,const std::string &file,const struct stat &expected,const std::string &bytes) {
    struct stat held,current;
    if (::fstat(fd,&held) || ::lstat(file.c_str(),&current) || !Same(held,expected) || !Same(current,expected) ||
        !S_ISREG(current.st_mode) || current.st_uid!=::geteuid() || current.st_nlink!=1 ||
        (current.st_mode&07777)!=0400 || held.st_size!=static_cast<off_t>(bytes.size()) ||
        ReadFile(file,65536)!=bytes)
        throw std::runtime_error("Successor plan file identity/content/permissions changed");
}
std::string Encode(const std::string &file,FixedEccConfiguration &configuration,const std::string &epoch) {
    configuration.Check();
    RemoteReceiverManager data(configuration.ReceiverOptions());
    const auto options=configuration.ControllerOptions();
    const auto plan=PlanNativeEccConfiguration(options,data.BindingSha256());
    EccSuccessorIdentity identity;identity.planFile=file;identity.serviceId=configuration.ServiceId();identity.controllerId=configuration.ReceiverOptions().controllerId;
    identity.inputBindingSha256=configuration.BindingSha256();identity.snapshotSha256=plan.snapshotSha256;identity.dataBindingSha256=data.BindingSha256();
    identity.scopeSha256=plan.dataControllerScopeSha256;identity.auditRoot=options.auditRoot;identity.epoch=epoch;
    const auto result=EncodeEccSuccessorIdentity(identity);
    configuration.Check();
    return result;
}
}
class EccSuccessorReview::Impl {
public:
    std::string file,bytes;
    Directory parent;
    Fd fd;
    struct stat identity;
    mutable std::atomic<bool> faulted{false};
    mutable std::mutex mutex;
    bool released=false;
    explicit Impl(const EccControlRequest &request) : file(request.successorPlanFile),parent(Parent(file)),
        fd(::open(file.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)) {
        ValidateEccControlRequest(request);
        if(request.command!="retire-data-plans" || file.empty() || fd.value<0 || ::fstat(fd.value,&identity))throw std::invalid_argument("Cannot bind reviewed successor plan lease");
        bytes=ReadFile(file,65536);Identity(fd.value,file,identity,bytes);
        if(::flock(fd.value,LOCK_SH|LOCK_NB))throw std::runtime_error("Reviewed successor identity is held by another startup");
        const auto target=DecodeEccSuccessorIdentity(bytes);
        if(EncodeEccSuccessorIdentity(target)!=bytes || target.planFile!=file || ConfigurationSha256(bytes)!=request.successorPlanSha256 ||
            target.scopeSha256!=request.successorScopeSha256 || target.epoch!=request.successorEpoch || target.dataBindingSha256!=request.successorDataBindingSha256)
            throw std::invalid_argument("Reviewed successor identity differs from retirement request");
        Missing(target.auditRoot);Missing(file+".activation.json");CheckLocked();
    }
    void CheckLocked() const {
        if(faulted)throw std::runtime_error("Reviewed successor identity fault remains latched");
        try {parent.Check();Identity(fd.value,file,identity,bytes);}catch(...){faulted=true;throw;}
    }
};
EccSuccessorReview::EccSuccessorReview(const EccControlRequest &request) : fImpl(new Impl(request)) {}
EccSuccessorReview::~EccSuccessorReview() {}
void EccSuccessorReview::Check() const {std::lock_guard<std::mutex> lock(fImpl->mutex);fImpl->CheckLocked();}
void EccSuccessorReview::Release() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.mutex);p.CheckLocked();
    if(!p.released) {if(::flock(p.fd.value,LOCK_UN)){p.faulted=true;throw std::runtime_error("Cannot release reviewed successor identity lease");}p.released=true;}
}
std::string EccSuccessorReview::Json() const {Check();return fImpl->bytes;}
class EccSuccessorPlan::Impl {
public:
    std::string file,bytes,markerBytes;
    Directory parent;
    Fd fd,marker;
    struct stat identity,markerIdentity;
    JsonValue document;
    mutable std::atomic<bool> faulted{false};
    std::atomic<bool> activated{false};
    std::mutex activationMutex;
    Impl(const std::string &path,FixedEccConfiguration &configuration) : file(path),parent(Parent(path)),
        fd(::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC)) {
        if (fd.value<0 || ::fstat(fd.value,&identity)) throw std::runtime_error("Cannot open successor plan");
        bytes=ReadFile(file,65536);document=ParseStrictJson(bytes);
        const auto epoch=String(document.At("receiver_controller_epoch"));Digest(epoch);
        if (bytes!=Encode(file,configuration,epoch)) throw std::invalid_argument("Successor plan does not match exact fixed configuration/path");
        Check();
    }
    std::string Field(const std::string &key) const {return String(document.At(key));}
    void Check() const {
        if (faulted) throw std::runtime_error("Successor plan fault remains latched");
        try {
            parent.Check();Identity(fd.value,file,identity,bytes);
            if (activated) Identity(marker.value,file+".activation.json",markerIdentity,markerBytes);
        } catch (...) {faulted=true;throw;}
    }
};
std::string EccSuccessorPlan::Create(const std::string &file,FixedEccConfiguration &configuration) {
    Directory parent(Parent(file));
    const auto bytes=Encode(file,configuration,RandomSession());
    Missing(configuration.ControllerOptions().auditRoot);Missing(file+".activation.json");parent.Check();
    WriteNew(file,bytes);Seal(file);
    if (::fsync(parent.Descriptor())) throw std::runtime_error("Cannot sync successor plan directory");
    EccSuccessorPlan checked(file,configuration);checked.Check();return checked.Json();
}
EccSuccessorPlan::EccSuccessorPlan(const std::string &file,FixedEccConfiguration &configuration) : fImpl(new Impl(file,configuration)) {}
EccSuccessorPlan::~EccSuccessorPlan() {}
void EccSuccessorPlan::Check() const {fImpl->Check();}
void EccSuccessorPlan::ActivateForStartup() {
    auto &p=*fImpl;std::lock_guard<std::mutex> lock(p.activationMutex);p.Check();
    if (p.activated) throw std::runtime_error("Successor plan already activated by this object");
    if (::flock(p.fd.value,LOCK_EX|LOCK_NB)) throw std::runtime_error("Successor plan is already held by another startup");
    try {
        p.Check();Missing(p.Field("audit_root"));Missing(p.file+".activation.json");
        p.markerBytes="{\"schema\":1,\"kind\":\"getdaq.ecc-successor-activation\",\"plan_sha256\":"+
            EncodeJsonString(ConfigurationSha256(p.bytes))+",\"activation_id\":"+EncodeJsonString(RandomSession())+
            ",\"pid\":"+std::to_string(::getpid())+"}\n";
        WriteNew(p.file+".activation.json",p.markerBytes);Seal(p.file+".activation.json");
        p.marker.value=::open((p.file+".activation.json").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        if (p.marker.value<0 || ::fstat(p.marker.value,&p.markerIdentity) || ::fsync(p.parent.Descriptor()))
            throw std::runtime_error("Cannot bind successor activation marker");
        p.activated=true;p.Check();
    } catch (...) {p.faulted=true;throw;}
}
bool EccSuccessorPlan::Activated() const {return fImpl->activated;}
bool EccSuccessorPlan::Faulted() const {return fImpl->faulted;}
std::string EccSuccessorPlan::Json() const {Check();return fImpl->bytes;}
std::string EccSuccessorPlan::Sha256() const {return ConfigurationSha256(fImpl->bytes);}
std::string EccSuccessorPlan::Epoch() const {return fImpl->Field("receiver_controller_epoch");}
std::string EccSuccessorPlan::SnapshotSha256() const {return fImpl->Field("snapshot_sha256");}
std::string EccSuccessorPlan::ScopeSha256() const {return fImpl->Field("data_controller_scope_sha256");}
std::string EccSuccessorPlan::DataBindingSha256() const {return fImpl->Field("data_lifecycle_binding_sha256");}
std::string EccSuccessorPlan::AuditRoot() const {return fImpl->Field("audit_root");}
std::string EccSuccessorPlan::ControllerId() const {return fImpl->Field("controller_id");}
}
