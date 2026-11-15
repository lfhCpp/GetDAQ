#include "recoverytaskstore_p.h"
#include "recoveryfiles_p.h"
#include "planio.h"
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ { namespace RecoveryPrivate {
using namespace SupervisorIO;

Jobs RecoveryTaskStore::Load(const Directory &root, const std::string &policySha256) {
    Fd scan(::openat(root.Descriptor(),".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC));
    DIR *entries=::fdopendir(scan.value);
    RequireRecovery(entries!=nullptr,"Cannot enumerate recovery tasks");
    scan.value=-1;
    Jobs jobs;
    try {
        for (;;) {
            errno=0;
            auto entry=::readdir(entries);
            if (!entry) { RequireRecovery(!errno,"Cannot read recovery task directory"); break; }
            const std::string name=entry->d_name;
            if (name.substr(0,4)!="job-") continue;
            RequireRecovery(jobs.size()<256,"Recovery task history exceeds 256 entries");
            Directory dir(root.Path()+'/'+name);
            PinnedFile request(dir.Path()+"/request.json",8192);
            Job job;
            job.operation.request=DecodeColdHandoffRequest(request.bytes);
            RequireRecovery(name=="job-"+job.operation.request.taskId && job.operation.request.policySha256==policySha256,
                            "Reopened recovery task binding differs");
            job.operation.requestKey=ColdHandoffRequestKey(job.operation.request);
            if (RecoveryPathExists(dir.Path()+"/operation.json")) {
                PinnedFile state(dir.Path()+"/operation.json",8*1024*1024);
                job.stateBytes=state.bytes;
                job.operation=DecodeColdHandoffOperation(state.bytes);
                RequireRecovery(EncodeColdHandoffRequest(job.operation.request)==request.bytes,"Reopened recovery intent differs");
            } else {
                job.operation.phase="accepted";
            }
            if (job.operation.phase=="accepted" || job.operation.phase=="running") {
                job.operation.phase="unknown";
                job.operation.message="Service reopened an unfinished task; no replay, PID adoption or kill";
                job.operation.receipt.clear();
                job.operation.receiptSha256.clear();
            }
            RequireRecovery(jobs.emplace(job.operation.request.taskId,job).second,"Duplicate recovery task");
        }
    } catch (...) {
        ::closedir(entries);
        throw;
    }
    ::closedir(entries);
    return jobs;
}

void RecoveryTaskStore::Check(const Directory &root, const Jobs &jobs) {
    for (const auto &entry:jobs) {
        Directory dir(root.Path()+"/job-"+entry.first);
        PinnedFile request(dir.Path()+"/request.json",8192);
        RequireRecovery(request.bytes==EncodeColdHandoffRequest(entry.second.operation.request),"Recovery saved request changed");
        if (entry.second.stateBytes.empty()) {
            RequireRecovery(!RecoveryPathExists(dir.Path()+"/operation.json"),"Unexpected recovery state appeared");
        } else {
            PinnedFile state(dir.Path()+"/operation.json",8*1024*1024);
            RequireRecovery(state.bytes==entry.second.stateBytes,"Recovery saved state changed");
        }
    }
}

void RecoveryTaskStore::CreateRequest(const Directory &root, const ColdHandoffRequest &request) {
    const auto path=root.Path()+"/job-"+request.taskId;
    CreateDirectory(path);
    WriteNew(path+"/request.json",EncodeColdHandoffRequest(request));
}

void RecoveryTaskStore::Save(const Directory &root, Job &job) {
    Directory dir(root.Path()+"/job-"+job.operation.request.taskId);
    const auto bytes=EncodeColdHandoffOperation(job.operation);
    dir.Check();
    WriteNew(dir.Path()+"/operation.pending",bytes);
    RequireRecovery(!::rename((dir.Path()+"/operation.pending").c_str(),(dir.Path()+"/operation.json").c_str()) &&
        !::fsync(dir.Descriptor()),"Cannot persist recovery operation");
    dir.Check();
    job.stateBytes=bytes;
}

} }
