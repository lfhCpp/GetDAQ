#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalog_scansource_p.h"
#include "supervisorio.h"

#include <algorithm>
#include <fcntl.h>
#include <memory>
#include <set>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {
using namespace ReceiverCatalogInternal;
using namespace SupervisorIO;

std::uint64_t ForEachBoundScanCatalogRawFile(
    const std::string &catalogPath, const std::string &targetCobo,
    const std::function<void(int)> &consume) {
    const auto receiptBefore=InspectReceiverCatalog(catalogPath);
    const auto requestBytes=ReadFile(catalogPath+"/request.json",1024*1024);
    const auto request=DecodeReceiverCatalogRequest(requestBytes);
    const auto sourcePlan=std::find_if(request.receiver.sources.begin(),request.receiver.sources.end(),
        [&](const ReceiverSourcePlan &source) { return source.id==targetCobo; });
    Require(sourcePlan!=request.receiver.sources.end(),"Scan target CoBo is not a source in the bound receiver plan");

    const auto manifestBytes=ReadFile(catalogPath+"/manifest.json",16*1024*1024);
    const auto manifest=CatalogJson(manifestBytes);
    const auto &sources=manifest.At("sources");
    Require(sources.type==JsonValue::Array,"Catalog source list is invalid");
    const JsonValue *sourceRow=nullptr;
    for (const auto &source:sources.items) {
        if (String(source.At("id"))!=targetCobo) continue;
        Require(!sourceRow,"Duplicate target source in catalog");
        sourceRow=&source;
    }
    Require(sourceRow,"Catalog does not contain the scan target CoBo");
    const auto storedDirectory=String(sourceRow->At("stored_directory"));
    Require(storedDirectory=="source_"+std::to_string(sourceRow-&sources.items[0]),
        "Catalog source directory mapping is invalid");

    Directory sourceDirectory(catalogPath+'/'+storedDirectory);
    std::uint64_t filesRead=0;
    for (const auto &file:sourceRow->At("files").items) {
        const auto name=String(file.At("name"));
        struct stat beforePath,beforeFd,afterFd,afterPath;
        Require(!::fstatat(sourceDirectory.Descriptor(),name.c_str(),&beforePath,AT_SYMLINK_NOFOLLOW) &&
            S_ISREG(beforePath.st_mode) && std::uint64_t(beforePath.st_size)==Unsigned(file.At("bytes"),true) &&
            std::uint64_t(beforePath.st_dev)==Unsigned(file.At("device"),true) &&
            std::uint64_t(beforePath.st_ino)==Unsigned(file.At("inode"),true),
            "Catalog raw file identity changed before analysis");
        Fd input(::openat(sourceDirectory.Descriptor(),name.c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC));
        Require(input.value>=0 && !::fstat(input.value,&beforeFd) && MetadataSame(beforePath,beforeFd),
            "Cannot securely open bound catalog raw file");
        consume(input.value);
        Require(!::fstat(input.value,&afterFd) &&
            !::fstatat(sourceDirectory.Descriptor(),name.c_str(),&afterPath,AT_SYMLINK_NOFOLLOW) &&
            MetadataSame(beforeFd,afterFd) && MetadataSame(beforePath,afterPath),
            "Catalog raw file changed during analysis");
        ++filesRead;
    }
    sourceDirectory.Check();
    Require(ReadFile(catalogPath+"/request.json",1024*1024)==requestBytes &&
        ReadFile(catalogPath+"/manifest.json",16*1024*1024)==manifestBytes &&
        InspectReceiverCatalog(catalogPath)==receiptBefore,
        "Bound catalog changed during scan analysis");
    return filesRead;
}

}
}
