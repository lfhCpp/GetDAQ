#ifndef GETDAQ_ECCJOURNAL_H
#define GETDAQ_ECCJOURNAL_H
#include "eccstatemachine.h"
#include <memory>
#include <vector>

namespace GetDAQ {
struct EccOperation {
    std::string id, phase, requestKey, evidence, message;
    int command = 0; // 原GET命令1..9，恢复记录10，选定CoBo重准备11。
    EccState from = EccState::Idle, to = EccState::Idle;
};
enum class EccJournalMode { Writable, ReadOnly, ExclusiveReadOnly };
// POSIX 本机日志，单一文件独占锁、追加写、每条 fsync；不删除/截断/重放旧记录。
// scope 必须标识真实目标与部署；变更目标不能沿用另一套设备的日志。
class EccJournal : public EccPersistence {
public:
    EccJournal(const std::string &path, const std::string &scope, EccJournalMode mode = EccJournalMode::Writable);
    ~EccJournal();
    EccJournal(const EccJournal &) = delete;
    EccJournal &operator=(const EccJournal &) = delete;
    EccStatus Load() const override;
    void CheckNewId(const std::string &id, const std::string &details) const override;
    void Begin(const std::string &id, EccCommand command, EccState from, EccState to, const std::string &requestKey) override;
    void Complete(const std::string &id) override;
    void Unknown(const std::string &id, const std::string &message) override;
    void Reconciled(const std::string &id, EccState state, const std::string &evidence) override;
    void Held(const std::string &originalOperationId,const std::string &evidence) override;
    std::vector<EccOperation> Operations() const;
    EccOperation Operation(const std::string &id) const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
