#ifndef GETDAQ_SYSTEMACQUISITION_H
#define GETDAQ_SYSTEMACQUISITION_H
#include "systempreparation.h"
#include "datalinksetclient.h"
namespace GetDAQ {
// 同一已Configured准备对象的运行协调；prepared必须比本对象长寿。
// 不传原始帧，不提交生产ECC，也不管理receiver进程/归档或未知状态恢复。
class SystemAcquisitionClient {
public:
    enum class State : unsigned int { Configured, Connecting, Ready, Starting, Running,
                                     Pausing, Paused, Resuming, Stopping, Stopped,
                                     Disconnecting, Closed, Failed, Breaking, Broken };
    SystemAcquisitionClient(SystemPreparationClient &prepared,const std::vector<DataSenderOptions> &senders);
    ~SystemAcquisitionClient(); // 没有自动Stop/Disconnect。
    SystemAcquisitionClient(const SystemAcquisitionClient &) = delete;
    SystemAcquisitionClient &operator=(const SystemAcquisitionClient &) = delete;
    void ConnectData(const std::string &newAuditDirectory);
    void Start(const std::string &newAuditDirectory); // Ready或成功Stopped；后者保留连接但再次RunInit。
    void Pause(const std::string &newAuditDirectory);
    void Resume(const std::string &newAuditDirectory);
    void Stop(const std::string &newAuditDirectory); // 保持告警不阻断此停止路径。
    // Failed后的显式生产停止；不重发先前已尝试的停止，不碰未知sender/连接，状态仍Failed。
    // parentOutcomeUnknownConfirmed仅供持久ECC已UNKNOWN、但子运行步骤已成功的情况。
    // 允许Configured/Ready/Running/Paused/Stopped进入Failed并仅停生产。
    // Broken且父UNKNOWN时仅核对原停止结果和新鲜停止寄存器，不重发Stop/Disconnect。
    // 已创建的运行对象须保留；不能退回准备对象忘记连接，也不能清UNKNOWN。
    void StopProductionAfterFailure(const std::string &newAuditDirectory,bool parentOutcomeUnknownConfirmed=false);
    void DisconnectData(const std::string &newAuditDirectory); // Ready/Stopped；Closed不是receiver封存。
    void BindIceReceiverSessions(const std::vector<IceReceiverSessionBinding> &bindings);
    // Ready/Running/Paused/Stopped/Closed：停生产/刷新(按实际阶段)、断开并清全部CoBo就绪。
    // 成功后本对象Broken、同一准备对象Prepared；监听器/对齐快照保留，receiver封存另核对。
    void Breakup(const std::string &newAuditDirectory);
    // Explicit negative cleanup of the same failed acquisition, after every
    // retained producer-stop outcome is confirmed. Never adopts unknown
    // senders or retries attempted cleanup. Does not seal data or clear ECC.
    void RetireFailedIceSources(const std::string &newAuditDirectory,bool parentOutcomeUnknownConfirmed);
    std::string VerifyRetiredIceSources(); // Same original controls; all retained failed ICE streams acknowledged retired.
    std::vector<std::pair<std::string,std::string>> SourceRetirementAuditChains() const;
    void BreakupAfterFailure(const std::string &newAuditDirectory,bool parentOutcomeUnknownConfirmed);
    std::vector<std::pair<std::string,std::string>> FailureBreakupAuditChains() const;
    State GetState() const;
    std::string CoBoLifecycleJson() const; // Same live per-board phase snapshot as the retained preparation object.
    DataSenderSnapshot SenderStatus(const std::string &id) const;
    std::vector<std::shared_ptr<DataSenderClient>> ObservationClients() const;
    bool CheckIceSourceFailures(); // Explicit failure-stop admission; read-only RPC, latch local unknown on failed control validation.
    bool IceSourceOutcomeUnknown() const; // Cached only, no network.
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
