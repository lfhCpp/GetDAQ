#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalogreplacement_p.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <limits>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
#if defined(__linux__)
#include <linux/fs.h>
#include <sys/syscall.h>
#elif defined(__APPLE__)
#include <stdio.h>
#endif

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
using namespace ReceiverCatalogReplacementInternal;
namespace ReceiverCatalogReplacementInternal {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
std::string ParentPath(const std::string &path) {
    const auto slash=path.find_last_of('/'); Require(slash!=std::string::npos && slash+1<path.size(),"Expected absolute leaf path");
    return slash ? path.substr(0,slash) : std::string("/");
}
std::string LeafPath(const std::string &path) {
    const auto slash=path.find_last_of('/'); Require(slash!=std::string::npos && slash+1<path.size(),"Expected absolute leaf path");
    return path.substr(slash+1);
}
bool PathPresent(const std::string &path) {
    struct stat info;
    if (!::lstat(path.c_str(),&info)) return true;
    Require(errno==ENOENT,"Cannot inspect replacement transaction path"); return false;
}
std::string ReplacementTransactionPath(const std::string &target,const std::string &operationId) {
    const auto hash=ConfigurationSha256("getdaq.catalog-replacement-transaction:1\n"+operationId).substr(0,24);
    return ParentPath(target)+"/."+LeafPath(target)+".replacement-"+hash+".txn";
}
std::string ReplacementIntent(const std::string &target,const std::string &backup,const std::string &candidate,
                              const std::string &operationId,const std::string &requestKey,
                              const std::string &oldReceipt,const std::string &newReceipt,
                              const std::string &candidateReceipt) {
    return "{\"schema\":1,\"kind\":\"getdaq.catalog-replacement-intent\",\"operation_id\":"+EncodeJsonString(operationId)+
        ",\"request_key\":"+EncodeJsonString(requestKey)+",\"target\":"+EncodeJsonString(target)+
        ",\"backup\":"+EncodeJsonString(backup)+",\"candidate\":"+EncodeJsonString(candidate)+
        ",\"old_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(oldReceipt))+
        ",\"new_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(newReceipt))+
        ",\"candidate_receipt_sha256\":"+EncodeJsonString(ConfigurationSha256(candidateReceipt))+"}\n";
}
void EnsureMarker(const Directory &transaction,const std::string &name,const std::string &body) {
    struct stat info;
    if (!::fstatat(transaction.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW)) {
        Require(S_ISREG(info.st_mode) && info.st_uid==::geteuid() && (info.st_mode&0777)==0600 && info.st_nlink==1 &&
            ReadFile(transaction.Path()+"/"+name,4096)==body,"Replacement journal marker differs; preserve transaction state");
        return;
    }
    Require(errno==ENOENT,"Cannot inspect replacement journal marker");
    WriteNew(transaction.Path()+"/"+name,body);
}
bool MarkerPresent(const Directory &transaction,const std::string &name,const std::string &body) {
    struct stat info;
    if (!::fstatat(transaction.Descriptor(),name.c_str(),&info,AT_SYMLINK_NOFOLLOW)) {
        Require(S_ISREG(info.st_mode) && info.st_uid==::geteuid() && (info.st_mode&0777)==0600 && info.st_nlink==1 &&
            ReadFile(transaction.Path()+"/"+name,4096)==body,"Replacement journal marker differs; preserve transaction state");
        return true;
    }
    Require(errno==ENOENT,"Cannot inspect replacement journal marker"); return false;
}
bool CatalogMatches(const std::string &path,const std::string &logicalPath,const std::string &receiptSha256) {
    if (!PathPresent(path)) return false;
    try { return ConfigurationSha256(InspectCatalogAt(path,logicalPath))==receiptSha256; }
    catch (...) { return false; }
}
void SyncDirectory(const Directory &directory) {
    directory.Check();
    Require(::fsync(directory.Descriptor()) == 0, "Cannot sync catalog directory");
    directory.Check();
}
int RenameNoReplace(int fromDirectory,const char *from,int toDirectory,const char *to) {
#if defined(__linux__)
    return static_cast<int>(::syscall(SYS_renameat2,fromDirectory,from,toDirectory,to,RENAME_NOREPLACE));
#elif defined(__APPLE__)
    return ::renameatx_np(fromDirectory,from,toDirectory,to,RENAME_EXCL);
#else
    (void)fromDirectory; (void)from; (void)toDirectory; (void)to; errno=ENOTSUP; return -1;
#endif
}
} // namespace ReceiverCatalogReplacementInternal
using namespace ReceiverCatalogReplacementInternal;

} // namespace GetDAQ
