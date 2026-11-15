#include "eccrequestproof_p.h"

#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"

#include <stdexcept>
#include <utility>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;

EccControlRequestProof::EccControlRequestProof(
    const std::string &path,const EccControlRequest &request,bool replay,
    std::shared_ptr<EccSuccessorReview> heldReview)
    : fDirectory(path),fBytes(EncodeEccControlRequest(request)),fReview(std::move(heldReview))
{
    if (!request.successorPlanFile.empty()) {
        const auto file=fDirectory+"/reviewed-successor-plan.json";
        if (replay) fReviewedBytes=ReadFile(file,65536);
        else {
            if (!fReview) throw std::runtime_error("Reviewed retirement lacks its pre-acceptance successor lease");
            fReviewedBytes=fReview->Json(); WriteNew(file,fReviewedBytes);
        }
        if (ConfigurationSha256(fReviewedBytes)!=request.successorPlanSha256 ||
            ::lstat(file.c_str(),&fReviewedIdentity))
            throw std::runtime_error("Accepted reviewed successor copy differs from request");
    }
    if (::lstat(fDirectory.c_str(),&fDirectoryIdentity) ||
        ::lstat((fDirectory+"/request.json").c_str(),&fFileIdentity))
        throw std::runtime_error("Cannot bind accepted ECC network request");
    Check();
}

void EccControlRequestProof::Check() const
{
    struct stat current;
    if (::lstat(fDirectory.c_str(),&current) || !Same(current,fDirectoryIdentity) || !S_ISDIR(current.st_mode) ||
        current.st_uid!=::geteuid() || (current.st_mode&0777)!=0700 ||
        ::lstat((fDirectory+"/request.json").c_str(),&current) || !Same(current,fFileIdentity) || !S_ISREG(current.st_mode) ||
        current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600 ||
        ReadFile(fDirectory+"/request.json",8192)!=fBytes)
        throw std::runtime_error("Accepted ECC network request identity/permissions/content changed");
    if (fReview) fReview->Check();
    if (!fReviewedBytes.empty()) {
        if (::lstat((fDirectory+"/reviewed-successor-plan.json").c_str(),&current) ||
            !Same(current,fReviewedIdentity) || !S_ISREG(current.st_mode) ||
            current.st_uid!=::geteuid() || (current.st_mode&0777)!=0600 ||
            ReadFile(fDirectory+"/reviewed-successor-plan.json",65536)!=fReviewedBytes)
            throw std::runtime_error("Accepted reviewed successor copy identity/content changed");
    }
}

void EccControlRequestProof::ReleaseReview() const
{
    if (fReview) fReview->Release();
}

}
