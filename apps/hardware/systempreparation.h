#ifndef GETDAQ_SYSTEMPREPARATION_H
#define GETDAQ_SYSTEMPREPARATION_H
#include "systemalignment.h"
#include "cobolifecycle.h"
#include "coboprepareclient.h"
#include "coboasadconfiguration.h"
#include "mutantconfiguration.h"
#include <memory>
#include <utility>
namespace GetDAQ {
class SystemAcquisitionClient;
struct SystemCoBoSettings {
    std::string id, linkProxy;
    bool enabled = true;
    CoBoPrepareOptions prepare;
    CoBoConfigureSettings configure;
};
struct SystemMutantSettings {
    AlignmentMutantEndpoint endpoint;
    MutantSettings settings;
};
struct SystemPreparationOptions {
    std::vector<SystemCoBoSettings> cobos;
    std::vector<SystemMutantSettings> mutants; // 0为独立CoBo，1..3为v1.5主/从系统。
    int encodingMinor = 0, timeoutMs = 3000;
    bool exclusiveOwnerConfirmed = false, dataProcessesStopped = false;
    std::string configurationSha256, artifactSha256; // 工件输入绑定；不是远端已加载版本或授权。
};
// 原生主机Prepare+Configure协调器。配置在构造时复制/编译；不传原始DAQ字节。
// Prepared/Configured仅为本对象的命令阶段；没有提交生产ECC状态或连接数据发送器。
class SystemPreparationClient {
public:
    enum class State : unsigned int { Fresh, Preparing, Prepared, Configuring, Configured, Failed,
                                     Invalidating, Unpreparing, Unprepared, Maintaining };
    explicit SystemPreparationClient(const SystemPreparationOptions &options);
    ~SystemPreparationClient(); // 本地资源释放；没有隐式Stop/注销/恢复。
    SystemPreparationClient(const SystemPreparationClient &) = delete;
    SystemPreparationClient &operator=(const SystemPreparationClient &) = delete;
    void Prepare(const std::string &newAuditDirectory);
    void Configure(); // 仅Prepared；成功Breakup返回Prepared后可用同一配置/掩码再次Configure。
    // Prepared下只重新准备选定活动CoBo，保留其余板/机箱对齐及原配置。
    // 必须由上层重新核对所有数据进程停止；不会Configure/Start或隐式重试。
    // 目标旧订阅明确注销后释放其监听器，再按原设置建立新订阅；失败保持Failed。
    void ReprepareCoBo(const std::string &coboId,const std::string &newAuditDirectory,
                       bool dataProcessesStoppedConfirmed);
    // Apply the retained configuration plan to one selected CoBo while the
    // enclosing system remains Prepared. This does not set initDone, align,
    // configure MuTaNT, or start acquisition.
    void ConfigureCoBo(const std::string &coboId,const std::string &newAuditDirectory,
                       bool dataProcessesStoppedConfirmed);
    // Change one supported LED state on one active CoBo only while Prepared.
    // This does not reconfigure the board or prove its physical lamp state.
    void SetCoBoLed(const std::string &coboId,unsigned int asadId,HardwareStep::LedType type,
                    HardwareStep::LedState state,const std::string &newAuditDirectory,
                    const std::function<void()> &checkpoint=std::function<void()>());
    void BlinkCoBoLeds(const std::string &coboId,const std::string &newAuditDirectory,
                       const std::function<void()> &checkpoint=std::function<void()>());
    void FlipCoBoLed(const std::string &coboId,unsigned int ledId,const std::string &newAuditDirectory,
                     const std::function<void()> &checkpoint=std::function<void()>());
    void PulseCoBoLed(const std::string &coboId,unsigned int ledId,unsigned int periodMs,const std::string &newAuditDirectory,
                      const std::function<void()> &checkpoint=std::function<void()>());
    // Trigger exactly one configured voltage pulse on one active CoBo while Prepared and stopped.
    void TriggerAsAdPulser(const std::string &coboId,int voltageMv,const std::string &newAuditDirectory,
                           const std::function<void()> &checkpoint=std::function<void()>());
    std::string ConfigureReadbacksJson() const; // Completed Configure audit evidence, not a live loaded-version query.
    // Per-board host command lifecycle projection. `unknown` means the last
    // command may have reached hardware but its outcome was not confirmed.
    std::string CoBoLifecycleJson() const;
    // 失败后的明确负向动作；不要求成功Configure/对齐，不改sender/供电/自有订阅。
    // 每个独立停止阶段只尝试一次，失败后保留Failed；不能替代硬件核对恢复。
    // parentOutcomeUnknownConfirmed允许父ECC提交失败后的Prepared/Configured对象停生产。
    void StopProductionAfterFailure(const std::string &newAuditDirectory,bool parentOutcomeUnknownConfirmed=false);
    // 仅Prepared；本次数据进程停止须重新确认，不沿用Prepare之前的声明。
    // 保持告警不阻断负向断电；全部观察/断电确认后才取消自有订阅。不清UNKNOWN。
    void Unprepare(const std::string &newAuditDirectory,bool dataProcessesStoppedConfirmed);
    std::vector<std::pair<std::string,std::string>> UnprepareAuditChains() const; // Actual writers, only after complete retained Unprepare.
    // Retained failed preparation only, after its acknowledged production stop.
    // Power off all declared AsAd and close only known owned subscriptions;
    // never repeats an attempted stage and does not reconcile the parent ECC.
    void UnprepareAfterFailure(const std::string &newAuditDirectory,bool dataProcessesStoppedConfirmed);
    // Only after retained Unprepare or failure cleanup completed. Fresh read-only
    // metadata, production/ready/power and owned-subscription verification.
    void VerifyUnpreparedAfterFailure(const std::string &newAuditDirectory);
    std::vector<std::pair<std::string,std::string>> FailureCleanupAuditChains() const;
    std::vector<std::pair<std::string,std::string>> FailureVerificationAuditChains() const;
    std::vector<std::pair<std::string,std::string>> ConfigurationInvalidationAuditChains() const;
    State GetState() const;
    void CheckInterlock() const; // 所有活动板的保持告警；没有硬件RPC或清除。
    CoBoAlarmClient *Alarms(const std::string &coboId) const; // 返回的对象不超出协调器生命周期。
    SystemAlignmentResult AlignmentResult() const; // 仅Prepared/Configured。
    SystemPreparationOptions Configuration() const; // 不可变输入副本，供同一对象的运行协调绑定。
private:
    friend class SystemAcquisitionClient;
    friend struct HardwareSystemAcquisitionTestAccess;
    std::shared_ptr<CoBoLifecycleLedger> CoBoLifecycleLedgerShared() const;
    // 仅由同一采集对象在全部sender断开确认后调用；不清理实验参数或对齐快照。
    void InvalidateConfiguration(const std::string &newAuditDirectory);
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
