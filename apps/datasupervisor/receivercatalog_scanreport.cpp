#include "receivercatalog_scananalysis_p.h"

#include "receivercataloginternal.h"
#include "supervisorio.h"

#include <cerrno>
#include <fcntl.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
namespace ReceiverCatalogScanAnalysisInternal {
using namespace ReceiverCatalogInternal;
using namespace SupervisorIO;

void ValidateScanAnalysisPaths(const std::string &scanPath,
                              const std::string &bindingPath,
                              const std::string &outputPath) {
    Path(scanPath);
    Path(bindingPath);
    Path(outputPath);
    Require(scanPath!=bindingPath &&
        scanPath.compare(0,bindingPath.size()+1,bindingPath+'/')!=0 &&
        bindingPath.compare(0,scanPath.size()+1,scanPath+'/')!=0,
        "Scan plan and binding directory must be separate");
    Require(outputPath.compare(0,scanPath.size()+1,scanPath+'/')!=0 &&
        outputPath.compare(0,bindingPath.size()+1,bindingPath+'/')!=0,
        "Analysis output must be outside the immutable plan and binding directory");
}

void PublishScanAnalysisReport(const std::string &outputPath,
                               const std::string &report,
                               const std::string &analysisName,
                               const std::string &publishedObjectName) {
    const auto slash=outputPath.find_last_of('/');
    Require(slash>0 && slash+1<outputPath.size(),
        "Analysis output must be inside an existing directory");
    struct stat existing;
    Require(::lstat(outputPath.c_str(),&existing) && errno==ENOENT,
        "Analysis output already exists; preserve it");

    const auto pending=outputPath+".pending";
    WriteNew(pending,report);
    if (::link(pending.c_str(),outputPath.c_str()))
        throw std::runtime_error("Cannot exclusively publish "+analysisName+" analysis");
    Fd parentFd(::open(outputPath.substr(0,slash).c_str(),O_RDONLY|O_DIRECTORY|O_CLOEXEC));
    if (parentFd.value<0 || ::fsync(parentFd.value))
        throw std::runtime_error("Cannot sync "+analysisName+" analysis output");
    if (::unlink(pending.c_str()) || ::fsync(parentFd.value))
        throw std::runtime_error(publishedObjectName+
            " published but pending-link cleanup/sync failed");
}

}
}
