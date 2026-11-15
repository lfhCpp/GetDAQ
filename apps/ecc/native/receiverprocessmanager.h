#ifndef GETDAQ_RECEIVERPROCESSMANAGER_H
#define GETDAQ_RECEIVERPROCESSMANAGER_H
#include "nativeecccoordinator.h"
#include "datasupervisor.h"
#include <mutex>
namespace GetDAQ {
// 将无Qt/Ice的本地主机进程核心接到ECC；只传控制元数据。
// 当前同一主机；远端接收主机须另接协议适配器，不能由控制端转发原始事件。
class ReceiverProcessManager : public EccDataLifecycle {
public:
    explicit ReceiverProcessManager(const DataSupervisorOptions &options);
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
    void AbortStartup(const std::string &newAuditDirectory);
    void AbortFailedReceivers(const std::string &newAuditDirectory,const std::string &parentFailureOperationId) override;
    void RetireFailedReceivers(const std::string &newAuditDirectory,const std::string &parentFailureOperationId) override;
    DataSupervisorStatus Status() const;
private:
    DataSupervisorOptions fOptions;
    DataSupervisor fSupervisor;
    std::string fControllerScope;
    std::mutex fControllerScopeMutex;
    void MatchSenders(const std::vector<DataSenderOptions> &senders) const;
};
}
#endif
