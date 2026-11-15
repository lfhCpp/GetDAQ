#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::Impl::ReadOnly(SupervisorCommand command,const Clock::time_point &deadline){
        const auto reference="getdaq.receiver-check:1:"+scope+':'+SupervisorCommandName(command);
        for (std::size_t i=0;i<peers.size();++i) {
            auto &peer=*peers[i];
            if (command==SupervisorCommand::VerifyReady) Ready(peer,peer.observed,false);
            if (command==SupervisorCommand::VerifyStopped) Stopped(peer,peer.observed);
            try {
                const auto result=Execute(i,Request(i,command,reference,"check-"+RandomSession()),deadline);
                if (command==SupervisorCommand::VerifyReady) Ready(peer,result.data,false);
                if (command==SupervisorCommand::VerifyStopped) Stopped(peer,result.data);
            } catch (...) {
                if (peer.hasCycle) Latch("Bound remote cycle validation failed/uncertain; retain confirmation gate");
                throw;
            }
        }
    }

void RemoteReceiverManager::Impl::Mutation(SupervisorCommand command,const std::string &path,const std::vector<DataSenderOptions> *senders,const std::string &experiment,unsigned int run,const std::vector<std::string> &unusedSourceIds,const std::string &parentFailure,
                  const std::string &successorScope,const std::string &successorEpoch,const std::vector<EccReceiverPlanTarget> &targets,bool replaceCompletedRun,
                  const std::string &catalogPhase,const std::string &replacementId,bool allowCatalogRecovery) {
        if (allowCatalogRecovery && (command!=SupervisorCommand::Catalog ||
            (catalogPhase!="publish" && catalogPhase!="finalize") || !replaceCompletedRun))
            throw std::invalid_argument("Catalog recovery privilege is restricted to explicitly authorized Publish/Finalize");
        const bool changedPlans=command==SupervisorCommand::RetirePlan;
        const bool handoff=command==SupervisorCommand::HandoffControl || changedPlans;
        if (handoff) {
            Digest(successorScope); Digest(successorEpoch);
            if (handoffAttempted || (successorScope==scope && successorEpoch==epoch))
                throw std::runtime_error("Control handoff requires a new explicit successor and no prior attempt");
        }
        const bool cleanup=command==SupervisorCommand::AbortFailedSession;
        const bool retirement=command==SupervisorCommand::RetireFailedSession;
        if(!cleanup && !retirement) Gate(false,allowCatalogRecovery);else {
            SupervisorOperationId(parentFailure);
            if(scope.empty() || !root || !operations) throw std::runtime_error("Cleanup lacks retained parent controller binding");
            if(cleanup && failedAbortAttempted) throw std::runtime_error("Remote cleanup already attempted; no replay");
            if(retirement && (!failedAbortComplete || retirementAttempted || retirementBlocked || parentFailure!=failedAbortParent))
                throw std::runtime_error("Remote retirement requires the original completed abort with no independent hold or prior attempt");
        }
        CheckRoot(); Path(path);
        const auto prefix=rootPath+"/operations/";
        if (path.compare(0,prefix.size(),prefix)) throw std::invalid_argument("Remote operation audit path is outside bound parent controller");
        const auto slash=path.find('/',prefix.size());
        if (slash==std::string::npos || path.find('/',slash+1)!=std::string::npos) throw std::invalid_argument("Remote operation audit path requires parent operation and action directory");
        SupervisorOperationId(path.substr(prefix.size(),slash-prefix.size()));
        if (!validated) throw std::runtime_error("Remote receiver plans not validated against parent sender/configuration set");
        const auto deadline=Clock::now()+std::chrono::milliseconds(options.commandTimeoutMs);
        std::vector<bool> alreadySealed(peers.size(),false);
        if(cleanup || retirement) {
            for(std::size_t i=0;i<peers.size();++i) {
                if(!peers[i]->hasCycle) throw std::runtime_error("Cleanup lacks retained confirmed receiver cycle; no startup adoption");
                Identity(i,deadline);Observe(i,deadline,true);SameCycle(*peers[i],peers[i]->observed,false);
                if(peers[i]->observed.state==DataSupervisorStatus::Stopped && !peers[i]->observed.needsReconciliation) {
                    SameCycle(*peers[i],peers[i]->observed,true);alreadySealed[i]=true;
                } else if(retirement && (peers[i]->observed.state!=DataSupervisorStatus::Aborted ||
                    !peers[i]->observed.needsReconciliation || !std::all_of(peers[i]->observed.receivers.begin(),peers[i]->observed.receivers.end(),
                    [](const ManagedReceiverStatus &r) {return r.reaped;}))) {
                    throw std::runtime_error("Remote retirement lacks the original aborted/reaped session");
                }
            }
        } else Refresh(deadline,allowCatalogRecovery);
        if (senders) Match(*senders,configHash,artifactHash);
        for (const auto &peer:peers) {
            if(cleanup || retirement) continue;
            if (handoff) {
                Stopped(*peer,peer->observed);
                if (!peer->hasCycle || peer->retiredCycle || peer->observed.state!=DataSupervisorStatus::Stopped)
                    throw std::runtime_error("Control handoff requires every original normal sealed receiver cycle");
            }
            else if (command==SupervisorCommand::Start) Stopped(*peer,peer->observed);
            else if (command==SupervisorCommand::Catalog) {
                if (allowCatalogRecovery && peer->observed.state==DataSupervisorStatus::Failed && peer->observed.needsReconciliation) {
                    // A failed Publish may be resumed only against the exact retained,
                    // already sealed receiver cycle. A stopped node is also valid: it
                    // may have completed the original Publish or never received it.
                    SameCycle(*peer,peer->observed,true);
                } else Stopped(*peer,peer->observed);
                if (!peer->hasCycle || peer->plan.options.catalogRoot.empty()) throw std::runtime_error("Remote catalog requires all current sealed cycles and enabled fixed roots");
            }
            else { if (peer->observed.state!=DataSupervisorStatus::Ready) throw std::runtime_error("Remote Finish lacks confirmed ready cycle"); SameCycle(*peer,peer->observed,false); }
        }
        std::set<std::string> unused(unusedSourceIds.begin(),unusedSourceIds.end()), remaining=unused;
        if ((!unused.empty() && command!=SupervisorCommand::Finish) || unused.size()!=unusedSourceIds.size()) throw std::invalid_argument("Invalid unused-source Finish context");
        for (const auto &peer:peers) for (const auto &source:peer->plan.options.sources) if (unused.count(source.id)) {
            if (source.inputProtocol!="FDT") throw std::invalid_argument("Unused-source declaration requires FDT plan");
            remaining.erase(source.id);
        }
        if (!remaining.empty()) throw std::invalid_argument("Unused source absent from bound remote plans");
        CreateDirectory(path);
        std::vector<SupervisorControlRequest> requests;
        for (std::size_t i=0;i<peers.size();++i) {
            const auto id=ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+path+'\n'+SupervisorCommandName(command)+'\n'+peers[i]->endpoint.agentId);
            if(cleanup || retirement) {
                auto request=Request(i,SupervisorCommand::Validate,path,id);
                request.command=command;request.expectedSessionId=peers[i]->cycle.sessionId;request.parentFailureOperationId=parentFailure;
                ValidateSupervisorControlRequest(request);requests.push_back(request);
            } else if (handoff) {
                auto request=Request(i,SupervisorCommand::Validate,path,id);
                request.command=command;request.successorControllerSnapshotSha256=successorScope;request.successorControllerEpoch=successorEpoch;
                if (changedPlans) {
                    request.expectedSessionId=peers[i]->cycle.sessionId;
                    request.successorPlanSha256=targets[i].planSha256;request.successorPlanFile=targets[i].planFile;
                }
                ValidateSupervisorControlRequest(request);requests.push_back(request);
            } else requests.push_back(Request(i,command,path,id,experiment,run,replaceCompletedRun,catalogPhase,replacementId));
            for (const auto &source:peers[i]->plan.options.sources) if (unused.count(source.id)) requests.back().unusedSourceIds.push_back(source.id);
            std::sort(requests.back().unusedSourceIds.begin(),requests.back().unusedSourceIds.end());
        }
        proofs.push_back(PersistMutationIntent(path,handoff,requests));
        auto &proof=*proofs.back();
        ExecuteMutation(command,path,experiment,run,unused,requests,deadline,handoff,cleanup,retirement,
            changedPlans,parentFailure,successorScope,successorEpoch,alreadySealed,proof);
    }

} // namespace GetDAQ
