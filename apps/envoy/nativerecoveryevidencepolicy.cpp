#include "nativerecoveryevidencepolicy.h"
#include <map>

namespace GetDAQ {
QString NativeRecoveryEvidenceError(const EccControlIdentity &identity,
                                    const EccControlSystemStatus &ready,
                                    const EccControlSystemStatus &current) {
    if (identity.instanceId!=current.instanceId ||
        ready.snapshotSha256!=current.snapshotSha256)
        return QStringLiteral("Native recovery instance/snapshot differs; hardware reconciliation is required");

    std::map<std::string,const EccControlReceiverStatus *> currentReceivers;
    for (const auto &node:current.nodes)
        for (const auto &receiver:node.receivers)
            currentReceivers[receiver.id]=&receiver;

    std::size_t count=0;
    for (const auto &node:ready.nodes) {
        for (const auto &receiver:node.receivers) {
            const auto found=currentReceivers.find(receiver.id);
            if (found==currentReceivers.end() ||
                found->second->sessionId!=receiver.sessionId ||
                found->second->pid!=receiver.pid ||
                found->second->outputDirectory!=receiver.outputDirectory)
                return QStringLiteral("Native recovery receiver session/PID/dataset differs");
            ++count;
        }
    }
    if (count!=currentReceivers.size() || count==0 ||
        current.controller.state<3 || current.controller.state>6)
        return QStringLiteral("Native recovery coverage/state differs");
    return QString();
}
}
