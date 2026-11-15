#include "controlproofs_p.h"

#include "configurationsha256.h"
#include "planio.h"
#include "strictjson.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;

void WriteOrVerifyRetirementFile(const std::string &path,const std::string &bytes,
                                 bool completion,bool write)
{
    struct stat st;
    if (::lstat(path.c_str(),&st)) {
        if (errno!=ENOENT) throw std::runtime_error("Cannot inspect retirement metadata");
        if (write) WriteNew(path,bytes); else return;
    } else if (!completion || !S_ISREG(st.st_mode) || st.st_uid!=::geteuid() || st.st_nlink!=1 ||
               ((st.st_mode&07777)!=0400 && (st.st_mode&07777)!=0600) || ReadFile(path,bytes.size()+1)!=bytes) {
        throw std::runtime_error("Existing retirement metadata differs; no overwrite");
    }
    if (!write) return;
    Fd file(::open(path.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
    struct stat held,named;
    if (file.value<0 || ::fstat(file.value,&held) || ::lstat(path.c_str(),&named) || !Same(held,named) ||
        !S_ISREG(held.st_mode) || held.st_uid!=::geteuid() || held.st_nlink!=1 ||
        ((held.st_mode&07777)!=0400 && (held.st_mode&07777)!=0600) || ReadFile(path,bytes.size()+1)!=bytes ||
        ((held.st_mode&07777)!=0400 && ::fchmod(file.value,0400)) || ::fsync(file.value))
        throw std::runtime_error("Cannot bind/seal retirement metadata");
}

SupervisorHandoffProof::SupervisorHandoffProof(const std::string &file,const std::string &expectedSha)
    : fPath(file),fBytes(ReadFile(file,4*1024*1024)),
      fDirectory(file.substr(0,file.find_last_of('/')))
{
    Digest(expectedSha);
    if (ConfigurationSha256(fBytes)!=expectedSha || ::lstat(fPath.c_str(),&fIdentity))
        throw std::runtime_error("Cannot bind original controller handoff proof");
    const auto requestPath=fDirectory.Path()+"/request.json";
    fRequestBytes=ReadFile(requestPath,2U*1024U*1024U);
    const auto request=DecodeSupervisorControlRequest(fRequestBytes);
    JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=131072;
    const auto proof=ParseStrictJson(fBytes,limits);
    fRetirement=request.command==SupervisorCommand::RetirePlan;
    if (fRetirement) {
        fCycleSha=String(proof.At("cycle_evidence_sha256"));Digest(fCycleSha);
        const auto cyclePath=fDirectory.Path()+"/cycle-evidence.json";
        if (ConfigurationSha256(ReadFile(cyclePath,16*1024*1024))!=fCycleSha ||
            ::lstat(cyclePath.c_str(),&fCycleIdentity))
            throw std::runtime_error("Original completed cycle evidence differs");
        fSuccessorBytes=ReadFile(fDirectory.Path()+"/successor-plan.json",1024*1024);
        if (ConfigurationSha256(fSuccessorBytes)!=request.successorPlanSha256 ||
            String(proof.At("successor_plan"))!=fSuccessorBytes ||
            ::lstat((fDirectory.Path()+"/successor-plan.json").c_str(),&fSuccessorIdentity))
            throw std::runtime_error("Original retirement successor plan copy differs");
    }
    if ((!fRetirement && request.command!=SupervisorCommand::HandoffControl) ||
        fRequestBytes!=EncodeSupervisorControlRequest(request) ||
        SupervisorControlRequestKey(request)!=String(proof.At("request_key")) ||
        ::lstat(requestPath.c_str(),&fRequestIdentity))
        throw std::runtime_error("Original handoff request binding differs");
    Check();
}

void SupervisorHandoffProof::Check() const
{
    fDirectory.Check();
    if (fRetirement) {
        struct stat cycle;const auto cyclePath=fDirectory.Path()+"/cycle-evidence.json";
        if (::lstat(cyclePath.c_str(),&cycle) || !Same(cycle,fCycleIdentity) || !S_ISREG(cycle.st_mode) ||
            cycle.st_uid!=::geteuid() || cycle.st_nlink!=1 || (cycle.st_mode&07777)!=0400 ||
            ConfigurationSha256(ReadFile(cyclePath,16*1024*1024))!=fCycleSha)
            throw std::runtime_error("Original completed cycle evidence changed");
        struct stat plan;const auto planPath=fDirectory.Path()+"/successor-plan.json";
        if (::lstat(planPath.c_str(),&plan) || !Same(plan,fSuccessorIdentity) || !S_ISREG(plan.st_mode) ||
            plan.st_uid!=::geteuid() || plan.st_nlink!=1 || (plan.st_mode&07777)!=0400 ||
            ReadFile(planPath,1024*1024)!=fSuccessorBytes)
            throw std::runtime_error("Original retirement successor plan copy changed");
    }
    struct stat request;
    const auto requestPath=fDirectory.Path()+"/request.json";
    if (::lstat(requestPath.c_str(),&request) || !Same(request,fRequestIdentity) || !S_ISREG(request.st_mode) ||
        request.st_nlink!=1 || request.st_uid!=::geteuid() || (request.st_mode&0777)!=0600 ||
        ReadFile(requestPath,8192)!=fRequestBytes)
        throw std::runtime_error("Original controller handoff request changed");
    struct stat current;
    if (::lstat(fPath.c_str(),&current) || !Same(current,fIdentity) || !S_ISREG(current.st_mode) ||
        current.st_nlink!=1 || current.st_uid!=::geteuid() ||
        (current.st_mode&0777)!=(fRetirement ? 0400U : 0600U) ||
        ReadFile(fPath,4*1024*1024)!=fBytes)
        throw std::runtime_error("Original controller handoff proof changed");
}

}
