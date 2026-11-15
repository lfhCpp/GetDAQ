#include "scanoperationrunner.h"

#include "../../common/experiment/configurationsha256.h"

#include <stdexcept>

namespace GetDAQ {

ScanOperationRunner::ScanOperationRunner(ScanExecutionJournalStore &store)
    : fStore(store)
{
}

void ScanOperationRunner::PersistIntent(const std::string &operationId,
                                        const std::string &operation,
                                        const std::string &requestBytes)
{
    const ScanExecutionJournal current = fStore.Current();
    const ScanExecutionJournal intent = current.WithOperationIntent(
        operationId, operation, ConfigurationSha256(requestBytes));
    fStore.Persist(intent);
}

ScanOperationResult ScanOperationRunner::Complete(const std::string &operationId,
                                                  const std::string &responseBytes)
{
    const ScanExecutionJournal current = fStore.Current();
    const ScanExecutionJournal completed = current.WithOperationComplete(
        operationId, ConfigurationSha256(responseBytes), current.PendingOperationTarget());
    fStore.Persist(completed);
    return {fStore.Current(), false, std::string()};
}

ScanOperationResult ScanOperationRunner::CompleteObserved(
    const std::string &operationId, const std::string &responseBytes)
{
    const ScanExecutionJournal current = fStore.Current();
    const ScanExecutionJournal completed = current.WithObservedOperationComplete(
        operationId, ConfigurationSha256(responseBytes));
    fStore.Persist(completed);
    return {fStore.Current(), false, std::string()};
}

ScanOperationResult ScanOperationRunner::MarkUnknown(const std::string &operationId,
                                                     const std::string &reason)
{
    const ScanExecutionJournal current = fStore.Current();
    const ScanExecutionJournal unknown = current.WithOperationUnknown(
        operationId, ConfigurationSha256(reason));
    fStore.Persist(unknown);
    return {fStore.Current(), true, reason};
}

ScanOperationResult ScanOperationRunner::Execute(
    const std::string &operationId, const std::string &operation,
    const std::string &requestBytes,
    const std::function<std::string()> &action)
{
    if (!action) throw std::invalid_argument("Scan operation callback is empty");
    PersistIntent(operationId, operation, requestBytes);

    std::string responseBytes;
    try {
        responseBytes = action();
    } catch (const std::exception &error) {
        return MarkUnknown(operationId, error.what());
    } catch (...) {
        const std::string message = "Scan operation callback threw a non-standard exception";
        return MarkUnknown(operationId, message);
    }
    return Complete(operationId, responseBytes);
}

}
