#ifndef GETDAQ_COLDHANDOFFPROTOCOL_H
#define GETDAQ_COLDHANDOFFPROTOCOL_H
#include <string>
namespace GetDAQ {
// 固定恢复服务只接受意图和原尝试ID，不接受可执行文件、命令或路径。
struct ColdHandoffIdentity {
    std::string serviceId,instanceId,policySha256,originalOperationId,originalRequestSha256,successorPlanSha256;
};
struct ColdHandoffRequest {
    std::string taskId,instanceId,policySha256,action,attemptId;
};
struct ColdHandoffOperation {
    ColdHandoffRequest request;
    std::string requestKey,phase,message,receipt,receiptSha256;
};
std::string EncodeColdHandoffIdentity(const ColdHandoffIdentity &value);
ColdHandoffIdentity DecodeColdHandoffIdentity(const std::string &bytes);
std::string EncodeColdHandoffRequest(const ColdHandoffRequest &value);
ColdHandoffRequest DecodeColdHandoffRequest(const std::string &bytes);
std::string ColdHandoffRequestKey(const ColdHandoffRequest &value);
std::string EncodeColdHandoffOperation(const ColdHandoffOperation &value);
ColdHandoffOperation DecodeColdHandoffOperation(const std::string &bytes);
}
#endif
