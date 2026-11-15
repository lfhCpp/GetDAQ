#ifndef GETDAQ_ECCREQUESTPROOF_P_H
#define GETDAQ_ECCREQUESTPROOF_P_H

#include "ecccontroljobs.h"
#include "eccsuccessorplan.h"

#include <memory>
#include <string>
#include <sys/stat.h>

namespace GetDAQ {

// Filesystem identity/content proof for one accepted network operation.
class EccControlRequestProof {
public:
    EccControlRequestProof(const std::string &operationDirectory,
                           const EccControlRequest &request,bool replay=false,
                           std::shared_ptr<EccSuccessorReview> heldReview={});
    void Check() const;
    void ReleaseReview() const;

private:
    std::string fDirectory,fBytes;
    struct stat fDirectoryIdentity{},fFileIdentity{};
    std::shared_ptr<EccSuccessorReview> fReview;
    std::string fReviewedBytes;
    struct stat fReviewedIdentity{};
};

}

#endif
