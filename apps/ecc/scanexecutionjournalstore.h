#ifndef GETDAQ_SCANEXECUTIONJOURNALSTORE_H
#define GETDAQ_SCANEXECUTIONJOURNALSTORE_H

#include "../../common/experiment/scanexecutionjournal.h"

#include <memory>
#include <string>

namespace GetDAQ {

// Platform-native single-writer store for control metadata only. POSIX builds
// require a current-user-owned 0700 directory and retain files at 0600. Windows
// builds require a local-drive directory owned by the current user with an ACL
// that grants access only to that user. This store never contains DAQ payloads.
class ScanExecutionJournalStore {
public:
    static std::unique_ptr<ScanExecutionJournalStore> Create(
        const std::string &path, const ScanExecutionJournal &initial);
    static std::unique_ptr<ScanExecutionJournalStore> Open(const std::string &path);
    ~ScanExecutionJournalStore();

    ScanExecutionJournal Current() const;
    void Persist(const ScanExecutionJournal &next);
    const std::string &Path() const;

    ScanExecutionJournalStore(const ScanExecutionJournalStore &) = delete;
    ScanExecutionJournalStore &operator=(const ScanExecutionJournalStore &) = delete;

private:
    class Impl;
    explicit ScanExecutionJournalStore(std::unique_ptr<Impl> impl);
    std::unique_ptr<Impl> fImpl;
};

}

#endif
