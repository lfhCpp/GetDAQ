#include "nativerecoverysnapshotpolicy.h"

namespace GetDAQ {
bool MatchesNativeRecoverySnapshot(const RunConfig &saved, const RunConfig &current) {
    if (!saved.nativeEcc || !current.nativeEcc ||
        saved.nativeCatalog!=current.nativeCatalog ||
        saved.experiment!=current.experiment || saved.runNumber!=current.runNumber ||
        saved.nodes.size()!=current.nodes.size() ||
        saved.outputDir!=current.outputDir ||
        saved.nativeRecordDir!=current.nativeRecordDir) return false;

    const EccSystemClientOptions &a=saved.nativeClient;
    const EccSystemClientOptions &b=current.nativeClient;
    if (a.address!=b.address || a.port!=b.port || a.serviceId!=b.serviceId ||
        a.clientId!=b.clientId || a.clientEpoch!=b.clientEpoch ||
        a.snapshotSha256!=b.snapshotSha256 || a.inputBindingSha256!=b.inputBindingSha256 ||
        a.configurationStatusEnabled!=b.configurationStatusEnabled) return false;

    for (int i=0;i<saved.nodes.size();++i) {
        const NodeConfig &oldNode=saved.nodes[i];
        const NodeConfig &newNode=current.nodes[i];
        if (oldNode.id!=newNode.id || oldNode.mutant!=newNode.mutant ||
            oldNode.nativeSenderId!=newNode.nativeSenderId ||
            oldNode.nativeAgentId!=newNode.nativeAgentId) return false;
    }
    return true;
}
}
