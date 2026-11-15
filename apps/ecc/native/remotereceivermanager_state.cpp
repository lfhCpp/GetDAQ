#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

RemoteReceiverManager::Impl::Impl(RemoteReceiverOptions o,std::shared_ptr<const EccSuccessorPlan> next,const std::string &originalEpoch)
    : options(CheckedOptions(o)),successor(next),binding(Binding(options)),
      epoch(next ? next->Epoch() : originalEpoch.empty() ? RandomSession() : originalEpoch),snapshotFd(-1) {
    if (successor) {
        successor->Check();
        if (!successor->Activated() || successor->ControllerId()!=options.controllerId || successor->DataBindingSha256()!=binding)
            throw std::invalid_argument("Successor identity is not activated or does not match data configuration");
    }
    status.controllerEpoch=epoch;
    for (const auto &endpoint:options.endpoints) {
        peers.emplace_back(new Peer(endpoint));
        // Identity先发布实例，Observe随后发布生命周期；两者之间也必须保留已绑定计划。
        peers.back()->observed.planSha256=endpoint.planSha256;
        RemoteReceiverNodeStatus node; node.agentId=endpoint.agentId; node.planSha256=endpoint.planSha256;
        node.data.planSha256=endpoint.planSha256; status.nodes.push_back(node);
    }
}

void RemoteReceiverManager::Impl::Latch(const std::string &message) {
    if (failedAbortComplete) retirementBlocked=true;
    std::lock_guard<std::mutex> lock(stateMutex); status.needsReconciliation=true; status.message=message;
}

void RemoteReceiverManager::Impl::Gate(bool allowPendingHandoff,bool allowCatalogRecovery) const {
    if (successor) successor->Check();
    std::lock_guard<std::mutex> lock(stateMutex);
    if (scope.empty()) throw std::runtime_error("Remote receiver manager lacks bound parent controller");
    if (successor && predecessorHandoffJson.empty() && !allowPendingHandoff)
        throw std::runtime_error("Planned successor requires the original complete handoff receipt before remote queries/commands");
    if (status.needsReconciliation && !allowCatalogRecovery) throw std::runtime_error("Remote receiver outcome UNKNOWN/reconciliation required; no retry/takeover");
}

void RemoteReceiverManager::Impl::CheckRoot() {
    if(bindingFault) throw std::runtime_error("Remote parent storage fault remains latched; no cleanup or replay");
    try {
        if(coldBoundary)coldBoundary();
        if(coldEvidence)coldEvidence->Check();
        root->Check(); operations->Check();
        if(coldCatalogEvidence)coldCatalogEvidence->Check();
        if (!predecessorHandoffJson.empty())
            BoundFile(rootPath+"/controller-handoff.json",predecessorHandoffIdentity,predecessorHandoffJson,1024*1024);
        BoundFile(rootPath+"/snapshot.json",snapshotIdentity,rootBytes,1024*1024);
        for (const auto &proof:proofs) proof->Check();
    }
    catch (...) { bindingFault=true;Latch("Parent controller root/snapshot changed; restoration cannot clear gate"); throw; }
}

void RemoteReceiverManager::Impl::Publish(std::size_t index) {
    std::lock_guard<std::mutex> lock(stateMutex);
    status.nodes[index].instanceId=peers[index]->instance;
    status.nodes[index].data=peers[index]->observed;
    status.nodes[index].catalogJson=peers[index]->catalogJson;
    status.nodes[index].catalogEnabled=peers[index]->hasPlan && !peers[index]->plan.options.catalogRoot.empty();
}

void RemoteReceiverManager::Impl::AuditProof::Check() const {
    directory->Check(); BoundFile(path+"/intent.json",intentIdentity,intent,1024*1024);
    BoundFile(path+"/remote.jsonl",journalIdentity,journal,1024*1024);
    if(!handoffContext.empty())BoundFile(path+"/handoff-context.json",handoffContextIdentity,handoffContext,1024*1024);
}

} // namespace GetDAQ
