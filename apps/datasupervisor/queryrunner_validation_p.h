#ifndef GETDAQ_SUPERVISOR_QUERYRUNNER_VALIDATION_P_H
#define GETDAQ_SUPERVISOR_QUERYRUNNER_VALIDATION_P_H

#include "controlhttp.h"
#include "planio.h"
#include "supervisorcommon.h"
#include <string>

namespace GetDAQ { namespace SupervisorQueryPrivate {

JsonValue DecodeResponse(const SupervisorHttpResponse &response,unsigned int expectedCode);
void ValidateScope(const JsonValue &value,const std::string &planSha256);
void ValidateOperation(const JsonValue &value,const std::string &operationId,
                       const std::string &controllerId,const std::string &planSha256);

} }

#endif
