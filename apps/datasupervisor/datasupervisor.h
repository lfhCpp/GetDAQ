#ifndef GETDAQ_DATASUPERVISOR_H
#define GETDAQ_DATASUPERVISOR_H
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
struct ReceiverSourcePlan {
    std::string id, receiverAddress, bindAddress, outputRoot;
    unsigned int dataPort=0, statusPort=0, exportPort=0;
    unsigned int queueMiB=64, maxFrameMiB=32, exportQueueMiB=16;
    std::uint64_t rotateMiB=1024;
    std::uint64_t minFreeMiB=0; // Per output volume; zero keeps legacy behavior.
    std::string fileNaming = "native"; // 非默认命名进入计划摘要。
    std::string inputProtocol = "TCP"; // 非默认模式进入计划；FDT用私有结束控制保持多EOF同会话。
    std::string storageMode = "frame-storage"; // byte-storage只保留源字节，不解析MFM。
};
struct DataSupervisorOptions {
    std::string auditRoot, deploymentId, receiverExecutable;
    std::string iceReceiverExecutable, iceReceiverSha256; // 可选ICE程序；进入固定计划且逐次核对。
    std::string configurationSha256, artifactSha256;
    std::string catalogRoot; // 可选固定同文件系统归档根；留空时保持旧计划字节。
    std::vector<ReceiverSourcePlan> sources;
    unsigned int startupTimeoutMs=30000, finishTimeoutMs=30000, statusTimeoutMs=1000;
    std::uint64_t maxTotalQueueMiB=8192, maxTotalFrameMiB=8192;
};
struct ManagedReceiverStatus {
    std::string id, outputDirectory, sessionId;
    int pid=0, exitCode=-1;
    bool reaped=false, sealVerified=false;
    bool sourceUnusedConfirmed=false; // 明确未使用FDT来源；没有发送器EOF，不代表采集完成。
    bool sourceEndConfirmed=false; // ICE同连接应用结束，经专属凭据核对；不是TCP EOF。
    std::uint64_t receivedBytes=0, frames=0, files=0;
};
struct DataSupervisorStatus {
    enum State { Fresh, Starting, Ready, Finishing, Stopped, Failed, Aborted, Retired };
    State state=Fresh;
    bool needsReconciliation=false;
    std::string planSha256, sessionId, message;
    std::vector<ManagedReceiverStatus> receivers;
};
struct ReceiverProcessObservation {
    std::string id, state, message; // running/exited/reaped/unavailable/not-started
    int pid=0, exitCode=-1, signal=0;
};
struct DataSupervisorObservation {
    bool busy=false;
    std::uint64_t observedAtUnixMs=0; // 服务端墙钟，仅用于提示时效，不作为事务顺序。
    DataSupervisorStatus lifecycle;
    std::vector<ReceiverProcessObservation> receivers;
};
struct ReceiverAcquisitionEventObservation {
    std::string sourceId,sessionId;
    int pid=0;
    bool available=false;
    std::uint64_t acquisitionEvents=0,observedAtUnixMs=0,fileBytes=0;
    bool fileBytesAvailable=false;
};
// 同一接收主机上的原生进程管理核心；不链接Qt/Ice，不接收/扫描原始事件。
// 重开持久根目录保持核对门槛；不重新启动旧进程、不默认解除故障。
class DataSupervisor {
public:
    explicit DataSupervisor(const DataSupervisorOptions &options);
    ~DataSupervisor(); // 不隐式发送信号；仍在运行的子进程保留，持久证据供恢复。
    DataSupervisor(const DataSupervisor &) = delete;
    DataSupervisor &operator=(const DataSupervisor &) = delete;
    void Validate() const; // 不启动/停止，不修改状态或日志。
    void VerifyStopped() const;
    // Same live object, normal completed session only; recheck released locks,
    // final status/seals and file sizes. No raw payload reads or process actions.
    void VerifyControlHandoff() const;
    std::string CompletedCycleEvidenceJson() const; // Immutable metadata/file identities and raw sizes, never raw contents.
    void Start(const std::string &newAuditDirectory);
    void VerifyReady() const;
    void Finish(const std::string &newAuditDirectory,const std::vector<std::string> &unusedSourceIds={}); // 显式未使用来源仅FDT；其余仍等真实EOF。
    void AbortStartup(const std::string &newAuditDirectory); // 仅失败启动且各子进程从未接入数据。
    // Explicit negative cleanup of the same retained failed session. SIGTERM only,
    // no seal/Finish, no PID adoption, no retry, no clearing reconciliation.
    void AbortFailedSession(const std::string &newAuditDirectory,const std::string &expectedSessionId,
                            const std::string &parentFailureOperationId="");
    // Retires only this object's confirmed failed-session abort. Preserves old
    // files/status; never signals, seals, starts or adopts a receiver.
    void RetireFailedSession(const std::string &newAuditDirectory,const std::string &expectedSessionId,
                             const std::string &parentFailureOperationId="");
    std::string Catalog(const std::string &newAuditDirectory,const std::string &operationId,
                        const std::string &experiment,unsigned int runNumber,const std::string &expectedSession,
                        bool replaceCompletedRun=false,const std::string &replacementPhase="single",
                        const std::string &replacementId="");
    bool CanResumeCatalogReplacement(const std::string &replacementId,const std::string &experiment,
                                     unsigned int runNumber,const std::string &expectedSession,
                                     const std::string &phase="publish") const;
    DataSupervisorStatus Status() const; // 短快照，不做网络/进程等待。
    DataSupervisorObservation ObserveReceivers() const; // try-lock + WNOWAIT；不回收、不发信号、不改生命周期。
    ReceiverAcquisitionEventObservation ObserveAcquisitionEvents(std::size_t sourceIndex,bool fileBytes=false) const; // One local status socket; bounded, identity-checked, no data-plane reads.
    DataSupervisorOptions Configuration() const;
    std::string PlanBytes() const; // 已绑定的规范元数据；不读取事件或重新生成当前配置。
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
