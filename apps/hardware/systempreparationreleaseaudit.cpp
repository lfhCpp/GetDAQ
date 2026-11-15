#include "systempreparationreleaseaudit_p.h"
#include <stdexcept>
#include "hardwareclient.h"

namespace GetDAQ {

SystemPreparationReleaseAudit::SystemPreparationReleaseAudit(
    const std::string &path,const char *action,const SystemPreparationOptions &options)
    : fAudit(path+"/system-release.jsonl"),fDirectory(path)
{
    Record("{\"schema_version\":1,\"phase\":\"intent\",\"operation\":"+JsonString(action)+
        ",\"configuration_sha256\":"+JsonString(options.configurationSha256)+
        ",\"artifact_sha256\":"+JsonString(options.artifactSha256)+
        ",\"control_lock_verified\":false,\"actual_loaded_configuration_verified\":false,\"receiver_sealed\":false,\"production_ecc_state_committed\":false}");
}

void SystemPreparationReleaseAudit::Record(const std::string &row)
{
    if (fAudit.Bytes()+row.size()+1>16*1024*1024)
        throw std::runtime_error("System release audit budget exceeded");
    fAudit.Append(row);
}

std::string SystemPreparationReleaseAudit::Begin(const std::string &id,const char *stage)
{
    const auto file="step-"+std::to_string(fSequence++)+".jsonl";
    Record("{\"phase\":\"step_begin\",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+
        ",\"audit_file\":"+JsonString(file)+"}");
    return fDirectory+'/'+file;
}

void SystemPreparationReleaseAudit::End(const std::string &id,const char *stage,bool complete)
{
    Record("{\"phase\":"+JsonString(complete ? "step_complete" : "step_failed")+
        ",\"id\":"+JsonString(id)+",\"stage\":"+JsonString(stage)+"}");
}

}
