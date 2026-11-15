#ifndef GETDAQ_ECCSUCCESSORPLAN_H
#define GETDAQ_ECCSUCCESSORPLAN_H
#include <memory>
#include <string>
namespace GetDAQ {
class FixedEccConfiguration;
struct EccControlRequest;
// Original ECC holds the service-host successor file while a reviewed GUI
// transition is running. No fixed-config construction, activation or RPC.
class EccSuccessorReview {
public:
    explicit EccSuccessorReview(const EccControlRequest &request);
    ~EccSuccessorReview();
    EccSuccessorReview(const EccSuccessorReview &)=delete;
    EccSuccessorReview &operator=(const EccSuccessorReview &)=delete;
    void Check() const;
    void Release();
    std::string Json() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
// 离线冻结接班目标。检查不取得控制权；一次性激活也不能替代原交接回执。
class EccSuccessorPlan {
public:
    static std::string Create(const std::string &file,FixedEccConfiguration &configuration);
    EccSuccessorPlan(const std::string &file,FixedEccConfiguration &configuration);
    ~EccSuccessorPlan();
    EccSuccessorPlan(const EccSuccessorPlan &)=delete;
    EccSuccessorPlan &operator=(const EccSuccessorPlan &)=delete;
    void Check() const;
    void ActivateForStartup(); // 独占、一次性；失败后的标记保留，不能自动重试。
    bool Activated() const;
    bool Faulted() const;
    std::string Json() const;
    std::string Sha256() const;
    std::string Epoch() const;
    std::string SnapshotSha256() const;
    std::string ScopeSha256() const;
    std::string DataBindingSha256() const;
    std::string AuditRoot() const;
    std::string ControllerId() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
