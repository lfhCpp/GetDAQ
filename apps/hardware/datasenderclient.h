#ifndef GETDAQ_DATASENDERCLIENT_H
#define GETDAQ_DATASENDERCLIENT_H
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
struct IceReceiverSessionBinding {std::string senderId,sessionId;};
struct DataSourceObservation {
    std::string senderId,protocol,phase="unbound",message;
    std::string sourceInstanceId,receiverSession,state,error;
    unsigned int localState=0;
    std::uint64_t observedAtUnixMs=0,issuedBytes=0,locallySentBytes=0;
    bool startIssued=false,stopAcknowledged=false,drainAcknowledged=false;
    bool finishIssued=false,sourceEndAcknowledged=false,failureStopIssued=false,failureStopAcknowledged=false;
};
struct DataSenderOptions {
    std::string senderId, controlProxy, receiverAddress; // 接收地址为明确 IPv4:port。
    std::string protocol = "TCP"; // FDT在Start/Stop重连；接收端须用受管理多连接周期。
    unsigned int circularBufferMask = 1;
    bool alwaysFlush = false;
    bool exclusiveStoppedOwnerConfirmed = false; // 调用者前提声明；旧接口无法验证独占/停止。
    int encodingMinor = 0, timeoutMs = 30000; // Stop可能等待中断线程和大数据刷新。
};
struct DataSenderSnapshot {
    enum State : unsigned int { Fresh, Connecting, Connected, Starting, Running,
                                Stopping, Stopped, Disconnecting, Disconnected, Unknown };
    State state = Fresh, lastConfirmedState = Fresh;
    bool outcomeUnknown = false;
    bool startIssued = false; // 本对象曾发出daqStart；失败/UNKNOWN后也不清除，不据零数据猜测未使用。
    std::string receiverSession,sourceInstanceId;
    bool sourceEndAcknowledged=false,failureStopIssued=false,failureStopAcknowledged=false;
    bool failureRetirementIssued=false,failureRetirementAcknowledged=false;
    std::uint64_t issuedCalls = 0, acknowledgedCalls = 0;
    std::string operation;
};
// 仅控制板端DaqCtrlNode，不读写原始事件；实例跨Pause/Resume保留同一连接。
// 状态表示调用应答与本地持久记录，不证明硬件停止/接收器封存或生产ECC状态。
class DataSenderClient {
public:
    explicit DataSenderClient(const DataSenderOptions &options); // 只解析代理，不连接远端。
    ~DataSenderClient(); // 只释放本地Ice资源；没有隐式Stop/Disconnect。
    DataSenderClient(const DataSenderClient &) = delete;
    DataSenderClient &operator=(const DataSenderClient &) = delete;
    void Validate(); // 实际ice_isA，只读；Connect及后续动作仍重新检查。
    void BindIceReceiverSession(const std::string &session); // Fresh only; runtime binding from verified receiver lifecycle.
    void Connect(const std::string &newAuditPath, const std::function<void()> &checkpoint = {});
    void Start(const std::string &newAuditPath, const std::function<void()> &checkpoint = {});
    void Stop(const std::string &newAuditPath); // 不接受告警联锁：保持告警不能阻止停止请求。
    void Disconnect(const std::string &newAuditPath); // 仅Connected/Stopped，绝不代替Stop/flush。
    void StopIceProductionAfterFailure(const std::string &newAuditPath); // One explicit source stop; retains UNKNOWN, no End.
    void StopIceProductionAfterFailure(const std::string &newAuditPath,bool ownerOutcomeUnknownConfirmed);
    void ValidateIceFailureRetirement(); // Same original connection/session, acknowledged failure stop and extension; read-only.
    void RetireIceSourceAfterFailure(const std::string &newAuditPath); // Close only; UNKNOWN and original End flags retained.
    std::string VerifyIceFailureRetirement(); // Read-only fresh receipt on original control connection.
    std::string CompletedFailureRetirementAuditChainSha256() const;
    DataSourceObservation ObserveSource(unsigned int timeoutMs=500) const; // Separate read-only peer; no control adoption or state changes.
    DataSenderSnapshot Snapshot() const;
    std::string CompletedCommandAuditChainSha256() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
// 全集合本地检查（ID/实际代理身份/接收地址唯一性），不调用任何远端。
// 上层还需对所有客户端Validate，再开始第一条Connect，避免后段错误留下前段连接。
void ValidateDataSenderOptions(const DataSenderOptions &options);
void ValidateDataSenderSet(const std::vector<DataSenderOptions> &options);
}
#endif
