#ifndef GETDAQ_REMOTEOPERATIONCODEC_H
#define GETDAQ_REMOTEOPERATIONCODEC_H

#include "controlprotocol.h"
#include "strictjson.h"
#include "supervisorcommon.h"

namespace GetDAQ {
namespace RemoteOperationCodec {

struct OperationResult {
    std::string phase,message,catalogJson,handoffProofSha256,completionOperationId,completionRequestJson;
    bool hasData=false;
    DataSupervisorStatus data;
};

SupervisorControlRequest DecodeRequestValue(const JsonValue &value);
OperationResult DecodeOperation(const JsonValue &value,const SupervisorControlRequest &request);

} // namespace RemoteOperationCodec
} // namespace GetDAQ

#endif
