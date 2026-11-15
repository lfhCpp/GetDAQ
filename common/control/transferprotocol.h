#ifndef GETDAQ_TRANSFERPROTOCOL_H
#define GETDAQ_TRANSFERPROTOCOL_H
#include <string>
#include <cstdint>
namespace GetDAQ {
struct TransferIdentity {std::string agentId,instanceId,policySha256,sourceRoot,backupRoot;};
struct TransferRequest {std::string operationId,instanceId,policySha256,experiment,sourceRequestKey;unsigned int runNumber=0;};
struct TransferReconciliationRequest {
    std::string verificationId,operationId,instanceId,policySha256,originalRequestKey;
};
struct TransferOperation {
    TransferRequest request;
    std::string requestKey,phase,message,source,destination,receipt;
    std::uint64_t copiedBytes=0;
};
std::string EncodeTransferIdentity(const TransferIdentity &identity);
TransferIdentity DecodeTransferIdentity(const std::string &bytes);
std::string EncodeTransferRequest(const TransferRequest &request);
TransferRequest DecodeTransferRequest(const std::string &bytes);
std::string TransferRequestKey(const TransferRequest &request);
std::string EncodeTransferOperation(const TransferOperation &operation);
TransferOperation DecodeTransferOperation(const std::string &bytes);
std::string EncodeTransferReconciliationRequest(const TransferReconciliationRequest &request);
TransferReconciliationRequest DecodeTransferReconciliationRequest(const std::string &bytes);
std::string EncodeTransferResumeRequest(const TransferReconciliationRequest &request);
TransferReconciliationRequest DecodeTransferResumeRequest(const std::string &bytes);
}
#endif
