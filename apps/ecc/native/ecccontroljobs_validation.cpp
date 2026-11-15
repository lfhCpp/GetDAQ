#include "ecccontroljobs_p.h"
#include <stdexcept>

namespace GetDAQ {
namespace EccControlJobsInternal {

bool NormalSealedData(const RemoteReceiverStatus &status) {
    if (status.needsReconciliation || status.nodes.empty()) return false;
    for (const auto &node:status.nodes) {
        if (node.data.state!=DataSupervisorStatus::Stopped || node.data.needsReconciliation || node.data.receivers.empty()) return false;
        for (const auto &receiver:node.data.receivers)
            if (!receiver.reaped || receiver.exitCode!=0 || !receiver.sealVerified) return false;
    }
    return true;
}

RemoteReceiverOptions RuntimeReceiverOptions(FixedEccConfiguration &configuration) {
    if(!configuration.ControlAccessValidated())throw std::invalid_argument("Offline evidence configuration cannot start ECC control");
    configuration.Check();return configuration.ReceiverOptions();
}

NativeEccOptions StartupOptions(FixedEccConfiguration &configuration,const RemoteReceiverManager &data,
                                const std::shared_ptr<const EccSuccessorPlan> &successor) {
    if(!configuration.ControlAccessValidated())throw std::invalid_argument("Offline evidence configuration cannot start ECC control");
    configuration.Check();
    const auto options=configuration.ControllerOptions();
    if (successor) {
        successor->Check();
        const auto planned=PlanNativeEccConfiguration(options,data.BindingSha256());
        if (!successor->Activated() || planned.snapshotSha256!=successor->SnapshotSha256() ||
            planned.dataControllerScopeSha256!=successor->ScopeSha256() || options.auditRoot!=successor->AuditRoot())
            throw std::invalid_argument("ECC startup differs from activated successor plan");
    }
    return options;
}

EccCommand Command(const std::string &name) {
    const std::string names[]={"describe","prepare","configure","start","stop","pause","resume","breakup","reset"};
    for (unsigned int i=0;i<9;++i) if (names[i]==name) return EccCommand(i+1);
    throw std::invalid_argument("Unsupported ECC control command");
}

void Validate(const EccControlRequest &request) {
    ValidateEccControlRequest(request);
}

} // namespace EccControlJobsInternal
} // namespace GetDAQ
