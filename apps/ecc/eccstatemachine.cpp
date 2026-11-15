#include "eccstatemachine.h"
#include "../../common/text/utf8.h"
#include <stdexcept>

// 转换关系依据 GET 20210208 BackEnd::createEngine；原始 CEA/CeCILL 源码保留在 source/。
namespace GetDAQ {
EccStateMachine::EccStateMachine(EccBackend &backend) : fBackend(backend) {}
EccStateMachine::EccStateMachine(EccBackend &backend, EccPersistence &persistence)
    : fBackend(backend), fPersistence(&persistence), fStatus(persistence.Load()) {}
void EccBackend::VerifyRecovery(EccState, const EccStatus &, const std::string &)
{
    throw std::runtime_error("Hardware recovery verification is not implemented");
}
void EccPersistence::Held(const std::string &,const std::string &) {
    throw std::runtime_error("Persistent external reconciliation hold is not implemented");
}
void EccStateMachine::HoldForReconciliation(const std::string &originalId,const std::string &evidence) {
    std::lock_guard<std::mutex> lock(fMutex);
    if(!fPersistence || fStatus.transition || originalId.empty() || evidence.empty() || fStatus.operationId!=originalId)
        throw std::logic_error("External hold requires the same persisted original operation and no transition");
    // Latch before persistence: a failed fsync must not permit new operations.
    fStatus.outcomeUnknown=true;fStatus.needsReconciliation=true;
    fStatus.message="Original operation held for external reconciliation";
    fPersistence->Held(originalId,evidence);
}
EccStatus EccStateMachine::Status() const
{
    std::lock_guard<std::mutex> lock(fMutex);
    return fStatus;
}
EccState EccStateMachine::Target(EccState from, EccCommand command)
{
    switch (command) {
    case EccCommand::Describe: if (from == EccState::Idle) return EccState::Described; break;
    case EccCommand::Prepare: if (from == EccState::Described) return EccState::Prepared; break;
    // These target one CoBo while the deployment-wide ECC remains Prepared;
    // they are maintenance commands, not transitions of an independent CoBo FSM.
    case EccCommand::ReprepareCoBo: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::ConfigureCoBo: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::SetCoBoLed: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::BlinkCoBoLeds: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::FlipCoBoLed: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::PulseCoBoLed: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::TriggerAsAdPulser: if (from == EccState::Prepared) return EccState::Prepared; break;
    case EccCommand::Configure: if (from == EccState::Prepared) return EccState::Ready; break;
    case EccCommand::Start: if (from == EccState::Ready) return EccState::Running; break;
    case EccCommand::Stop:
        if (from == EccState::Running || from == EccState::Paused) return EccState::Ready;
        break;
    case EccCommand::Pause: if (from == EccState::Running) return EccState::Paused; break;
    case EccCommand::Resume: if (from == EccState::Paused) return EccState::Running; break;
    case EccCommand::Breakup:
        if (from == EccState::Ready || from == EccState::Running || from == EccState::Paused) return EccState::Prepared;
        break;
    case EccCommand::Reset:
        if (from == EccState::Prepared) return EccState::Described;
        if (from == EccState::Described) return EccState::Idle;
        break;
    }
    throw std::invalid_argument("ECC command is not valid in the current state");
}
void EccStateMachine::Execute(EccCommand command)
{
    if (fPersistence) throw std::invalid_argument("Persistent ECC requires an operation ID and request key");
    Execute(command, "", "");
}
void EccStateMachine::Execute(EccCommand command, const std::string &operationId, const std::string &requestKey)
{
    EccState from, to;
    {
        std::lock_guard<std::mutex> lock(fMutex);
        if (fStatus.transition) throw std::runtime_error("ECC transition already in progress; command was not queued");
        if (fStatus.outcomeUnknown) throw std::runtime_error("ECC outcome UNKNOWN; reconcile hardware and operation logs before further commands");
        if (fStatus.needsReconciliation) throw std::runtime_error("ECC startup/storage requires hardware reconciliation before commands");
        if (fPersistence && (operationId.empty() || requestKey.empty()))
            throw std::invalid_argument("Persistent ECC requires an operation ID and request key");
        from = fStatus.state;
        to = Target(from, command); // 拒绝非法/重复命令，不修改状态，也不调用后端。
        fStatus.transition = static_cast<int>(command);
        fStatus.error = 0;
        fStatus.message.clear();
    }
    bool applying = false;
    bool storing = false;
    try {
        // 不持有状态锁执行慢操作，状态查询可立即返回。
        if (fPersistence) fPersistence->CheckNewId(operationId, requestKey);
        fBackend.Validate(command, from, to);
        if (fPersistence) {
            storing = true;
            fPersistence->Begin(operationId, command, from, to, requestKey);
            storing = false;
        }
        applying = true;
        { std::lock_guard<std::mutex> lock(fMutex); fStatus.operationId = operationId; }
        fBackend.Apply(command, from, to);
        if (fPersistence) {
            storing = true;
            fPersistence->Complete(operationId);
            storing = false;
        }
        std::lock_guard<std::mutex> lock(fMutex);
        fStatus.state = to;
        fStatus.transition = 0;
    } catch (...) {
        std::string message;
        try { throw; }
        catch (const std::exception &e) { message = EccErrorText(e.what()); }
        catch (...) { message = "Non-standard exception in ECC backend"; }
        bool storageFailed = storing;
        if (applying && fPersistence && !storing) {
            try { fPersistence->Unknown(operationId, message); }
            catch (...) { storageFailed = true; }
        }
        // Apply 异常不能简单视为“没有执行”：已确认的旧状态仅供诊断。
        std::lock_guard<std::mutex> lock(fMutex);
        fStatus.transition = 0;
        fStatus.error = static_cast<int>(command);
        fStatus.outcomeUnknown = applying;
        fStatus.needsReconciliation = storageFailed;
        if (applying || storing) fStatus.operationId = operationId;
        fStatus.message = message;
        throw;
    }
}
void EccStateMachine::Reconcile(const std::string &operationId, EccState verifiedState, const std::string &evidence)
{
    EccStatus previous;
    {
        std::lock_guard<std::mutex> lock(fMutex);
        if (!fPersistence) throw std::runtime_error("Recovery requires a persistent operation journal");
        if (fStatus.transition) throw std::runtime_error("ECC transition already in progress; recovery was not queued");
        if (!fStatus.outcomeUnknown && !fStatus.needsReconciliation)
            throw std::runtime_error("ECC does not require reconciliation");
        if (verifiedState < EccState::Idle || verifiedState > EccState::Paused || operationId.empty() || evidence.empty())
            throw std::invalid_argument("Recovery requires state 1..6, a new operation ID and evidence");
        previous = fStatus;
        fStatus.transition = 10; // 仅新核心的恢复标志，不扩展旧 GET Transition 枚举。
    }
    bool storing = false;
    try {
        fPersistence->CheckNewId(operationId, evidence);
        fBackend.VerifyRecovery(verifiedState, previous, evidence);
        storing = true;
        fPersistence->Reconciled(operationId, verifiedState, evidence);
        storing = false;
        std::lock_guard<std::mutex> lock(fMutex);
        fStatus.state = verifiedState;
        fStatus.transition = 0;
        fStatus.error = 0;
        fStatus.outcomeUnknown = false;
        fStatus.needsReconciliation = false;
        fStatus.operationId = operationId;
        fStatus.message.clear();
    } catch (...) {
        std::lock_guard<std::mutex> lock(fMutex);
        fStatus = previous;
        fStatus.error = 10;
        fStatus.needsReconciliation = previous.needsReconciliation || storing;
        try { throw; }
        catch (const std::exception &e) { fStatus.message = EccErrorText(e.what()); }
        catch (...) { fStatus.message = "Non-standard exception during hardware reconciliation"; }
        throw;
    }
}
}
