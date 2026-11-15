#include "recoveryservicepolicy_p.h"
#include "configurationsha256.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include <QHostAddress>
#include <cstdlib>
#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace GetDAQ { namespace RecoveryPrivate {
using namespace SupervisorIO;

RecoveryServicePolicy::RecoveryServicePolicy(const std::string &file)
    : config(new PinnedFile(file,65536)),lease(-1),port(0) {
    const auto v=ParseStrictJson(config->bytes);
    ExactSupervisorFields(v,{"schema","kind","service_id","bind","port","credential_file","executable","executable_sha256","old_config","new_config","successor_plan","original_evidence","audit_root"});
    RequireRecovery(Unsigned(v.At("schema"))==1 && String(v.At("kind"))=="getdaq.cold-handoff-service","Unsupported recovery configuration");
    identity.serviceId=String(v.At("service_id"));identity.instanceId=RandomSession();identity.policySha256=std::string(64,'0');identity.originalRequestSha256=identity.successorPlanSha256=identity.policySha256;
    identity.originalOperationId="validation";EncodeColdHandoffIdentity(identity);
    bind=String(v.At("bind"));port=SupervisorUnsigned32(v.At("port"));RequireRecovery(!QHostAddress(QString::fromStdString(bind)).isNull() && port<65536,"Invalid recovery bind/port");
    credential.reset(new SupervisorCredential(String(v.At("credential_file"))));
    oldConfig=String(v.At("old_config"));newConfig=String(v.At("new_config"));plan=String(v.At("successor_plan"));originalEvidence=String(v.At("original_evidence"));executable=String(v.At("executable"));
    root.reset(new Directory(String(v.At("audit_root"))));
    char *canonical=::realpath(root->Path().c_str(),nullptr);const bool exact=canonical && root->Path()==canonical;::free(canonical);RequireRecovery(exact,"Recovery audit root must be canonical");
    Directory originalDirectory(originalEvidence); // The CLI rechecks the original directory for each task; startup does not hold the original ECC lock.
    std::string policy="{\"schema\":1,\"kind\":\"getdaq.cold-handoff-service-policy\",\"configuration_sha256\":"+EncodeJsonString(ConfigurationSha256(config->bytes))+",\"inputs\":[";
    for(const auto &path:{oldConfig,newConfig,plan,originalEvidence+"/request.json",originalEvidence+"/original-status.json",executable}) {
        inputs.emplace_back(new PinnedFile(path,path==executable ? 64*1024*1024 : 1024*1024,path==executable,path==plan));
        if(policy.back()!='[')policy+=',';
        policy+="{\"path\":"+EncodeJsonString(path)+",\"sha256\":"+EncodeJsonString(ConfigurationSha256(inputs.back()->bytes))+"}";
    }
    RequireRecovery(ConfigurationSha256(inputs.back()->bytes)==String(v.At("executable_sha256")),"Recovery executable digest differs");
    original=DecodeEccControlRequest(inputs[3]->bytes);before=DecodeEccControlSystemStatus(inputs[4]->bytes);
    RequireRecovery(original.command=="retire-data-plans" || original.command=="handoff-data-control","Recovery original request is not a data handoff");
    RequireRecovery(EncodeEccControlRequest(original)==inputs[3]->bytes && before.instanceId==original.instanceId && before.snapshotSha256==original.snapshotSha256,"Recovery original pre-state differs");
    identity.originalOperationId=original.operationId;identity.originalRequestSha256=ConfigurationSha256(inputs[3]->bytes);identity.successorPlanSha256=ConfigurationSha256(inputs[2]->bytes);
    RequireRecovery(identity.successorPlanSha256==original.successorPlanSha256,"Recovery successor plan differs from original request");
    policy+="]}\n";identity.policySha256=ConfigurationSha256(policy);EncodeColdHandoffIdentity(identity);
    lease.value=::openat(root->Descriptor(),"service.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600);
    RequireRecovery(lease.value>=0 && !::fstat(lease.value,&leaseIdentity) && S_ISREG(leaseIdentity.st_mode) && leaseIdentity.st_uid==::geteuid() &&
        (leaseIdentity.st_mode&0777)==0600 && leaseIdentity.st_nlink==1 && !::flock(lease.value,LOCK_EX|LOCK_NB),"Recovery audit root already owned/invalid");
    if(!RecoveryPathExists(root->Path()+"/policy.json"))WriteNew(root->Path()+"/policy.json",policy);
    policyFile.reset(new PinnedFile(root->Path()+"/policy.json",65536));RequireRecovery(policyFile->bytes==policy,"Recovery immutable policy differs; use its original fixed inputs");
}

void RecoveryServicePolicy::Check() const {
    config->Check();credential->Check();root->Check();policyFile->Check();
    for(const auto &input:inputs)input->Check();
    struct stat named,held;
    RequireRecovery(!::fstat(lease.value,&held) && !::fstatat(root->Descriptor(),"service.lock",&named,AT_SYMLINK_NOFOLLOW) &&
        Same(named,leaseIdentity) && Same(named,held) && (named.st_mode&0777)==0600,"Recovery service lease changed");
}

bool RecoveryServicePolicy::Authorize(const std::string &header) const {return credential->Authorize(header);}
Directory &RecoveryServicePolicy::Root() const {return *root;}
const ColdHandoffIdentity &RecoveryServicePolicy::Identity() const {return identity;}
const EccControlRequest &RecoveryServicePolicy::OriginalRequest() const {return original;}
const EccControlSystemStatus &RecoveryServicePolicy::OriginalStatus() const {return before;}
const std::string &RecoveryServicePolicy::BindAddress() const {return bind;}
const std::string &RecoveryServicePolicy::OldConfig() const {return oldConfig;}
const std::string &RecoveryServicePolicy::NewConfig() const {return newConfig;}
const std::string &RecoveryServicePolicy::SuccessorPlan() const {return plan;}
const std::string &RecoveryServicePolicy::OriginalEvidence() const {return originalEvidence;}
const std::string &RecoveryServicePolicy::Executable() const {return executable;}
unsigned int RecoveryServicePolicy::Port() const {return port;}

} }
