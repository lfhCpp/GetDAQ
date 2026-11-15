#ifndef GETDAQ_SCANEXECUTIONJOURNALSTORE_WINDOWS_IO_P_H
#define GETDAQ_SCANEXECUTIONJOURNALSTORE_WINDOWS_IO_P_H

#include "scanexecutionjournalstore_windows_support_p.h"

#include <cstddef>

namespace GetDAQ {
namespace ScanExecutionJournalWindows {

std::string ReadJournalFile(const std::wstring &directoryPath,
                            const std::wstring &leaf,
                            const std::vector<unsigned char> &userSid,
                            Identity *identity,
                            std::size_t maximumBytes);
void PublishNewJournalFile(const std::wstring &directoryPath,
                           const std::wstring &leaf,
                           const std::vector<unsigned char> &userSid,
                           const std::string &bytes);
void ReplaceJournalFile(const std::wstring &directoryPath,
                        const std::wstring &leaf,
                        const std::vector<unsigned char> &userSid,
                        const Identity &expectedIdentity,
                        const std::string &bytes);

} // namespace ScanExecutionJournalWindows
} // namespace GetDAQ

#endif
