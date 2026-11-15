#ifndef GETDAQ_RECEIVERCATALOGINTERNAL_H
#define GETDAQ_RECEIVERCATALOGINTERNAL_H

#include "controlprotocol.h"
#include "supervisorio.h"
#include <cerrno>
#include <cstdlib>
#include <iomanip>
#include <limits>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <stdexcept>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace GetDAQ {
struct ReceiverCatalogOptions;
namespace ReceiverCatalogInternal {

void CheckOptions(const ReceiverCatalogOptions &options);
std::string InspectCatalogAt(const std::string &path,const std::string &logicalPath);
std::set<std::string> RawNames(const SupervisorIO::Directory &directory);
bool ReceiverBoundary(const JsonValue &seal,const ReceiverSourcePlan &plan,const ManagedReceiverStatus &expected);
void CheckSealFields(const JsonValue &seal,const ReceiverSourcePlan &plan);
std::string CatalogPublicationTarget(const ReceiverCatalogOptions &options);
std::string CatalogReplacementBackupPath(const std::string &target,const std::string &operationId);
std::string CatalogReplacementStagePath(const std::string &target,const std::string &operationId);

struct SourceBinding {
    const ManagedReceiverStatus *status=nullptr;
    std::unique_ptr<SupervisorIO::Directory> directory;
    std::unique_ptr<SupervisorIO::Fd> lease;
    struct stat leaseIdentity,sealIdentity,intentIdentity;
    std::string seal,intent,sealName;
    std::map<std::string,struct stat> files;
};

void CheckSourceBinding(SourceBinding &source);
void ValidateSourceBindings(const ReceiverCatalogOptions &options,dev_t catalogDevice,
                            std::vector<std::unique_ptr<SourceBinding> > &sources);

inline void Require(bool value,const char *message) {
    if (!value) throw std::runtime_error(message);
}

inline std::int64_t SignedIntegerText(const std::string &text) {
    Require(!text.empty() && text.size()<=20,"Expected bounded signed integer");
    errno=0;
    char *end=nullptr;
    const long long parsed=std::strtoll(text.c_str(),&end,10);
    Require(!errno && end==text.c_str()+text.size() && std::to_string(parsed)==text,
        "Expected canonical signed integer");
    return static_cast<std::int64_t>(parsed);
}

inline std::int64_t ManifestInteger(const JsonValue &value) {
    Require(value.type==JsonValue::Number,"Scan range fields must be JSON integers");
    return SignedIntegerText(value.text);
}

inline std::string Decimal(double value) {
    std::ostringstream out;
    out<<std::setprecision(std::numeric_limits<double>::max_digits10)<<value;
    return out.str();
}

inline JsonValue CatalogJson(const std::string &bytes) {
    JsonLimits limits;
    limits.arrayItems=65536;
    return ParseStrictJson(bytes,limits);
}

inline bool MetadataSame(const struct stat &a,const struct stat &b,bool linked=false) {
    if (!SupervisorIO::Same(a,b) || a.st_mode!=b.st_mode || a.st_uid!=b.st_uid ||
        a.st_size!=b.st_size || a.st_mtime!=b.st_mtime) return false;
#ifdef __APPLE__
    if (a.st_mtimespec.tv_nsec!=b.st_mtimespec.tv_nsec) return false;
    return linked || (a.st_nlink==b.st_nlink && a.st_ctimespec.tv_sec==b.st_ctimespec.tv_sec &&
        a.st_ctimespec.tv_nsec==b.st_ctimespec.tv_nsec);
#else
    if (a.st_mtim.tv_nsec!=b.st_mtim.tv_nsec) return false;
    return linked || (a.st_nlink==b.st_nlink && a.st_ctim.tv_sec==b.st_ctim.tv_sec &&
        a.st_ctim.tv_nsec==b.st_ctim.tv_nsec);
#endif
}

} // namespace ReceiverCatalogInternal
} // namespace GetDAQ

#endif
