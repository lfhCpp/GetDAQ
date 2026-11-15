#ifndef GETDAQ_RECEIVERPLANTRANSITION_H
#define GETDAQ_RECEIVERPLANTRANSITION_H
#include "planio.h"
#include <memory>
namespace GetDAQ {
struct SupervisorControlOptions;
class SupervisorNextPlan {
public:
    SupervisorNextPlan(const std::string &file,const std::string &sha256,
                       const DataSupervisorOptions &original,const std::string &originalExecutableSha256);
    ~SupervisorNextPlan();
    void Check() const;
    std::string Bytes() const;
    SupervisorPlanDocument Document() const;
private:
    class Impl;
    std::shared_ptr<Impl> fImpl;
};
// On the receiver host, exclusively stage one frozen scan successor plan and
// create its private scan audit parent. This does not retire the current plan.
std::string StageScanSuccessorPlan(const std::string &sourcePlanFile,const std::string &targetPlanFile,
                                   const DataSupervisorOptions &original,const std::string &originalExecutableSha256);
std::string StageScanSuccessorPlanBytes(const std::string &planBytes,const std::string &targetPlanFile,
                                        const DataSupervisorOptions &original,const std::string &originalExecutableSha256);
void PreflightScanSuccessorPlanBytes(const std::string &planBytes,const std::string &targetPlanFile,
                                     const DataSupervisorOptions &original,const std::string &originalExecutableSha256);
// Explicit predecessor retirement, not cold PID adoption or an inferred EOF.
// Activation is once-only and holds the original deployment lease for life.
class SupervisorPlanPredecessor {
public:
    SupervisorPlanPredecessor(const std::string &proofFile,const SupervisorControlOptions &next);
    ~SupervisorPlanPredecessor();
    SupervisorPlanPredecessor(const SupervisorPlanPredecessor &)=delete;
    SupervisorPlanPredecessor &operator=(const SupervisorPlanPredecessor &)=delete;
    void Activate();
    void Check() const;
    bool Activated() const;
    std::string Sha256() const;
    std::string Json() const;
    std::string OwnerScopeSha256() const;
    std::string OwnerEpoch() const;
    std::string NextPlanSha256() const;
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
}
#endif
