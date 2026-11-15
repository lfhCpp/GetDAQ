#ifndef GETDAQ_SCANEXECUTIONJOURNALSTORE_PUBLISH_P_H
#define GETDAQ_SCANEXECUTIONJOURNALSTORE_PUBLISH_P_H

#include <string>
#include <sys/stat.h>

namespace GetDAQ {
namespace ScanExecutionJournalStoreInternal {

void PublishNew(int directory, const std::string &leaf, const std::string &bytes);
void PublishReplace(int directory, const std::string &leaf,
                    const struct stat &expectedIdentity,
                    const std::string &bytes);

} // namespace ScanExecutionJournalStoreInternal
} // namespace GetDAQ

#endif
