#ifndef GETDAQ_NATIVEECCCOORDINATOR_H
#define GETDAQ_NATIVEECCCOORDINATOR_H
#include "eccjournal.h"
#include "systemdescription.h"
#include "systempreparation.h"
#include "datasenderclient.h"
#include <memory>
#include <stdexcept>
namespace GetDAQ {
// 控制端只协调独立数据进程；接口不允许传入或返回原始事件帧。
// 实现必须长于NativeEccCoordinator；Validate/Verify方法不得改变硬件或进程。
// 没有默认成功实现。接收器真正EOF/排空/fsync/退出/解锁/封存后Finish才可返回。
class EccDataLifecycle {
public:
    virtual ~EccDataLifecycle() {}
    virtual std::string BindingSha256() const = 0; // 不可变接收计划摘要，进入父ECC快照。
    // 父快照和审计根核对后调用；不能发送远端命令或启动/停止进程。
    virtual void BindController(const std::string &snapshotSha256,const std::string &auditRoot) = 0;
    virtual bool NeedsReconciliation() const = 0; // 仅缓存；不能等待网络。
    virtual void Validate(EccCommand command,EccState from,const std::vector<DataSenderOptions> &senders,
                          const std::string &configurationSha256,const std::string &artifactSha256) const = 0;
    virtual void VerifyStopped() const = 0;
    virtual void StartReceivers(const std::string &newAuditDirectory,const std::vector<DataSenderOptions> &senders) = 0;
    virtual void VerifyReady() const = 0;
    virtual std::vector<IceReceiverSessionBinding> IceReceiverSessions() const {
        throw std::logic_error("Data lifecycle does not provide verified ICE receiver sessions");
    }
    virtual void FinishReceivers(const std::string &newAuditDirectory) = 0;
    virtual void AbortFailedReceivers(const std::string &,const std::string &) {
        throw std::logic_error("Data lifecycle does not implement retained failed-session cleanup");
    }
    virtual void RetireFailedReceivers(const std::string &,const std::string &) {
        throw std::logic_error("Data lifecycle does not implement retained failed-session retirement");
    }
    virtual void FinishReceivers(const std::string &newAuditDirectory,const std::vector<std::string> &unusedSourceIds) {
        if (!unusedSourceIds.empty()) throw std::logic_error("Data lifecycle does not implement explicit unused-source termination");
        FinishReceivers(newAuditDirectory);
    }
};
struct NativeEccOptions {
    std::string auditRoot, deploymentId;
    std::string hostControlOwnershipRoot; // Optional shared directory on this ECC host; not a board/cross-host lock.
    std::string deploymentSourceSha256; // 可选固定输入文件绑定；不是实际板端加载版本。
    std::string configurationPublicationSha256; // 可选发布清单字节摘要，仍不代表板端读回。
    std::string configurationSelectionSha256,configurationRepositoryId,configurationRegistrationSha256;
    std::string experimentArtifactBytes; // 必须是完整收到的工件字节；内部重新解码和核对摘要。
    SystemDescriptionOptions description;
    SystemPreparationOptions endpoints; // 保留端点/描述/回调策略，实验参数由工件替换。
    std::vector<DataSenderOptions> senders;
};
struct NativeEccPlan {
    std::string snapshotJson,snapshotSha256,dataControllerScopeSha256;
};
// Pure input compilation shared with real startup. Does not create audit roots,
// acquire host reservations, connect hardware or bind/start data processes.
NativeEccPlan PlanNativeEccConfiguration(const NativeEccOptions &options,const std::string &dataLifecycleBindingSha256);
struct NativeEccCoBoAlarm {
    std::string id;
    CoBoAlarmSnapshot snapshot;
};
struct NativeEccAlarms {
    EccStatus controller;
    std::vector<NativeEccCoBoAlarm> nodes;
    bool stopRequested=false;
};
// 无Qt的原生协调库；长期持有同一准备对象、告警订阅、对齐快照和私有运行对象。
// 尚非网络服务/真实硬件全局锁/完整恢复实现。重开日志必须核对，当前恢复默认拒绝。
class NativeEccCoordinator {
public:
    NativeEccCoordinator(const NativeEccOptions &options,EccDataLifecycle &data);
    ~NativeEccCoordinator(); // 没有自动Stop/Disconnect/Unprepare/数据进程清理。
    NativeEccCoordinator(const NativeEccCoordinator &) = delete;
    NativeEccCoordinator &operator=(const NativeEccCoordinator &) = delete;
    void Execute(EccCommand command,const std::string &operationId); // 并发请求立即拒绝，不排队。
    void ReprepareCoBo(const std::string &operationId,const std::string &coboId);
    void ConfigureCoBo(const std::string &operationId,const std::string &coboId);
    void SetCoBoLed(const std::string &operationId,const std::string &coboId,unsigned int asadId,
                    unsigned int ledType,unsigned int ledState);
    void BlinkCoBoLeds(const std::string &operationId,const std::string &coboId);
    void FlipCoBoLed(const std::string &operationId,const std::string &coboId,unsigned int ledId);
    void PulseCoBoLed(const std::string &operationId,const std::string &coboId,unsigned int ledId,unsigned int periodMs);
    void TriggerAsAdPulser(const std::string &operationId,const std::string &coboId,int voltageMv);
    std::vector<std::pair<std::string,std::string>> MaintenanceTargets() const; // Immutable active IDs/names; no RPC.
    EccStatus Status() const; // 不等待慢硬件RPC。
    // Check the original live control journal and immutable inputs in known
    // Idle/Described. Does not clear or ignore independent hardware holds; the
    // separately retained data-handoff fence is intentionally not released.
    void CheckHandoffCompletionBoundary() const;
    EccOperation Operation(const std::string &operationId) const;
    std::vector<EccOperation> Operations() const;
    void Reconcile(const std::string &operationId,EccState verifiedState,const std::string &evidence);
    // Same live instance, failed Describe only. Reads final descriptions and
    // stopped/ready bits, checks stopped data, records proof, then commits
    // Described. Does not repair partially created layouts or recover a Run.
    void ReconcileDescription(const std::string &operationId,const std::string &failedOperationId);
    // ECC UNKNOWN且保留准备或运行对象时显式停生产；准备失败不要求已有采集对象。
    // 不碰sender/接收器，不清UNKNOWN；未Failed的子对象进入Failed以阻止继续运行。
    // 独立负向操作只记录自己的目录，不伪装成普通ECC Stop/Ready提交。
    void StopProductionAfterFailure(const std::string &operationId);
    void UnprepareAfterFailure(const std::string &operationId); // Same failed Prepare, audited stopped production; keeps parent UNKNOWN.
    void ReconcilePreparation(const std::string &operationId,const std::string &failedOperationId);
    void ReleaseStoppedAlarm(const std::string &operationId,const std::string &stoppedOperationId); // Normal Breakup/seal, verified Unprepare; no UNKNOWN adoption.
    void ReconcileAcquisition(const std::string &operationId,const std::string &failedOperationId);
    void RetireFailedSources(const std::string &operationId); // Original stopped ICE connections; no End/seal/UNKNOWN reset.
    void ReconcileAcquisitionWithSources(const std::string &operationId,const std::string &failedOperationId);
    void BreakupAfterFailure(const std::string &operationId); // Same failed acquisition and confirmed production stop; keeps UNKNOWN/data reconciliation.
    void AbortFailedReceivers(const std::string &operationId); // Only after the same retained object's audited production stop; keeps UNKNOWN.
    void RetireFailedReceivers(const std::string &operationId); // Original stop/connection/Abort proofs; retires data only, keeps UNKNOWN/Run incomplete.
    void UnprepareAcquisitionAfterFailure(const std::string &operationId); // Retired data, same preparation; power off/close owned subscriptions, keeps UNKNOWN.
    // Called only after the network layer has durably bound receiver-failure evidence.
    // Preserves the retained operation/state; prevents positive commands and enables negative stop.
    void HoldReceiverFailure(const std::string &expectedOperationId);
    // Network layer has durably held an external input fault. Keep the original
    // in-memory targets and operation; do not reload or clear hardware UNKNOWN.
    void HoldControlInputFailure(const std::string &expectedOperationId);
    std::string SnapshotSha256() const;
    std::string ConfigurationSha256() const;
    std::string ArtifactSha256() const;
    // Internal recovery gate: includes controller/input/source/receiver/storage/
    // ownership holds but excludes the separate data-lifecycle reconciliation bit.
    // The caller must independently prove and bind that data hold before use.
    EccStatus ControlStatus() const;
    std::string OwnershipJson() const; // Cached lifetime reservation; no filesystem/RPC check on the caller thread.
    std::string ConfigureReadbacksJson() const; // Historical Configure audit evidence; no live readback or loaded-version claim.
    // 仅同一保留对象的回调缓存；不发硬件/数据RPC，不消费或清除告警。
    // 正在执行命令时立即返回false，避免与Reset释放准备对象并发。
    bool ObserveAlarms(NativeEccAlarms &observation) const;
    std::string ObserveSourcesJson(unsigned int budgetMs=2000) const; // Read-only bounded discovery; no original control adoption/actions.
    std::string ObserveHardwareStateJson(unsigned int budgetMs=2000) const;
private:
    void ExecuteChecked(EccCommand command,const std::string &operationId,bool verifyAlarmRelease,
                        const std::string &maintenanceNodeId=std::string(),unsigned int asadId=0,
                        unsigned int ledType=0,unsigned int ledState=0,int pulserVoltageMv=0,unsigned int ledFlipId=0,
                        unsigned int ledPulseId=0,unsigned int ledPulsePeriodMs=0);
    void ReconcileOriginal(const std::string &operationId,const std::string &failedOperationId,bool preparationFailure,bool acquisitionFailure=false,bool sourceFailure=false);
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
