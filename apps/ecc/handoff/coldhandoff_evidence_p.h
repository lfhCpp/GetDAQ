#ifndef GETDAQ_COLDHANDOFF_EVIDENCE_P_H
#define GETDAQ_COLDHANDOFF_EVIDENCE_P_H

#include "eccprotocol.h"
#include "remotehandoffevidence.h"

#include <string>
#include <vector>

namespace GetDAQ {

void ValidateColdParentHistory(const std::string &bytes,const EccControlRequest &original);
std::string BuildColdCompletionReceipt(const std::string &scope,const std::string &epoch,
    const std::string &originalPath,const std::vector<RemoteHandoffNodeEvidence> &nodes,
    const std::string &journal);

} // namespace GetDAQ

#endif
