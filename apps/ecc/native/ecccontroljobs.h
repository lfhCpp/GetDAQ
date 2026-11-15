#ifndef GETDAQ_ECCCONTROLJOBS_H
#define GETDAQ_ECCCONTROLJOBS_H
#include "fixedeccconfiguration.h"
#include "eccprotocol.h"
namespace GetDAQ {
// 固定配置、长寿命原生ECC与数据适配器的低频后台入口；不排队、不重放、不转发原始数据。
class EccControlJobs {
public:
    explicit EccControlJobs(FixedEccConfiguration &configuration);
    EccControlJobs(FixedEccConfiguration &configuration,std::shared_ptr<const EccSuccessorPlan> successor,
                   const std::string &handoffReceiptJson);
    ~EccControlJobs(); // 等当前动作结束；没有隐式停生产/断开/kill策略。
    EccControlOperation Submit(const EccControlRequest &request);
    EccControlOperation Operation(const std::string &id) const;
    std::string IdentityJson() const;
    std::string StatusJson() const;
    std::string AlarmsJson() const; // 仅后台发布的缓存；不读取硬件或原始数据。
    std::string ReceiverFailureJson() const; // Cached policy/evidence; no network or raw data.
    std::string ReceiversJson() const; // 调用方必须在后台执行；有界只读远端进程观察。
    std::string ReceiverEventsJson(const std::string &senderId,bool fileBytes=false) const; // Single fixed source, original run/session/PID bound.
    std::string SourcesJson() const; // Background-only, bounded read-only source discovery, no raw bytes/actions.
    std::string HardwareStateJson() const;
    std::string ConfigurationJson() const; // 启动时选定输入缓存；实际板端加载仍未验证。
    std::string OwnershipJson() const; // Cached same-host reservation; no board-global ownership claim.
    std::string ConfigureReadbacksJson() const; // Historical hardware audit checks and current parent status; no RPC.
    std::string MaintenanceTargetsJson() const; // Fixed targets plus journal-projected lifecycle evidence; no RPC/readback or authorization grant.
    std::string ControlHandoffJson() const; // Original completed receipt only; no RPC or implicit transfer.
    void ValidateServiceShutdown(const std::string &instanceId,const std::string &snapshotSha256,
                                 const std::string &handoffOperationId) const; // Only after clean fenced data handoff.
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
