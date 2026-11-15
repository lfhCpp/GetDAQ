#ifndef GETDAQ_ECCSTATEMACHINE_H
#define GETDAQ_ECCSTATEMACHINE_H
#include <mutex>
#include <string>

namespace GetDAQ {
// 数值兼容旧 GET SM.h；Off 保留。新内存对象初始为 Idle，持久化对象从日志恢复。
enum class EccState { Off = 0, Idle = 1, Described = 2, Prepared = 3, Ready = 4, Running = 5, Paused = 6 };
enum class EccCommand { Describe = 1, Prepare = 2, Configure = 3, Start = 4, Stop = 5,
                        Pause = 6, Resume = 7, Breakup = 8, Reset = 9,
                        ReprepareCoBo = 11, SetCoBoLed = 12, TriggerAsAdPulser = 13, BlinkCoBoLeds = 14, FlipCoBoLed = 15, PulseCoBoLed = 16,
                        ConfigureCoBo = 17 }; // 10 remains the persisted reconciliation record.
struct EccStatus {
    EccState state = EccState::Idle; // 最近确认状态；unknown/待核对时不代表当前硬件状态。
    int transition = 0;
    int error = 0;
    bool outcomeUnknown = false;
    bool needsReconciliation = false; // 重启后的硬件状态尚未核对，或持久化不可用。
    std::string operationId;
    std::string message;
};
// 接口不引入文件系统/解析依赖，具体 POSIX 日志独立实现。
class EccPersistence {
public:
    virtual ~EccPersistence() {}
    virtual EccStatus Load() const = 0;
    virtual void CheckNewId(const std::string &id, const std::string &details) const = 0;
    virtual void Begin(const std::string &id, EccCommand command, EccState from, EccState to,
                       const std::string &requestKey) = 0;
    virtual void Complete(const std::string &id) = 0;
    virtual void Unknown(const std::string &id, const std::string &message) = 0;
    virtual void Reconciled(const std::string &id, EccState state, const std::string &evidence) = 0;
    virtual void Held(const std::string &originalOperationId,const std::string &evidence); // Default rejects; never rewrites the original result.
};
class EccBackend {
public:
    virtual ~EccBackend() {}
    // 必须无硬件写入；缺少流程/配置/元数据时在此拒绝，不允许空操作冒充成功。
    virtual void Validate(EccCommand command, EccState from, EccState to) = 0;
    // 执行完整流程并确认完成，负责操作日志。任何异常都按可能部分生效处理。
    virtual void Apply(EccCommand command, EccState from, EccState to) = 0;
    // 必须实际核对硬件/配置/操作记录，无硬件写入。默认拒绝，避免缺少恢复流程时清除 unknown。
    virtual void VerifyRecovery(EccState state, const EccStatus &lastStatus, const std::string &evidence);
};
// 无 Qt、Ice、旧 StateMachine 库依赖。原生服务与协调器在 apps/ecc/native；此类只约束状态转换。
class EccStateMachine {
public:
    explicit EccStateMachine(EccBackend &backend);
    EccStateMachine(EccBackend &backend, EccPersistence &persistence);
    EccStatus Status() const;
    void Execute(EccCommand command);
    void Execute(EccCommand command, const std::string &operationId, const std::string &requestKey);
    // 仅允许待核对/unknown 状态；后端核对成功、恢复记录持久化成功后才允许普通命令。
    void Reconcile(const std::string &operationId, EccState verifiedState, const std::string &evidence);
    void HoldForReconciliation(const std::string &originalOperationId,const std::string &evidence);
    static EccState Target(EccState from, EccCommand command);
private:
    EccBackend &fBackend;
    EccPersistence *fPersistence = nullptr; // 必须比状态机存活更久。
    mutable std::mutex fMutex;
    EccStatus fStatus;
};
}
#endif
