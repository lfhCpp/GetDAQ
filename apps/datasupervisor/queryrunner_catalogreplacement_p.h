#ifndef GETDAQ_SUPERVISOR_QUERYRUNNER_CATALOGREPLACEMENT_P_H
#define GETDAQ_SUPERVISOR_QUERYRUNNER_CATALOGREPLACEMENT_P_H

#include "controlhttp.h"
#include <string>

namespace GetDAQ { namespace SupervisorQueryPrivate {

SupervisorHttpResponse ObserveCatalogReplacement(SupervisorHttpClient &client,
                                                   const std::string &publishOperationId,
                                                   const std::string &agentId,
                                                   const std::string &controllerId,
                                                   const std::string &planSha256,
                                                   const std::string &instanceId);

} }

#endif
