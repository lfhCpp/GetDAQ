#ifndef GETDAQ_SUPERVISOR_QUERYRUNNER_PLANTRANSITION_P_H
#define GETDAQ_SUPERVISOR_QUERYRUNNER_PLANTRANSITION_P_H

#include "controlhttp.h"
#include <string>

namespace GetDAQ { namespace SupervisorQueryPrivate {

void ValidatePlanTransitionResponse(const SupervisorHttpResponse &response, bool predecessor,
                                    const std::string &agentId, const std::string &controllerId,
                                    const std::string &planSha256, const std::string &instanceId);

} }

#endif
