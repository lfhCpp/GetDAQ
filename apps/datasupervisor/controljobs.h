#ifndef GETDAQ_SUPERVISORCONTROLJOBS_H
#define GETDAQ_SUPERVISORCONTROLJOBS_H
#include "controlprotocol.h"
#include <cstddef>
#include <memory>
namespace GetDAQ {
class SupervisorPlanPredecessor;
struct SupervisorControlOptions {
    DataSupervisorOptions receiver;
    std::string executableSha256,agentId,controllerId;
};
struct SupervisorControlOperation {
    SupervisorControlRequest request;
    std::string requestKey,phase,message,resultStatusJson,catalogResultJson;
    std::string handoffProofSha256;
    std::string planRetirementProofSha256;
};
std::string EncodeSupervisorControlOperation(const SupervisorControlOperation &operation);
// 低频固定计划控制核心；一个后台操作，不排队。已有ID只查询/返回结果。
// 不含认证材料；凭据检查由网络边界实现。首个变更绑定控制者scope/epoch。
class SupervisorControl {
public:
    explicit SupervisorControl(const SupervisorControlOptions &options);
    SupervisorControl(const SupervisorControlOptions &options,std::shared_ptr<const SupervisorPlanPredecessor> predecessor);
    ~SupervisorControl(); // 等自有后台操作结束；不杀receiver、不清故障。
    SupervisorControl(const SupervisorControl &)=delete;
    SupervisorControl &operator=(const SupervisorControl &)=delete;
    SupervisorControlOperation Submit(const SupervisorControlRequest &request);
    SupervisorControlOperation Operation(const std::string &id) const;
    std::string CatalogReplacementObservationJson(const std::string &publishOperationId) const;
    std::string IdentityJson() const;
    std::string StatusJson() const;
    std::string ReceiversJson() const;
    std::string ReceiverEventsJson(std::size_t sourceIndex,bool fileBytes=false) const; // Single-source, identity-checked low-frequency count.
    std::string InstanceId() const;
    std::string PlanSha256() const;
    std::string PlanRetirementJson() const;
    std::string PlanRetirementCompletionJson() const;
    std::string ControlHandoffCompletionJson() const;
    std::string PredecessorJson() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
