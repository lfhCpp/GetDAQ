#ifndef GETDAQ_SCANEXECUTIONJOURNALSTORE_INTERNAL_P_H
#define GETDAQ_SCANEXECUTIONJOURNALSTORE_INTERNAL_P_H

#include <cstddef>
#include <string>
#include <sys/stat.h>

namespace GetDAQ {
namespace ScanExecutionJournalStoreInternal {

std::string ReadCurrent(int directory,const std::string &leaf,
                        struct stat *identity,std::size_t maximumBytes);

} // namespace ScanExecutionJournalStoreInternal
} // namespace GetDAQ

#endif
