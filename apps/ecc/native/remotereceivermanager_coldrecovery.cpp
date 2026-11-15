#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

std::string RemoteReceiverManager::CompleteColdHandoff(const RemoteReceiverOptions &options,
    const NativeEccOptions &controller,const std::string &originalPath,const std::string &operationId,
    const std::function<void()> &boundary) {
    if(!boundary)throw std::invalid_argument("Cold data handoff requires retained fixed-input/successor checks");
    boundary();SupervisorOperationId(operationId);Path(originalPath);
    const auto plan=PlanNativeEccConfiguration(controller,RemoteReceiverBindingSha256(options));
    const auto operationPath=controller.auditRoot+"/operations/"+operationId;
    if(originalPath.compare(0,controller.auditRoot.size()+12,controller.auditRoot+"/operations/"))
        throw std::invalid_argument("Cold handoff evidence is outside the original controller");
    const auto nativePath=controller.auditRoot+"/ecc.jsonl";
    const auto nativeBytes=ReadFile(nativePath,64*1024*1024);struct stat nativeIdentity;
    if(::lstat(nativePath.c_str(),&nativeIdentity) || nativeIdentity.st_uid!=::geteuid() ||
        (nativeIdentity.st_mode&0777)!=0600 || nativeIdentity.st_nlink!=1)
        throw std::runtime_error("Cold handoff native journal identity/permissions differ");
    const auto artifactPath=controller.auditRoot+"/experiment.json";struct stat artifactIdentity;
    if(::lstat(artifactPath.c_str(),&artifactIdentity) || !S_ISREG(artifactIdentity.st_mode) ||
        artifactIdentity.st_uid!=::geteuid() || (artifactIdentity.st_mode&0777)!=0600 || artifactIdentity.st_nlink!=1 ||
        ReadFile(artifactPath,1024*1024)!=controller.experimentArtifactBytes)
        throw std::runtime_error("Cold handoff original experiment artifact differs");
    // Bind the bytes before parsing, then recheck after acquiring the lock:
    // never accept a replacement journal as if it were the one just parsed.
    EccJournal journal(nativePath,"getdaq.native-ecc:1:"+plan.snapshotSha256,EccJournalMode::ExclusiveReadOnly);
    BoundFile(nativePath,nativeIdentity,nativeBytes,64*1024*1024);
    const auto status=journal.Load();
    if((status.state!=EccState::Idle && status.state!=EccState::Described) || status.transition || status.error || status.outcomeUnknown)
        throw std::runtime_error("Cold data handoff requires historically released native control; hardware UNKNOWN is not recovered");
    const auto check=[&] {
        boundary();BoundFile(nativePath,nativeIdentity,nativeBytes,64*1024*1024);
        BoundFile(artifactPath,artifactIdentity,controller.experimentArtifactBytes,1024*1024);
    };
    check();
    RemoteHandoffEvidence original(originalPath,RemoteReceiverBindingSha256(options),plan.dataControllerScopeSha256);
    const auto nodes=original.Nodes();const auto epoch=original.Epoch();
    auto ordered=CheckedOptions(options);
    if(nodes.size()!=ordered.endpoints.size())throw std::runtime_error("Cold handoff lacks exact original node coverage");
    for(std::size_t i=0;i<nodes.size();++i) {
        if(nodes[i].agentId!=ordered.endpoints[i].agentId || nodes[i].planSha256!=ordered.endpoints[i].planSha256 ||
            nodes[i].request.controllerId!=ordered.controllerId)
            throw std::runtime_error("Cold handoff original data target differs");
    }
    // Create the empty destination before binding the directory set. Its files
    // are subsequently guarded by AuditProof; old attempts stay immutable.
    check();original.Check();CreateDirectory(operationPath);CreateDirectory(operationPath+"/data-handoff");
    RemoteReceiverManager temporary(ordered,epoch);auto &p=*temporary.fImpl;
    p.coldBoundary=check;
    temporary.BindController(plan.snapshotSha256,controller.auditRoot);
    p.coldEvidence.reset(new RemoteHandoffEvidence(originalPath,p.binding,p.scope));
    for(std::size_t i=0;i<nodes.size();++i) {
        auto &peer=*p.peers[i];const auto &node=nodes[i];
        peer.instance=node.instanceId;peer.cycle=node.stopped;peer.observed=node.stopped;
        peer.hasCycle=true;peer.retiredCycle=false;p.Publish(i);
        p.handoffRequests.push_back(node.request);p.handoffRejections.push_back(node.rejection);
    }
    std::unique_ptr<Impl::AuditProof> retained(new Impl::AuditProof);
    retained->path=originalPath;retained->directory.reset(new Directory(originalPath));
    retained->intent=ReadFile(originalPath+"/intent.json",1024*1024);
    retained->journal=ReadFile(originalPath+"/remote.jsonl",1024*1024);
    retained->handoffContext=ReadFile(originalPath+"/handoff-context.json",1024*1024);
    if(::lstat((originalPath+"/intent.json").c_str(),&retained->intentIdentity) ||
       ::lstat((originalPath+"/remote.jsonl").c_str(),&retained->journalIdentity) ||
       ::lstat((originalPath+"/handoff-context.json").c_str(),&retained->handoffContextIdentity))
        throw std::runtime_error("Cannot bind original cold handoff files");
    p.proofs.push_back(std::move(retained));p.handoffProof=p.proofs.back().get();
    p.handoffAttempted=true;p.coldAuditPath=operationPath+"/data-handoff";
    p.Latch("Cold data completion only; original control remains permanently fenced");
    p.CheckRoot();temporary.CompletePendingHandoff(p.coldAuditPath);check();
    return temporary.CompletedControlHandoffJson();
}
RemoteReceiverStatus RemoteReceiverManager::ResumeColdCatalogReplacement(const RemoteReceiverOptions &options,
    const NativeEccOptions &controller,const std::string &originalPath,const std::string &operationId,
    const std::string &experiment,unsigned int run,const std::function<void()> &boundary) {
    if(!boundary)throw std::invalid_argument("Cold catalog recovery requires current Prepared hardware/hold checks");
    boundary();SupervisorOperationId(operationId);Path(originalPath);Text(experiment,128);
    const auto ordered=CheckedOptions(options);
    const auto binding=RemoteReceiverBindingSha256(ordered);
    const auto plan=PlanNativeEccConfiguration(controller,binding);
    const auto operationsPath=controller.auditRoot+"/operations/";
    if(originalPath.compare(0,operationsPath.size(),operationsPath) ||
       originalPath.size()<=std::string("_publish").size() ||
       originalPath.compare(originalPath.size()-std::string("_publish").size(),std::string("_publish").size(),"_publish"))
        throw std::invalid_argument("Cold catalog Publish evidence is outside the fixed controller operation root");
    const auto operationPath=controller.auditRoot+"/operations/"+operationId;
    const auto resumePath=operationPath+"/catalog-resume";
    if(originalPath==resumePath || originalPath.compare(0,operationPath.size()+1,operationPath+"/")==0)
        throw std::invalid_argument("Cold catalog recovery requires a distinct operation from its original Publish");
    if(resumePath.size()>3300)throw std::invalid_argument("Cold catalog recovery path exceeds the bounded controller path budget");

    const auto artifactPath=controller.auditRoot+"/experiment.json";
    struct stat artifactIdentity;
    if(::lstat(artifactPath.c_str(),&artifactIdentity) || !S_ISREG(artifactIdentity.st_mode) ||
       artifactIdentity.st_uid!=::geteuid() || (artifactIdentity.st_mode&0777)!=0600 || artifactIdentity.st_nlink!=1 ||
       ReadFile(artifactPath,1024*1024)!=controller.experimentArtifactBytes)
        throw std::runtime_error("Cold catalog controller experiment artifact differs from the fixed input");
    const auto artifact=DecodeSystemExperimentArtifact(controller.experimentArtifactBytes);
    const auto recheck=[&] {
        boundary();
        if(RemoteReceiverBindingSha256(options)!=binding ||
           PlanNativeEccConfiguration(controller,binding).snapshotSha256!=plan.snapshotSha256)
            throw std::runtime_error("Cold catalog fixed controller/data inputs changed during recovery");
        BoundFile(artifactPath,artifactIdentity,controller.experimentArtifactBytes,1024*1024);
    };
    recheck();

    std::vector<std::string> fixedAgentIds;
    for(const auto &endpoint:ordered.endpoints)fixedAgentIds.push_back(endpoint.agentId);
    RemoteCatalogPublishEvidence original(originalPath,binding,plan.dataControllerScopeSha256,fixedAgentIds);
    if(original.Experiment()!=experiment || original.RunNumber()!=run || original.Nodes().size()!=ordered.endpoints.size())
        throw std::runtime_error("Cold catalog Publish evidence differs from the requested experiment/Run/fixed nodes");
    const auto originalEpoch=original.Epoch();
    original.Check();
    // Reconstruct with the epoch bound by the immutable original Publish
    // intent; this temporary instance never escapes and cannot guess/take over
    // a different remote owner.
    RemoteReceiverManager recovered(ordered,originalEpoch);
    auto &restored=*recovered.fImpl;
    recovered.BindController(plan.snapshotSha256,controller.auditRoot);
    restored.coldCatalogEvidence.reset(new RemoteCatalogPublishEvidence(originalPath,binding,plan.dataControllerScopeSha256,
        fixedAgentIds));
    auto &evidence=*restored.coldCatalogEvidence;
    const auto check=[&] {recheck();evidence.Check();};
    restored.coldBoundary=check;
    if(evidence.IntentSha256()!=original.IntentSha256() || evidence.Epoch()!=originalEpoch ||
       evidence.ReplacementId()!=original.ReplacementId())
        throw std::runtime_error("Cold catalog Publish evidence changed while rebuilding its fixed owner");

    std::unique_ptr<Impl::AuditProof> proof(new Impl::AuditProof);
    proof->path=originalPath;proof->directory.reset(new Directory(originalPath));
    proof->intent=ReadFile(originalPath+"/intent.json",1024*1024);
    proof->journal=ReadFile(originalPath+"/remote.jsonl",1024*1024);
    if(::lstat((originalPath+"/intent.json").c_str(),&proof->intentIdentity) ||
       ::lstat((originalPath+"/remote.jsonl").c_str(),&proof->journalIdentity))
        throw std::runtime_error("Cannot bind original cold Publish audit files");
    if(ConfigurationSha256(proof->intent)!=evidence.IntentSha256())
        throw std::runtime_error("Original cold Publish intent changed during recovery");
    restored.proofs.push_back(std::move(proof));

    const std::string replacementId=evidence.ReplacementId();
    const auto deadline=Clock::now()+std::chrono::milliseconds(ordered.commandTimeoutMs);
    for(std::size_t i=0;i<restored.peers.size();++i) {
        check();restored.Identity(i,deadline);
        auto &peer=*restored.peers[i];const auto &node=evidence.Nodes()[i];const auto &request=node.request;
        const auto expectedOperation=ConfigurationSha256("getdaq.remote-data-operation:1\n"+plan.dataControllerScopeSha256+'\n'+originalEpoch+'\n'+originalPath+"\ncatalog\n"+peer.endpoint.agentId);
        if(node.agentId!=peer.endpoint.agentId || node.planSha256!=peer.endpoint.planSha256 ||
           node.instanceId!=peer.instance || request.command!=SupervisorCommand::Catalog || request.catalogPhase!="publish" ||
           !request.catalogReplaceExisting || request.catalogReplacementId!=ConfigurationSha256("getdaq.remote-catalog-replacement-node:1\n"+replacementId+'\n'+peer.endpoint.agentId) ||
           request.operationId!=expectedOperation || request.instanceId!=peer.instance || request.planSha256!=peer.endpoint.planSha256 ||
           request.configurationSha256!=peer.plan.options.configurationSha256 || request.artifactSha256!=peer.plan.options.artifactSha256 ||
           request.controllerId!=ordered.controllerId || request.controllerSnapshotSha256!=plan.dataControllerScopeSha256 ||
           request.controllerEpoch!=originalEpoch || request.parentAuditReference!=originalPath ||
           request.catalogExperiment!=experiment || request.catalogRunNumber!=run ||
           evidence.Nodes()[i].operationId!=request.operationId || evidence.Nodes()[i].requestKey!=SupervisorControlRequestKey(request))
            throw std::runtime_error("Original Publish request differs from the fixed cold recovery controller/node/Run binding");

        restored.Observe(i,deadline,true);
        // Accept either the exact normal stopped lifecycle or the two-layer
        // failure state produced by this service's still-live catalog recovery
        // gate. A restarted supervisor has a different fixed instance and is
        // rejected above; this never adopts its new process identity.
        const bool stopped=peer.observed.state==DataSupervisorStatus::Stopped &&
            !peer.observed.needsReconciliation && !peer.serviceNeedsReconciliation;
        const bool catalogFailure=peer.observed.state==DataSupervisorStatus::Failed &&
            peer.observed.needsReconciliation && peer.serviceNeedsReconciliation;
        if((!stopped && !catalogFailure) || peer.observed.sessionId.empty() ||
           peer.observed.sessionId!=request.catalogSessionId ||
           peer.observed.receivers.size()!=peer.plan.options.sources.size())
            throw std::runtime_error("Cold catalog recovery requires the exact current normally sealed stopped receiver cycle (state="+
                std::to_string(static_cast<int>(peer.observed.state))+",lifecycle_reconciliation="+
                std::string(peer.observed.needsReconciliation ? "true" : "false")+",session_match="+
                std::string(peer.observed.sessionId==request.catalogSessionId ? "true" : "false")+",receivers="+
                std::to_string(peer.observed.receivers.size())+")");
        for(std::size_t source=0;source<peer.plan.options.sources.size();++source) {
            const auto &receiver=peer.observed.receivers[source];
            if(receiver.id!=peer.plan.options.sources[source].id || receiver.pid<=0 || receiver.sessionId!=peer.observed.sessionId ||
               receiver.outputDirectory.empty() || !receiver.reaped || receiver.exitCode!=0 || !receiver.sealVerified)
                throw std::runtime_error("Cold catalog recovery receiver identity/seal differs from the fixed source plan");
        }
        peer.cycle=peer.observed;peer.hasCycle=true;peer.retiredCycle=false;restored.Publish(i);
    }
    restored.Match(controller.senders,artifact.configurationSha256,artifact.artifactSha256);
    restored.configHash=artifact.configurationSha256;restored.artifactHash=artifact.artifactSha256;restored.validated=true;
    check();CreateDirectory(operationPath);
    restored.Latch("Cold catalog replacement continuation is verifying original remote Publish operations");
    recovered.ResumeCatalogReplacement(resumePath,originalPath,experiment,run);
    check();
    return recovered.Status();
}

} // namespace GetDAQ
