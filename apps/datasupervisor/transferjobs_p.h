#ifndef GETDAQ_TRANSFERJOBS_P_H
#define GETDAQ_TRANSFERJOBS_P_H

#include "transferjobs.h"
#include "supervisorio.h"

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <sys/stat.h>
#include <thread>

namespace GetDAQ {
using SupervisorIO::Directory;
using SupervisorIO::Fd;

class TransferJobs::Impl {
public:
    TransferJobOptions options;
    Directory source, backup, root;
    Fd lease;
    std::string policy;
    TransferIdentity identity;
    struct stat policyInfo, executableInfo, leaseInfo;
    mutable std::mutex mutex;
    std::mutex submitMutex;
    std::map<std::string, TransferOperation> jobs;
    bool active;
    std::atomic<bool> reopenedPending;
    std::atomic<bool> fault;
    std::thread worker;

    explicit Impl(const TransferJobOptions &options);
    ~Impl();
    void Check();
    std::string Source(const std::string &experiment, unsigned int run) const;
    void CheckIntent(const Directory &directory, const TransferOperation &operation) const;
    std::string Receipt(const std::string &experiment, unsigned int run) const;
    void Save(const Directory &directory, const TransferOperation &operation);
    void CheckVerificationIntent(const Directory &directory, const std::string &intent);
    void Work(TransferOperation operation, const std::string &directoryName,
              bool verification = false, const std::string &verificationIntent = "",
              bool resume = false);
};

} // namespace GetDAQ

#endif
