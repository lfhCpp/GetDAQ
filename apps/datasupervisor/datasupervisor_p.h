#ifndef GETDAQ_DATASUPERVISOR_P_H
#define GETDAQ_DATASUPERVISOR_P_H

#include "datasupervisor.h"
#include "controlprotocol.h"
#include "receiverprocessposix_p.h"
#include "supervisorio.h"

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace GetDAQ {
namespace DataSupervisorInternal {
using namespace SupervisorIO;
using namespace ReceiverProcessInternal;
void CheckOutputReserve(const ReceiverSourcePlan &source);
void Append(int fd,const std::string &bytes);
class Audit {
    Fd fd; Directory directory;
    struct stat identity;
    std::string path,content;
public:
    explicit Audit(const std::string &path);
    void Check() const;
    void Row(const std::string &row);
};
}
using namespace DataSupervisorInternal;
class DataSupervisor::Impl {
public:
    struct Child {
        ManagedReceiverStatus status;
        std::string log;
        std::unique_ptr<Directory> directory;
        struct stat lockIdentity;
        bool lockBound=false;
        Fd cycleControl;
    };
    DataSupervisorOptions options;
    std::string executableHash,manifest,planHash,session;
    struct stat iceExecutableIdentity,executableIdentity,manifestIdentity,journalIdentity,leaseIdentity;
    std::unique_ptr<Directory> root;
    std::map<std::string,std::unique_ptr<Directory>> outputRoots;
    Fd lease,journal;
    std::uint64_t journalBytes=0,sequence=0;
    std::string journalContent;
    bool startupFailed=false,abortAttempted=false,failedAbortAttempted=false;
    bool failedAbortComplete=false,retirementAttempted=false;
    bool catalogFailureRecoverable=false,catalogFailureResumeAttempted=false;
    std::string catalogFailureReplacementId,catalogFailureExperiment,catalogFailureSession;
    std::string catalogFailurePhase;
    unsigned int catalogFailureRun=0;
    std::string failedAbortParent;
    std::unique_ptr<Audit> failedAbortAudit,retirementAudit;
    mutable bool storageFault=false;
    std::vector<std::unique_ptr<Child>> children;
    mutable std::mutex operation,statusMutex;
    mutable DataSupervisorStatus snapshot;
    explicit Impl(const DataSupervisorOptions &o);
    ~Impl();
    void Publish(DataSupervisorStatus::State state,const std::string &message="");
    void CheckStore() const;
    void Record(const std::string &event,const std::string &fields="");
    void Eligible() const;
    bool CanResumeCatalog(const std::string &replacementId,const std::string &experiment,unsigned int run,const std::string &expectedSession,const std::string &phase) const;
    void CheckRetainedStoppedChildren() const;
    void CheckRetirement() const;
    void Identity(const JsonValue &s,const Child &c,const ReceiverSourcePlan &source,bool seal=false) const;
    void LockHeld(Child &c,bool bind) const;
    JsonValue ReadyStatus(std::size_t i,bool bind=false) const;
    const std::string &Executable(std::size_t i) const;
    std::vector<std::string> Arguments(std::size_t i,const Child &c) const;
    void Seal(std::size_t i,bool unused=false);
};
}
#endif
