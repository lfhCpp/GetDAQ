#ifndef GETDAQ_TRANSFERJOBS_H
#define GETDAQ_TRANSFERJOBS_H
#include "transferprotocol.h"
#include <memory>
namespace GetDAQ {
struct TransferJobOptions {std::string agentId,deploymentId,sourceRoot,backupRoot,auditRoot,executable,executableSha256;};
void ValidateTransferJobOptions(const TransferJobOptions &options);
// Metadata-only controller; payload lives in a separate native catalog process.
class TransferJobs {
public:
    explicit TransferJobs(const TransferJobOptions &options);
    ~TransferJobs(); // Waits for its child; does not terminate or restart bulk copy.
    TransferJobs(const TransferJobs &)=delete;
    TransferJobs &operator=(const TransferJobs &)=delete;
    TransferIdentity Identity() const;
    TransferOperation Submit(const TransferRequest &request);
    TransferOperation Reconcile(const TransferReconciliationRequest &request);
    TransferOperation Resume(const TransferReconciliationRequest &request);
    TransferOperation Operation(const std::string &id) const;
    std::string SourceReceipt(const std::string &experiment,unsigned int run) const;
private:
    TransferOperation Recover(const TransferReconciliationRequest &request,bool resume);
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
