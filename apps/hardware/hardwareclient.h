#ifndef GETDAQ_HARDWARECLIENT_H
#define GETDAQ_HARDWARECLIENT_H
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include "../../common/hardware/hardwaredescription.h"

// 无 Qt 的硬件访问边界，Ice 类型仅在实现文件内出现。
namespace GetDAQ {
struct CoBoAgetSettings;
struct CoBoReadoutSettings;
struct CoBoConfigureSettings;
struct CoBoPrepareFoundationSettings;
struct AsAdInputSettings;
struct CoBoClockSettings;
struct CoBoMonitoringSettings;
struct CoBoCalibrationSettings;
struct MutantSettings;
struct HardwareStep {
    enum Kind : unsigned int { WriteValue, ExpectValue, Delay, ReadValue, PulserCommand, PollValue,
                               LedCommand, AsAdAlarmMonitorCommand, ExpectObservedValue, MaskedWriteValue };
    enum PulserOperation { SetDefaultVoltage, TriggerPulser, ConfigureExternal, ConfigurePeriodic,
                           ConfigureDouble, SetRandom, StartPeriodic, StopPeriodic };
    enum LedOperation : unsigned int { SetAllLeds, ModifyLed, BlinkAllLeds, FlipOneLed, FlipAllLeds, PulseOneLed };
    enum LedType : unsigned int { LedP, LedB, LedU, LedD };
    enum LedState : unsigned int { LedOff, LedOn, LedSlowPulse, LedFastPulse };
    Kind kind = WriteValue;
    std::string device, reg, field;
    std::uint64_t value = 0;
    std::uint64_t mask = UINT64_MAX;
    std::uint64_t allowedDifference = 0; // Expect 显式允许的差异位；仍写 actual 与 warning 日志。
    bool warningOnReadback255 = false; // Internal, scoped operator policy only; never a write/stop/connection waiver.
    int delayMs = 0;
    PulserOperation pulserOperation = StopPeriodic;
    int voltageMv = 0;
    std::uint32_t periodMs = 0;
    bool enabled = false;
    LedOperation ledOperation = SetAllLeds;
    LedType ledType = LedP;
    LedState ledState = LedOff;
    unsigned int asadId = 0;
    unsigned int ledId = 0; // GPIO bit 0..15 for FlipOneLed; unused by other operations.
    bool requireBoundDescription = false; // 执行前必须已绑定本地权限/布局策略。
    std::string annotation; // 有界的内部上下文，仅写入 intent，不执行硬件操作。
    int conditionStep = -1; // 只能引用前面的无条件 ReadValue；使用它已观察到的值，不再隐式读取。
    int observationStep = -1; // ExpectObservedValue 检查已记录观察，避免指示故障后再次读硬件改变判断。
    std::uint64_t conditionMask = UINT64_MAX, conditionValue = 0;
    std::uint64_t minimum = 0, maximum = UINT64_MAX;
    unsigned int pollAttempts = 1, zeroReadRetries = 1, pollDelayMs = 0;
};
class HardwareClient {
public:
    explicit HardwareClient(const std::string &nodeProxy, int encodingMinor = 0, int timeoutMs = 3000);
    ~HardwareClient();
    HardwareClient(const HardwareClient &) = delete;
    HardwareClient &operator=(const HardwareClient &) = delete;
    std::string Name();
    std::vector<std::string> Devices();
    DeviceInfo Describe(const std::string &device);
    // 只允许空节点，拒绝隐式 destroy/remove；失败可能部分生效，绝不自动重发。
    void Create(const HardwareDescription &description, const std::string &auditPath);
    // 显式替换描述的前半段：须绑定并核对当前布局/名称；调用者先确认停采集与控制权。
    // destroy虽在旧Slice中为idempotent，也只尝试一次；不停止硬件、不关闭sender。
    void DestroyDescription(const std::string &auditPath);
    // 绑定本地权限策略；实际访问前核对远端结构，不能把旧板端的只读标志当可靠依据。
    void UseDescription(const HardwareDescription &description);
    std::uint64_t Read(const std::string &device, const std::string &reg, const std::string &field = "");
    // 不隐式预读/读回：部分寄存器读操作本身有副作用。
    // 写入必须使用全新日志路径；成功仅表示远端应答，不等于读回一致。
    void Write(const std::string &device, const std::string &reg, const std::string &field,
               std::uint64_t value, const std::string &auditPath);
    // 顺序执行、有完整预检查和逐步日志；失败停止，不回滚或重放已执行步骤。
    void Execute(const std::vector<HardwareStep> &steps, const std::string &auditPath);
    // 原生协调器的停止联锁；检查器抛异常时停止后段，不回滚、不发送Stop。
    // 在步骤、轮询读取、读改写写入前及等待期间调用；不能打断已经发出的RPC。
    // 检查器须快速、无硬件写入，不能重入本HardwareClient（执行期间持有互斥锁）。
    void Execute(const std::vector<HardwareStep> &steps, const std::string &auditPath,
                 const std::function<void()> &checkpoint);
    // 复用Execute的完整软件/远端元数据检查，不读寄存器值、不写入、不创建日志。
    // 只作跨节点操作的预检查；执行时仍重新检查，不能代替独占或防止外部修改。
    void Validate(const std::vector<HardwareStep> &steps);
    std::string CompletedAuditChainSha256() const; // Latest successful Execute, never a Validate or partial execution.
private:
    void Process(const std::vector<HardwareStep> &steps, const std::string &auditPath, bool execute,
                 const std::function<void()> &checkpoint);
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
std::string JsonString(const std::string &value);
std::uint64_t ParseUnsigned(const std::string &value);
HardwareDescription LoadHardwareDescription(const std::string &path);
HardwareDescription DecodeHardwareDescription(const std::string &bytes);
// 撤销准备：观察实际上电位图，选择/读取保护状态；断电为独立负向动作。
// 上层先全系统核对停采集/数据生命周期，再执行；不取消告警或清除UNKNOWN。
std::vector<HardwareStep> CoBoUnprepareMonitoring();
std::vector<HardwareStep> CoBoAsAdPowerOff();
void ValidateHardwareDescription(const HardwareDescription &description);
// 保存实际绑定的完整权限/布局快照；不重新读取外部文件。
std::string HardwarePolicyJson(const HardwareDescription &description);
// 迁移旧 CoBoNode::pipeInit，仅初始化流水线，不是完整 Prepare 或单板重对齐。
std::vector<HardwareStep> CoBoPipelineInit();
// 完整迁移 asadCal 子流程；mask 为实际启用的 AsAd 位图 1..15。
// 前提：已上电并完成相关初始化、当前没有采集；不是完整 Prepare/自动恢复。
std::vector<HardwareStep> CoBoAsAdCalibrate(unsigned int mask);
// 旧三脉冲慢控制切换，保留控制器复位及 1000ms 等待；不等于完整 AGET 校准。
std::vector<HardwareStep> CoBoSwitchToSlowControl(unsigned int agetMask);
// 完整的 AGET 寄存器/读出掩码子流程，非完整 CoBo Configure。
// 前提：已完成 Prepare/串行校准、无采集、目标布局绑定且由同一控制者独占。
std::vector<HardwareStep> CoBoAgetConfigure(const CoBoAgetSettings &settings);
// AGET + CoBo Module/零抑制/缓冲区；仍缺 AsAd/2p 初始化及完整 ECC Configure。
// 同样要求完整 Prepare、停止采集/接收和控制权独占；缓冲区指针重置不能用于运行中。
std::vector<HardwareStep> CoBoReadoutConfigure(const CoBoReadoutSettings &settings);
// CoBo Configure 参数子流程：AGET/AsAd/Module/pulser 配置和 20ms 2p 初始化。
// 需要绑定描述和同一控制者已完成 Prepare/串行校准、停止数据读取；不启动周期 pulser/采集。
std::vector<HardwareStep> CoBoConfigure(const CoBoConfigureSettings &settings);
// Prepare 子流程；必须由完整协调器按上电/监控/时钟/AGET 校准次序调用，不能提交 Prepared。
std::vector<HardwareStep> CoBoPrepareFoundation(const CoBoPrepareFoundationSettings &settings);
std::vector<HardwareStep> CoBoAsAdInputProtections(unsigned int asadMask);
std::vector<HardwareStep> CoBoAsAdInputsInit(const AsAdInputSettings &settings);
// 最后一次ADC复位后只重应用ADC通道关闭位；不改变IM时钟/保护策略，不读只写ADC。
std::vector<HardwareStep> CoBoAsAdAdcChannelsFinal(const AsAdInputSettings &settings);
std::vector<HardwareStep> CoBoAsAdAdcInit(unsigned int asadMask, bool doubleDataRate = true);
std::vector<HardwareStep> CoBoAsAdDacInit(unsigned int asadMask);
// 在时钟初始化之后设置；R-CoBo 手动延迟才生效，不等于自动延迟校准。
std::vector<HardwareStep> CoBoManualReadDataDelay(unsigned int delay);
// 整板 ClockInit 参数子流程；只确认命令应答，不证明 PLL 锁定、相位或完整 Prepared。
std::vector<HardwareStep> CoBoClockInit(const CoBoClockSettings &settings);
// AsAd 上电和监控/自动保护子流程；完整 Prepare、连续告警服务和独占控制仍由后端负责。
std::vector<HardwareStep> CoBoAsAdPowerCycle(const CoBoMonitoringSettings &settings);
std::vector<HardwareStep> CoBoAsAdMonitoringInit(const CoBoMonitoringSettings &settings);
std::vector<HardwareStep> CoBoAsAdProtectionInit(const CoBoMonitoringSettings &settings);
std::vector<HardwareStep> CoBoAsAdMonitoringRead(const CoBoMonitoringSettings &settings);
// 组合上电、慢控制校准、监控启动、VDD 稳定和输入保护；仍不是完整 Prepare。
std::vector<HardwareStep> CoBoAsAdPrepare(const CoBoMonitoringSettings &settings);
// 显式板端LED管理器和全板告警检查任务；订阅回调生命周期由独立CoBoAlarmClient管理。
std::vector<HardwareStep> CoBoLedSetAll(bool enabled, unsigned int hardwareVersion);
std::vector<HardwareStep> CoBoLedBlinkAll();
std::vector<HardwareStep> CoBoLedFlip(unsigned int ledId); // 0..15 is one bit, 16 means all LEDs.
std::vector<HardwareStep> CoBoLedPulse(unsigned int ledId,unsigned int periodMs); // 0..15 is one LED, 16 all LEDs; zero stops that pulser.
std::vector<HardwareStep> CoBoLedModify(HardwareStep::LedType type, unsigned int asadId,
                                      HardwareStep::LedState state, unsigned int hardwareVersion);
std::vector<HardwareStep> CoBoAsAdAlarmMonitorInit(const CoBoMonitoringSettings &settings);
// MuTaNT寄存器子流程；对齐、数据sender与完整生产ECC状态仍由协调器负责。
std::vector<HardwareStep> MutantPrepare(const MutantSettings &settings);
std::vector<HardwareStep> MutantConfigure(const MutantSettings &settings);
std::vector<HardwareStep> MutantRunInit(const MutantSettings &settings);
std::vector<HardwareStep> MutantWscaClock(bool enabled);
std::vector<HardwareStep> MutantSyncPulse(bool enabled);
std::vector<HardwareStep> MutantClockShelfMasks(unsigned int gmc, unsigned int wsca);
// 串行校准包含 ADC 测试向量及活动芯片版本检查；改变 ADC 配置，之后仍需 ADCInit。
std::vector<HardwareStep> CoBoAgetCalibrate(const CoBoCalibrationSettings &settings);
std::vector<HardwareStep> CoBoAgetInit(const CoBoCalibrationSettings &settings);
// 自动延迟计算会覆盖 AGET reg1/reg2，并临时使能固件；必须在最终 Configure 前执行。
std::vector<HardwareStep> CoBoAutomaticReadDelayCalibrate(const CoBoCalibrationSettings &settings);
// 仅选择自动/手动延迟；与是否重新计算独立，仍要求停止采集、绑定描述及独占控制。
std::vector<HardwareStep> CoBoSelectReadDataDelay(const CoBoCalibrationSettings &settings);
// 保留其他 reg1 位，通过板端已有 DACthreshold 字段访问设置 0..7；不等于最终 Configure。
std::vector<HardwareStep> CoBoAgetGlobalThreshold(const CoBoCalibrationSettings &settings, unsigned int threshold);
// 组合带条件的子计划时必须调用此函数，自动平移片段内的观察索引。
void AppendHardwareSteps(std::vector<HardwareStep> &destination, const std::vector<HardwareStep> &fragment);
}
#endif
