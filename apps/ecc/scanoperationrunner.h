#ifndef GETDAQ_SCANOPERATIONRUNNER_H
#define GETDAQ_SCANOPERATIONRUNNER_H

#include "scanexecutionjournalstore.h"

#include <functional>
#include <string>

namespace GetDAQ {

struct ScanOperationResult {
    ScanExecutionJournal journal;
    bool outcomeUnknown;
    std::string errorMessage;
};

// Executes one external operation with durable intent-before-action ordering.
// The callback is invoked at most once. Any callback exception is ambiguous
// and is quarantined; callers must observe/reconcile instead of retrying.
class ScanOperationRunner {
public:
    explicit ScanOperationRunner(ScanExecutionJournalStore &store);

    // Async adapters use these phases around a single non-blocking request:
    // persist intent before submission, then record exactly one observed result.
    void PersistIntent(const std::string &operationId,
                       const std::string &operation,
                       const std::string &requestBytes);
    ScanOperationResult Complete(const std::string &operationId,
                                 const std::string &responseBytes);
    ScanOperationResult CompleteObserved(const std::string &operationId,
                                        const std::string &responseBytes);
    ScanOperationResult MarkUnknown(const std::string &operationId,
                                    const std::string &reason);

    ScanOperationResult Execute(const std::string &operationId,
                                const std::string &operation,
                                const std::string &requestBytes,
                                const std::function<std::string()> &action);

private:
    ScanExecutionJournalStore &fStore;
};

}

#endif
