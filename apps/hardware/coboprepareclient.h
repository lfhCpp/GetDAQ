#ifndef GETDAQ_COBOPREPARECLIENT_H
#define GETDAQ_COBOPREPARECLIENT_H
#include "hardwareclient.h"
#include "coboalarmclient.h"
#include "coboprepareconfiguration.h"
namespace GetDAQ {
struct CoBoPreparePlan {
    std::vector<HardwareStep> beforeSubscription, afterSubscription;
};
// 整板旧Prepare命令组合；主机订阅必须由CoBoPrepareClient插入两段之间。
CoBoPreparePlan BuildCoBoPreparePlan(const CoBoPrepareSettings &settings);
struct CoBoPrepareOptions {
    std::string nodeProxy, expectedNodeName;
    HardwareDescription description;
    CoBoPrepareSettings settings;
    int encodingMinor = 0, timeoutMs = 3000;
    std::string callbackEndpoints, publishedEndpoints;
    unsigned int maxPendingAlarms = 256;
    std::uint64_t maxAlarmAuditBytes = 16*1024*1024;
    // 由生产协调器提供的前提声明；本类不实现远端锁、发送器/接收器Stop或跨板对齐。
    bool ownsStoppedHardware = false, dataProcessesStopped = false;
};
// 无Qt的长寿命单板协调器；成功后仍保留自有订阅，供Configure/运行协调器消费。
// Complete只确认本板命令流程，不提交整个系统ECC Prepared。
class CoBoPrepareClient {
public:
    enum State : unsigned int { Fresh, Running, Complete, Failed, Preflighted };
    explicit CoBoPrepareClient(const CoBoPrepareOptions &options); // 纯参数检查，无连接。
    ~CoBoPrepareClient(); // 只释放本地资源，不自动Stop/unsubscribe/复位。
    CoBoPrepareClient(const CoBoPrepareClient &) = delete;
    CoBoPrepareClient &operator=(const CoBoPrepareClient &) = delete;
    void Prepare(const std::string &newAuditDirectory);
    // 全系统首次写入前调用；只创建本地日志/监听器并检查元数据及停止前提。
    // additionalSteps用于提前检查后续Configure等步骤；不执行这些步骤。
    void Preflight(const std::string &newAuditDirectory,
                   const std::vector<HardwareStep> &additionalSteps = std::vector<HardwareStep>());
    // 接续同一对象的Preflight，保留原监听器；checkpoint可检查其他板的保持告警。
    void Prepare(const std::function<void()> &checkpoint);
    void CheckInterlock() const; // 无硬件访问，不清除告警；Failed也拒绝继续。
    State Status() const;
    // Preflighted/Complete/Failed可取；禁用订阅时返回nullptr。生命周期不得超过本对象。
    CoBoAlarmClient *Alarms() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
