#ifndef GETDAQ_COBOALARMCLIENT_H
#define GETDAQ_COBOALARMCLIENT_H
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace GetDAQ {
struct CoBoAlarmOptions {
    std::string nodeProxy, expectedCoBoId, callbackEndpoints, publishedEndpoints, eventAuditPath;
    int encodingMinor = 0, timeoutMs = 3000;
    unsigned int maxPendingEvents = 256;
    std::uint64_t maxEventAuditBytes = 16*1024*1024;
};
struct CoBoAlarmEvent {
    std::uint64_t sequence = 0, receivedUnixMs = 0;
    unsigned int alarmBits = 0;
};
struct CoBoAlarmSnapshot {
    enum Subscription : unsigned int { Fresh, Subscribing, Active, Unsubscribing, Closed, Unknown };
    Subscription subscription = Fresh;
    std::uint64_t received = 0, durable = 0, lost = 0, invalid = 0;
    unsigned int pending = 0, alarmMask = 0;
    bool stopRequested = false, queueOverflow = false, auditFailed = false, budgetExceeded = false;
};
// 独立的原生Ice回调；每实例随机专用订阅ID，绝不调用reset或复用旧ECC ID。
// stopRequested是供ECC消费的保持信号，不从回调线程自动发送硬件Stop。
class CoBoAlarmClient {
public:
    explicit CoBoAlarmClient(const CoBoAlarmOptions &options);
    ~CoBoAlarmClient(); // 只关闭本地资源，不隐式注销/重发网络操作。
    CoBoAlarmClient(const CoBoAlarmClient &) = delete;
    CoBoAlarmClient &operator=(const CoBoAlarmClient &) = delete;
    // 只检查实际名称和AlarmService类型，不订阅、不reset；供完整流程在首次写入前检查。
    // Subscribe仍重新检查，不能代替控制权或证明回调可达。
    void ValidateService();
    // 核对本对象已持有的Active订阅服务，不重新发现/更换服务、不取消任何订阅。
    void ValidateUnsubscribe();
    void Subscribe(const std::string &newCommandAuditPath);
    void Unsubscribe(const std::string &newCommandAuditPath); // 仅Active；未知状态须先交完整恢复协调器。
    std::string ClosedCommandAuditChainSha256() const;
    CoBoAlarmSnapshot Snapshot() const;
    std::vector<CoBoAlarmEvent> Drain(unsigned int maximum);
    std::string SubscriberId() const;
    std::string CallbackProxy() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
