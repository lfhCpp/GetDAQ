#ifndef GETDAQ_REMOTERECEIVERMANAGER_H
#define GETDAQ_REMOTERECEIVERMANAGER_H
#include "nativeecccoordinator.h"
#include "controlhttp.h"
#include <functional>
namespace GetDAQ {
class EccSuccessorPlan;
struct EccReceiverPlanTarget;
struct RemoteReceiverEndpoint {
    std::string agentId,planSha256;
    SupervisorHttpOptions control;
};
struct RemoteReceiverOptions {
    std::string controllerId;
    std::vector<RemoteReceiverEndpoint> endpoints;
    unsigned int commandTimeoutMs=600000,pollIntervalMs=100;
};
// Pure canonical plan hash: validates declarations, never reads credentials or
// creates HTTP clients. It grants no remote access or controller ownership.
std::string RemoteReceiverBindingSha256(const RemoteReceiverOptions &options);
struct RemoteReceiverNodeStatus {
    std::string agentId,instanceId,planSha256;
    std::string catalogJson;
    bool catalogEnabled=false;
    DataSupervisorStatus data;
};
struct RemoteReceiverStatus {
    bool needsReconciliation=false;
    std::string controllerScopeSha256,controllerEpoch,message;
    std::vector<RemoteReceiverNodeStatus> nodes;
};
// 原生低频远程适配器。服务管理各自主机上的receiver；板端MFM不经本类。
// 每次进程动作保存全部节点意图，再逐节点一次POST。失联/部分结果保持故障，无回滚。
class RemoteReceiverManager : public EccDataLifecycle {
public:
    explicit RemoteReceiverManager(const RemoteReceiverOptions &options);
    RemoteReceiverManager(const RemoteReceiverOptions &options,std::shared_ptr<const EccSuccessorPlan> successor);
    ~RemoteReceiverManager(); // 不发远端命令、不kill、不接管旧进程。
    std::string BindingSha256() const override;
    void BindController(const std::string &snapshotSha256,const std::string &auditRoot) override;
    bool NeedsReconciliation() const override;
    void Validate(EccCommand command,EccState from,const std::vector<DataSenderOptions> &senders,
                  const std::string &configurationSha256,const std::string &artifactSha256) const override;
    void VerifyStopped() const override;
    void StartReceivers(const std::string &newAuditDirectory,const std::vector<DataSenderOptions> &senders) override;
    void VerifyReady() const override;
    std::vector<IceReceiverSessionBinding> IceReceiverSessions() const override;
    void FinishReceivers(const std::string &newAuditDirectory) override;
    void FinishReceivers(const std::string &newAuditDirectory,const std::vector<std::string> &unusedSourceIds) override;
    void CatalogReceivers(const std::string &newAuditDirectory,const std::string &experiment,unsigned int runNumber,
                          bool replaceCompletedRun=false);
    void PrepareCatalogReplacement(const std::string &newAuditDirectory,const std::string &experiment,
                                   unsigned int runNumber);
    void PublishCatalogReplacement(const std::string &newAuditDirectory,const std::string &experiment,
                                   unsigned int runNumber);
    void FinalizeCatalogReplacement(const std::string &newAuditDirectory,const std::string &experiment,
                                    unsigned int runNumber);
    // Resume only a previously failed/uncertain Publish, after read-only
    // observation proves every submitted child operation terminal and every
    // unsubmitted child operation absent. Reuses the stable replacement ID.
    void ResumeCatalogReplacement(const std::string &newAuditDirectory,
                                  const std::string &originalPublishAuditDirectory,
                                  const std::string &experiment,unsigned int runNumber);
    // One-shot parent-manager reconstruction for a failed catalog replacement
    // while the data Supervisor instances remain alive. Revalidates the
    // original fixed controller/data binding and sealed remote cycles, then
    // uses the explicit operation-observation path. The caller-supplied
    // boundary must recheck current Prepared hardware and independent
    // controller holds before every read or mutation. This helper is not
    // integrated into the ECC service and does not restore a long-lived
    // manager; it cannot recover after a data Supervisor process restart.
    static RemoteReceiverStatus ResumeColdCatalogReplacement(const RemoteReceiverOptions &options,
        const NativeEccOptions &controller,const std::string &originalPublishAuditDirectory,
        const std::string &newOperationId,const std::string &experiment,unsigned int runNumber,
        const std::function<void()> &boundary);
    bool HasRetainedCatalogPublishAudit(const std::string &originalPublishAuditDirectory) const;
    void AbortFailedReceivers(const std::string &newAuditDirectory,const std::string &parentFailureOperationId) override;
    void RetireFailedReceivers(const std::string &newAuditDirectory,const std::string &parentFailureOperationId) override;
    // Explicit original-manager transfer after a normally sealed cycle. All
    // intents are saved before the first POST; partial/uncertain results fence
    // this manager permanently. Same immutable data plans, no payload transfer.
    void HandoffControl(const std::string &newAuditDirectory,const std::string &successorScopeSha256,
                        const std::string &successorEpoch);
    std::string CompletedControlHandoffJson() const;
    // The successor must already have its own bound parent root/random epoch.
    // Only this matching original handoff receipt permits adopting the old
    // normally sealed cycle; fresh HTTP history/status are checked before use.
    void AdoptControlHandoff(const std::string &originalHandoffReceiptJson);
    std::string SuccessorBindingSha256(const std::vector<EccReceiverPlanTarget> &targets) const;
    void RetirePlans(const std::string &newAuditDirectory,const std::vector<EccReceiverPlanTarget> &targets,
                     const std::string &successorScopeSha256,const std::string &successorEpoch);
    // Explicit completion on the retained original manager. Observe accepted
    // original operations; submit only requests proven previously rejected or
    // never sent. Never changes target plans, clears the fence or adopts a cold
    // instance. Every attempt gets its own audit directory.
    void CompletePendingHandoff(const std::string &newAuditDirectory);
    // Explicit cold completion of the saved DATA handoff only. Holds the old
    // native journal exclusively without appending/opening a new ECC instance;
    // accepts only a historically released Idle/Described controller. The
    // caller must keep fixed inputs/successor authority checked by boundary.
    // No manager escapes this call, no old hardware authority is restored.
    static std::string CompleteColdHandoff(const RemoteReceiverOptions &options,
        const NativeEccOptions &controller,const std::string &originalAuditDirectory,
        const std::string &newOperationId,const std::function<void()> &boundary);
    bool CanCatalog() const; // 缓存的完整Stopped/固定归档计划门槛；不访问文件或网络。
    RemoteReceiverStatus Status() const; // 短快照；不是实时数据计数/健康查询。
    std::string ObserveReceiversJson() const; // 有界只读HTTP；独立客户端，不改管理状态/控制权。
    std::string ObserveAcquisitionEventsJson(const std::string &senderId,bool fileBytes=false) const; // One identity-bound receiver query; metadata only.
private:
    RemoteReceiverManager(const RemoteReceiverOptions &options,const std::string &originalEpoch);
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
