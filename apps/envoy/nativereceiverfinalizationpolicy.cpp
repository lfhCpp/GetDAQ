#include "nativereceiverfinalizationpolicy.h"
#include <cstdint>
#include <limits>

namespace GetDAQ {
bool HasNormalNativeRetirementEvidence(const EccControlSystemStatus &status) {
    if(status.nodes.empty() || (status.controller.state!=1 && status.controller.state!=2))return false;
    for(const auto &node:status.nodes) {
        if(node.state!="stopped" || node.needsReconciliation || node.receivers.empty())return false;
        for(const auto &receiver:node.receivers)
            if(!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)return false;
    }
    return true;
}

QString NativeReceiverFinalizationError(const EccControlSystemStatus &status,
                                        const RunConfig &config,
                                        bool requireCatalog) {
    std::size_t receivers=0;
    std::size_t expectedReceivers=0;
    for(const auto &node:config.nodes)if(!node.mutant || !node.nativeSenderId.isEmpty())++expectedReceivers;
    for(const auto &node:status.nodes) {
        std::uint64_t files=0;
        if(node.needsReconciliation || node.state!="stopped")
            return QStringLiteral("Native receiver session is not confirmed stopped");
        for(const auto &receiver:node.receivers) {
            if(!receiver.reaped || receiver.exitCode!=0 || !receiver.sealVerified)
                return QStringLiteral("Every receiver must confirm EOF, normal exit and seal");
            if(receivers==std::numeric_limits<std::size_t>::max())
                return QStringLiteral("Native final receiver coverage overflow");
            ++receivers;
            if(files>std::numeric_limits<std::uint64_t>::max()-receiver.files)
                return QStringLiteral("Native catalog file count overflow");
            files+=receiver.files;
        }
        if(requireCatalog && (!node.catalogVerified || node.catalog.linkedFiles!=files))
            return QStringLiteral("Every data-host catalog must match the sealed receiver file count");
    }
    if(receivers!=expectedReceivers)
        return QStringLiteral("Native Run final receiver coverage differs from the configured source set");
    return QString();
}
}
