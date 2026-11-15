#ifndef GETDAQ_RECEIVERCATALOGREPLACEMENT_P_H
#define GETDAQ_RECEIVERCATALOGREPLACEMENT_P_H

#include "receivercataloginternal.h"
#include <unistd.h>

namespace GetDAQ {
namespace ReceiverCatalogReplacementInternal {
std::string ParentPath(const std::string &path);
std::string LeafPath(const std::string &path);
bool PathPresent(const std::string &path);
std::string ReplacementTransactionPath(const std::string &target, const std::string &operationId);
std::string ReplacementIntent(const std::string &target, const std::string &backup, const std::string &candidate,
                              const std::string &operationId, const std::string &requestKey,
                              const std::string &oldReceipt, const std::string &newReceipt,
                              const std::string &candidateReceipt);
void EnsureMarker(const SupervisorIO::Directory &transaction, const std::string &name, const std::string &body);
bool MarkerPresent(const SupervisorIO::Directory &transaction, const std::string &name, const std::string &body);
bool CatalogMatches(const std::string &path, const std::string &logicalPath, const std::string &receiptSha256);
int RenameNoReplace(int fromDirectory, const char *from, int toDirectory, const char *to);
void SyncDirectory(const SupervisorIO::Directory &directory);
}
}

#endif
