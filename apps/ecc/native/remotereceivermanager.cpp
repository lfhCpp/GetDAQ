#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
RemoteReceiverManager::RemoteReceiverManager(const RemoteReceiverOptions &options) : fImpl(new Impl(options,nullptr)) {}
RemoteReceiverManager::RemoteReceiverManager(const RemoteReceiverOptions &options,std::shared_ptr<const EccSuccessorPlan> successor) : fImpl(new Impl(options,successor)) {}
RemoteReceiverManager::RemoteReceiverManager(const RemoteReceiverOptions &options,const std::string &epoch) : fImpl(new Impl(options,nullptr,epoch)) {}
RemoteReceiverManager::~RemoteReceiverManager() {}
std::string RemoteReceiverManager::BindingSha256() const { return fImpl->binding; }
void RemoteReceiverManager::BindController(const std::string &snapshot,const std::string &path) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote controller binding busy; not queued");
    Digest(snapshot); Path(path); if (path.size()>3300) throw std::invalid_argument("Remote controller root leaves insufficient bounded operation path space");
    const auto scope=ConfigurationSha256("getdaq.data-controller:1\n"+snapshot+'\n'+path);
    if (p.successor) {
        p.successor->Check();
        if (p.successor->SnapshotSha256()!=snapshot || p.successor->ScopeSha256()!=scope || p.successor->AuditRoot()!=path)
            throw std::invalid_argument("Actual parent controller differs from frozen successor plan");
    }
    if (!p.scope.empty()) { if (p.scope!=scope) throw std::runtime_error("Remote manager already bound to a different parent controller"); p.CheckRoot(); return; }
    p.root.reset(new Directory(path)); p.operations.reset(new Directory(path+"/operations"));
    p.snapshotFd.value=::open((path+"/snapshot.json").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
    if (p.snapshotFd.value<0 || ::fstat(p.snapshotFd.value,&p.snapshotIdentity)) throw std::runtime_error("Cannot bind parent controller snapshot");
    p.rootBytes=ReadFile(path+"/snapshot.json",1024*1024);
    if (ConfigurationSha256(p.rootBytes)!=snapshot) throw std::runtime_error("Parent controller snapshot SHA differs");
    p.rootPath=path; p.scope=scope; p.CheckRoot();
    std::lock_guard<std::mutex> lock(p.stateMutex); p.status.controllerScopeSha256=scope;
}
bool RemoteReceiverManager::NeedsReconciliation() const { std::lock_guard<std::mutex> lock(fImpl->stateMutex); return fImpl->status.needsReconciliation || (fImpl->successor && fImpl->successor->Faulted()); }
void RemoteReceiverManager::Validate(EccCommand,EccState,const std::vector<DataSenderOptions> &senders,const std::string &configuration,const std::string &artifact) const {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote receiver check busy; not queued");
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.commandTimeoutMs); p.Refresh(deadline); p.Match(senders,configuration,artifact);
    p.ReadOnly(SupervisorCommand::Validate,deadline); p.configHash=configuration; p.artifactHash=artifact; p.validated=true;
}
void RemoteReceiverManager::VerifyStopped() const {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote receiver stopped check busy; not queued");
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.commandTimeoutMs); p.Refresh(deadline); p.ReadOnly(SupervisorCommand::VerifyStopped,deadline);
}
void RemoteReceiverManager::VerifyReady() const {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote receiver ready check busy; not queued");
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.commandTimeoutMs); p.Refresh(deadline); p.ReadOnly(SupervisorCommand::VerifyReady,deadline);
}
void RemoteReceiverManager::HandoffControl(const std::string &path,const std::string &successorScope,const std::string &successorEpoch) {
    auto &p=*fImpl;
    std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote controller handoff busy; not queued");
    p.Mutation(SupervisorCommand::HandoffControl,path,nullptr,"",0,{},"",successorScope,successorEpoch);
}
std::string RemoteReceiverManager::SuccessorBindingSha256(const std::vector<EccReceiverPlanTarget> &targets) const {
    const auto &p=*fImpl;auto options=p.options;
    if (targets.size()!=options.endpoints.size()) throw std::invalid_argument("Retirement requires every exact data agent");
    for (std::size_t i=0;i<targets.size();++i) {
        const auto &target=targets[i];Digest(target.planSha256);Path(target.planFile);
        if (target.agentId!=options.endpoints[i].agentId || target.planSha256==options.endpoints[i].planSha256)
            throw std::invalid_argument("Retirement requires sorted exact agents and different reviewed plans");
        options.endpoints[i].planSha256=target.planSha256;
    }
    return Binding(options);
}
void RemoteReceiverManager::RetirePlans(const std::string &path,const std::vector<EccReceiverPlanTarget> &targets,
                                      const std::string &scope,const std::string &epoch) {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote plan retirement busy; not queued");
    SuccessorBindingSha256(targets);
    p.Mutation(SupervisorCommand::RetirePlan,path,nullptr,"",0,{},"",scope,epoch,targets);
}
void RemoteReceiverManager::StartReceivers(const std::string &path,const std::vector<DataSenderOptions> &senders) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote receiver start busy; not queued");
    p.Mutation(SupervisorCommand::Start,path,&senders);
}
void RemoteReceiverManager::FinishReceivers(const std::string &path) {
    FinishReceivers(path,{});
}
void RemoteReceiverManager::AbortFailedReceivers(const std::string &path,const std::string &parentFailure) {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if(!operation.owns_lock()) throw std::runtime_error("Remote receiver cleanup busy; not queued");
    p.Mutation(SupervisorCommand::AbortFailedSession,path,nullptr,"",0,{},parentFailure);
}
void RemoteReceiverManager::RetireFailedReceivers(const std::string &path,const std::string &parentFailure) {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if(!operation.owns_lock()) throw std::runtime_error("Remote retirement busy; not queued");
    p.Mutation(SupervisorCommand::RetireFailedSession,path,nullptr,"",0,{},parentFailure);
}
void RemoteReceiverManager::FinishReceivers(const std::string &path,const std::vector<std::string> &unused) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote receiver finish busy; not queued");
    p.Mutation(SupervisorCommand::Finish,path,nullptr,"",0,unused);
}
RemoteReceiverStatus RemoteReceiverManager::Status() const {
    std::lock_guard<std::mutex> lock(fImpl->stateMutex);auto result=fImpl->status;
    if (fImpl->successor && fImpl->successor->Faulted()) {
        result.needsReconciliation=true;result.message="Frozen successor identity changed; reconciliation required";
    }
    return result;
}
std::vector<IceReceiverSessionBinding> RemoteReceiverManager::IceReceiverSessions() const {
    auto &p=*fImpl;std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if(!operation.owns_lock()) throw std::runtime_error("Remote ICE session query busy");
    const auto deadline=Clock::now()+std::chrono::milliseconds(p.options.commandTimeoutMs);p.Refresh(deadline);p.ReadOnly(SupervisorCommand::VerifyReady,deadline);
    std::vector<IceReceiverSessionBinding> bindings;std::set<std::string> ids;
    for(const auto &peer:p.peers) for(const auto &source:peer->plan.options.sources) if(source.inputProtocol=="ICE") {
        const auto found=std::find_if(peer->cycle.receivers.begin(),peer->cycle.receivers.end(),[&](const ManagedReceiverStatus &receiver) {return receiver.id==source.id;});
        if(!peer->hasCycle || found==peer->cycle.receivers.end() || found->reaped || found->sessionId!=peer->cycle.sessionId || !ids.insert(source.id).second)
            throw std::runtime_error("Remote ICE receiver session/source mapping differs");
        IceReceiverSessionBinding binding;binding.senderId=source.id;binding.sessionId=found->sessionId;bindings.push_back(binding);
    }
    return bindings;
}
std::string RemoteReceiverManager::ObserveReceiversJson() const {
    return ObserveRemoteReceiverEndpoints(fImpl->options,Status());
}
std::string RemoteReceiverManager::ObserveAcquisitionEventsJson(const std::string &senderId,bool fileBytes) const {
    const auto &p=*fImpl;
    SupervisorIO::Text(senderId,128);
    if (!p.validated) throw std::runtime_error("Receiver event query requires the validated fixed data binding");
    const Impl::Peer *target=nullptr; std::size_t sourceIndex=0;
    for (const auto &peer:p.peers) {
        if (!peer->hasPlan) throw std::runtime_error("Receiver event query lacks a validated remote plan");
        for (std::size_t i=0;i<peer->plan.options.sources.size();++i) if (peer->plan.options.sources[i].id==senderId) {
            if (target) throw std::runtime_error("Receiver event source maps to multiple remote agents");
            target=peer.get(); sourceIndex=i;
        }
    }
    if (!target) throw std::out_of_range("Receiver event source is outside the fixed ECC data binding");
    if (!target->hasCycle || target->observed.state!=DataSupervisorStatus::Ready || target->observed.needsReconciliation ||
        target->cycle.sessionId.empty() || target->cycle.receivers.size()!=target->plan.options.sources.size())
        throw std::runtime_error("Receiver event query requires the original Ready receiver cycle");
    const auto child=std::find_if(target->cycle.receivers.begin(),target->cycle.receivers.end(),[&](const ManagedReceiverStatus &value) {return value.id==senderId;});
    if (child==target->cycle.receivers.end() || child->reaped || child->pid<=0 || child->sessionId!=target->cycle.sessionId)
        throw std::runtime_error("Receiver event source lacks its original live PID/session binding");
    return ObserveRemoteAcquisitionEvents(target->endpoint,p.options.controllerId,target->instance,target->endpoint.planSha256,
        senderId,static_cast<unsigned int>(sourceIndex),child->sessionId,child->pid,fileBytes);
}
void RemoteReceiverManager::CatalogReceivers(const std::string &path,const std::string &experiment,unsigned int run,bool replaceCompletedRun) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote catalog busy; not queued");
    if (!replaceCompletedRun) {
        p.Mutation(SupervisorCommand::Catalog,path,nullptr,experiment,run);
        return;
    }
    const auto replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+p.scope+'\n'+p.epoch+'\n'+path+'\n'+experiment+'\n'+std::to_string(run));
    // Complete the prepare request for every node before any node is allowed to publish.
    p.Mutation(SupervisorCommand::Catalog,path+"_prepare",nullptr,experiment,run,{},"","","",{},true,"prepare",replacementId);
    p.Mutation(SupervisorCommand::Catalog,path+"_publish",nullptr,experiment,run,{},"","","",{},true,"publish",replacementId);
    p.Mutation(SupervisorCommand::Catalog,path+"_finalize",nullptr,experiment,run,{},"","","",{},true,"finalize",replacementId);
}
void RemoteReceiverManager::PrepareCatalogReplacement(const std::string &path,const std::string &experiment,unsigned int run) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote catalog prepare busy; not queued");
    const auto replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+p.scope+'\n'+p.epoch+'\n'+path+'\n'+experiment+'\n'+std::to_string(run));
    p.Mutation(SupervisorCommand::Catalog,path+"_prepare",nullptr,experiment,run,{},"","","",{},true,"prepare",replacementId);
}
void RemoteReceiverManager::PublishCatalogReplacement(const std::string &path,const std::string &experiment,unsigned int run) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote catalog publish busy; not queued");
    const auto replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+p.scope+'\n'+p.epoch+'\n'+path+'\n'+experiment+'\n'+std::to_string(run));
    p.Mutation(SupervisorCommand::Catalog,path+"_publish",nullptr,experiment,run,{},"","","",{},true,"publish",replacementId);
}
void RemoteReceiverManager::FinalizeCatalogReplacement(const std::string &path,const std::string &experiment,unsigned int run) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote catalog finalization busy; not queued");
    const auto replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+p.scope+'\n'+p.epoch+'\n'+path+'\n'+experiment+'\n'+std::to_string(run));
    p.Mutation(SupervisorCommand::Catalog,path+"_finalize",nullptr,experiment,run,{},"","","",{},true,"finalize",replacementId);
}
void RemoteReceiverManager::ResumeCatalogReplacement(const std::string &path,const std::string &originalPublishPath,const std::string &experiment,unsigned int run) {
    auto &p=*fImpl; std::unique_lock<std::mutex> operation(p.operationMutex,std::try_to_lock);
    if (!operation.owns_lock()) throw std::runtime_error("Remote catalog resume busy; not queued");
    p.ResumeCatalogReplacement(path,originalPublishPath,experiment,run);
}
bool RemoteReceiverManager::HasRetainedCatalogPublishAudit(const std::string &originalPublishPath) const {
    auto &p=*fImpl;
    try {
        p.CheckRoot();Path(originalPublishPath);
        {
            std::lock_guard<std::mutex> state(p.stateMutex);
            if (p.scope.empty() || !p.status.needsReconciliation) return false;
        }
        if (originalPublishPath.size()<=8 || originalPublishPath.compare(originalPublishPath.size()-8,8,"_publish")!=0) return false;
        for (const auto &proof:p.proofs) if (proof->path==originalPublishPath) { proof->Check();return true; }
    } catch (...) { return false; }
    return false;
}
bool RemoteReceiverManager::CanCatalog() const {
    std::lock_guard<std::mutex> lock(fImpl->stateMutex); const auto &status=fImpl->status;
    if (status.needsReconciliation || status.nodes.empty() || (fImpl->successor && fImpl->successor->Faulted())) return false;
    for (const auto &node:status.nodes) if (!node.catalogEnabled || !node.catalogJson.empty() || node.data.state!=DataSupervisorStatus::Stopped || node.data.needsReconciliation || node.data.receivers.empty()) return false;
    return true;
}
}
