#include "transferjobs_p.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include "../../common/text/utf8.h"

#include <cerrno>
#include <fcntl.h>
#include <spawn.h>
#include <stdexcept>
#include <sys/stat.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

extern char **environ;

namespace GetDAQ {
using namespace SupervisorIO;
namespace {
void Require(bool value, const char *message)
{
    if (!value) throw std::runtime_error(message);
}
} // namespace

void TransferJobs::Impl::Work(TransferOperation operation,const std::string &directoryName,
                              bool verification,const std::string &verificationIntent,
                              bool resume) {
        pid_t child=0;bool reaped=false;
        try {
            Directory directory(directoryName),job(root.Path()+"/job-"+operation.request.operationId);Check();
            if(verification)CheckVerificationIntent(directory,verificationIntent);
            operation.phase=verification && !resume ? "verifying" : "running";Save(job,operation);{std::lock_guard<std::mutex> lock(mutex);jobs[operation.request.operationId]=operation;}
            Fd stdoutFile(::openat(directory.Descriptor(),"stdout.json",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600)),stderrFile(::openat(directory.Descriptor(),"stderr.log",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600));
            Require(stdoutFile.value>=0 && stderrFile.value>=0,"Cannot bind copy child metadata outputs");
            std::vector<std::string> words=verification && !resume ? std::vector<std::string>{options.executable,"--verify-transfer",operation.destination} : std::vector<std::string>{options.executable,resume ? "--resume-catalog" : "--copy-catalog",operation.source,operation.destination,"--progress-file",directory.Path()+"/progress.json","--expected-source-key",operation.request.sourceRequestKey};std::vector<char *> args;for(auto &word:words)args.push_back(&word[0]);args.push_back(nullptr);
            posix_spawn_file_actions_t actions;Require(!::posix_spawn_file_actions_init(&actions),"Cannot initialize copy spawn actions");
            const int configured=::posix_spawn_file_actions_adddup2(&actions,stdoutFile.value,STDOUT_FILENO) || ::posix_spawn_file_actions_adddup2(&actions,stderrFile.value,STDERR_FILENO);
            const int spawned=configured ? configured : ::posix_spawn(&child,options.executable.c_str(),&actions,nullptr,args.data(),environ);::posix_spawn_file_actions_destroy(&actions);Require(!spawned && child>0,"Native copy child not spawned; intent retained");
            WriteNew(directory.Path()+"/child.json","{\"pid\":"+std::to_string(child)+",\"parent_instance\":"+EncodeJsonString(identity.instanceId)+",\"payload_in_child\":true}\n");
            int status=0;for(;;) {
                const auto result=::waitpid(child,&status,WNOHANG);if(result<0 && errno==EINTR)continue;Require(result>=0,"Cannot observe owned copy child");if(result==child){reaped=true;break;}
                struct stat info;if(!::fstatat(directory.Descriptor(),"progress.json",&info,AT_SYMLINK_NOFOLLOW)) {
                    const auto value=ParseStrictJson(ReadFile(directory.Path()+"/progress.json",4096));ExactSupervisorFields(value,{"schema","phase","copied_bytes"});Require(Unsigned(value.At("schema"))==1,"Copy progress schema differs");
                    operation.copiedBytes=Unsigned(value.At("copied_bytes"),true);const auto phase=String(value.At("phase"));operation.phase=phase=="before-complete" ? "verifying" : "running";
                    std::lock_guard<std::mutex> lock(mutex);jobs[operation.request.operationId]=operation;
                } else Require(errno==ENOENT,"Cannot inspect child progress");
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            Check();Require(WIFEXITED(status) && WEXITSTATUS(status)==0,"Native transfer child failed; no automatic restart or source deletion");
            if(verification)CheckVerificationIntent(directory,verificationIntent);
            operation.receipt=ReadFile(directory.Path()+"/stdout.json",65536);
            Require(operation.receipt==ReadFile(operation.destination+"/transfer-complete.json",65536),"Native child receipt differs from destination completion");
            const auto complete=ParseStrictJson(operation.receipt);operation.copiedBytes=Unsigned(complete.At("copied_bytes"),true);operation.phase="complete";operation.message=resume ? "Explicit native continuation and full verification completed the original copy; old instance/Run/ECC unchanged" : (verification ? "Explicit native full verification reconciled the original copy; old instance/Run/ECC unchanged" : "Native child completed copy and full content verification; original Run/ECC unchanged");EncodeTransferOperation(operation);Save(job,operation);
        } catch(const std::exception &e) {
            // Loss of supervision must not kill a bulk child or claim completion.
            if(child>0 && !reaped){int status;while(::waitpid(child,&status,0)<0 && errno==EINTR){};}
            operation.phase=verification ? "unknown" : "failed";operation.receipt.clear();operation.message=EccErrorText(e.what());
            try {Directory job(root.Path()+"/job-"+operation.request.operationId);Save(job,operation);}catch(...){fault=true;operation.phase="unknown";operation.message="Transfer result persistence failed; inspect original intent and native output";}
        }
        std::lock_guard<std::mutex> lock(mutex);jobs[operation.request.operationId]=operation;active=false;
        reopenedPending=false;for(const auto &entry:jobs)if(entry.second.phase=="unknown")reopenedPending=true;
    }

} // namespace GetDAQ
