#include "remotereceivermanager_p.h"
#include <stdexcept>

namespace GetDAQ {
using namespace RemoteReceiverManagerInternal;

void RemoteReceiverManager::Impl::ResumeCatalogReplacement(const std::string &newPath,const std::string &originalPath,const std::string &experiment,unsigned int run){
        if (successor) successor->Check();
        CheckRoot(); Path(newPath); Path(originalPath);
        {
            std::lock_guard<std::mutex> stateLock(stateMutex);
            if (scope.empty() || !status.needsReconciliation)
                throw std::runtime_error("Catalog resume requires this manager's latched Publish reconciliation state");
        }
        if (!validated || peers.empty()) throw std::runtime_error("Catalog resume requires the original validated fixed data binding");
        const std::string suffix="_publish";
        if (originalPath.size()<=suffix.size() || originalPath.compare(originalPath.size()-suffix.size(),suffix.size(),suffix)!=0)
            throw std::invalid_argument("Original catalog Publish audit path must end in _publish");
        const std::string basePath=originalPath.substr(0,originalPath.size()-suffix.size());
        if (newPath==originalPath || newPath==basePath || newPath.compare(0,rootPath.size(),rootPath))
            throw std::invalid_argument("Catalog resume requires a distinct audit path inside the bound controller root");
        AuditProof *original=nullptr;
        for (const auto &candidate:proofs) if (candidate->path==originalPath) {
            if (original) throw std::runtime_error("Duplicate original Publish proof in this controller");
            original=candidate.get();
        }
        if (!original) throw std::runtime_error("Original Publish proof is not retained by this manager; cold recovery is not supported");
        original->Check();
        std::vector<std::string> fixedAgents;for(const auto &peer:peers)fixedAgents.push_back(peer->endpoint.agentId);
        RemoteCatalogPublishEvidence persisted(originalPath,binding,scope,fixedAgents);
        if(persisted.Epoch()!=epoch || persisted.Experiment()!=experiment || persisted.RunNumber()!=run ||
            persisted.IntentSha256()!=ConfigurationSha256(original->intent) || persisted.ReplacementId().empty())
            throw std::runtime_error("Persisted Publish evidence differs from the retained controller/run identity");
        const auto intent=ParseStrictJson(original->intent);
        ExactSupervisorFields(intent,{"schema","kind","binding_sha256","requests"});
        if (Unsigned(intent.At("schema"))!=1 || String(intent.At("kind"))!="getdaq.remote-receiver-intent" ||
            String(intent.At("binding_sha256"))!=binding || intent.At("requests").items.size()!=peers.size())
            throw std::runtime_error("Original Publish intent does not match the retained fixed binding");
        const std::string replacementId=ConfigurationSha256("getdaq.remote-catalog-replacement:1\n"+scope+'\n'+epoch+'\n'+basePath+'\n'+experiment+'\n'+std::to_string(run));
        bool publishRecoveryRequired=false;
        std::map<std::string,std::string> nodeEvidence;
        if(persisted.Nodes().size()!=peers.size())throw std::runtime_error("Persisted Publish evidence node count differs from the fixed remote binding");
        for(std::size_t i=0;i<persisted.Nodes().size();++i) {
            const auto &node=persisted.Nodes()[i];const auto &peer=*peers[i];
            if(node.agentId!=peer.endpoint.agentId || node.instanceId!=peer.instance || node.planSha256!=peer.endpoint.planSha256)
                throw std::runtime_error("Persisted Publish node instance/plan differs from the current fixed remote binding");
            nodeEvidence[node.agentId]=node.outcome;
        }

        // Persist an observation-only audit before querying any child operation.
        const std::string observePath=newPath+"_observe";
        CreateDirectory(observePath);
        std::unique_ptr<AuditProof> observation(new AuditProof); observation->path=observePath;
        observation->directory.reset(new Directory(observePath));
        observation->intent="{\"schema\":1,\"kind\":\"getdaq.remote-catalog-resume-observation\",\"binding_sha256\":"+
            EncodeJsonString(binding)+",\"original_publish_path\":"+EncodeJsonString(originalPath)+
            ",\"original_intent_sha256\":"+EncodeJsonString(ConfigurationSha256(original->intent))+
            ",\"resume_path\":"+EncodeJsonString(newPath)+"}\n";
        WriteNew(observePath+"/intent.json",observation->intent);
        if (::lstat((observePath+"/intent.json").c_str(),&observation->intentIdentity)) throw std::runtime_error("Cannot bind catalog resume observation intent");
        observation->audit.reset(new HardwareAudit(observePath+"/remote.jsonl"));
        if (::lstat((observePath+"/remote.jsonl").c_str(),&observation->journalIdentity)) throw std::runtime_error("Cannot bind catalog resume observation journal");
        proofs.push_back(std::move(observation)); auto &audit=*proofs.back();
        const auto record=[&](const std::string &entry) { CheckRoot(); audit.audit->Append(entry); audit.journal+=entry+'\n'; CheckRoot(); };
        record("{\"phase\":\"intent\",\"read_only_remote_observation\":true,\"automatic_retry\":false}");

        const auto deadline=Clock::now()+std::chrono::milliseconds(options.commandTimeoutMs);
        for (std::size_t i=0;i<peers.size();++i) {
            auto &peer=*peers[i]; const auto &requestValue=intent.At("requests").items[i];
            const auto request=DecodeRequestValue(requestValue);
            const auto expectedOperation=ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+originalPath+'\n'+SupervisorCommandName(SupervisorCommand::Catalog)+'\n'+peer.endpoint.agentId);
            if (!peer.hasPlan || !peer.hasCycle || request.command!=SupervisorCommand::Catalog || request.catalogPhase!="publish" ||
                !request.catalogReplaceExisting || request.catalogReplacementId!=ConfigurationSha256("getdaq.remote-catalog-replacement-node:1\n"+replacementId+'\n'+peer.endpoint.agentId) ||
                request.catalogExperiment!=experiment || request.catalogRunNumber!=run || request.catalogSessionId!=peer.cycle.sessionId ||
                request.operationId!=expectedOperation || request.instanceId!=peer.instance || request.planSha256!=peer.endpoint.planSha256 ||
                request.configurationSha256!=peer.plan.options.configurationSha256 || request.artifactSha256!=peer.plan.options.artifactSha256 ||
                request.controllerId!=options.controllerId || request.controllerSnapshotSha256!=scope || request.controllerEpoch!=epoch ||
                request.parentAuditReference!=originalPath)
                throw std::runtime_error("Original Publish request differs from the retained experiment/Run/agent/session binding");
            const auto evidence=nodeEvidence.find(peer.endpoint.agentId);
            if (evidence==nodeEvidence.end()) throw std::runtime_error("Original Publish audit contains an unexpected or missing node");
            Identity(i,deadline);
            const auto prior=peer.client.Operation(request.operationId);
            if (evidence->second=="not_attempted") {
                if (prior.code!=404) throw std::runtime_error("Unattempted Publish operation unexpectedly exists remotely; manual reconciliation required");
                publishRecoveryRequired=true;
                record("{\"phase\":\"resume_node_proven_unattempted\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
                    ",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"remote_operation_absent\":true}");
                continue;
            }
            if (prior.code!=200) throw std::runtime_error("Submitted Publish operation cannot be read by its original ID; no recovery POST is safe");
            auto priorResult=DecodeOperation(ParseStrictJson(prior.body),request);
            while (priorResult.phase=="accepted" || priorResult.phase=="running") {
                Deadline(deadline); std::this_thread::sleep_for(std::chrono::milliseconds(options.pollIntervalMs));
                CheckRoot(); const auto observed=peer.client.Operation(request.operationId);
                if (observed.code!=200) throw std::runtime_error("Submitted Publish operation disappeared during read-only observation");
                priorResult=DecodeOperation(ParseStrictJson(observed.body),request);
            }
            if (priorResult.phase=="unknown") throw std::runtime_error("Original Publish operation remains UNKNOWN; no recovery POST is safe");
            if (priorResult.phase=="complete") {
                if (!priorResult.hasData || priorResult.data.state!=DataSupervisorStatus::Stopped || priorResult.data.needsReconciliation)
                    throw std::runtime_error("Completed original Publish lacks a normal stopped lifecycle receipt");
                SameCycle(peer,priorResult.data,true);
                const auto &plan=peer.plan.options;
                ReceiverCatalogOptions expected; expected.receiver=plan; expected.executableSha256=peer.plan.executableSha256; expected.stopped=priorResult.data;
                expected.operationId=request.catalogReplacementId; expected.experiment=experiment; expected.runNumber=run; expected.stagedForReplacement=true;
                const auto receipt=DecodeEccControlCatalogReceipt(priorResult.catalogJson);
                std::ostringstream number; number<<"run_"<<std::setw(4)<<std::setfill('0')<<run;
                if (receipt.operationId!=expected.operationId || receipt.requestKey!=ReceiverCatalogRequestKey(expected) ||
                    receipt.directory!=plan.catalogRoot+'/'+experiment+'/'+number.str()+'/'+plan.deploymentId)
                    throw std::runtime_error("Completed original Publish receipt differs from the bound catalog destination");
                record("{\"phase\":\"resume_node_already_complete\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
                    ",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"catalog\":"+EmbeddedJsonObject(priorResult.catalogJson)+"}");
            } else if (priorResult.phase=="failed") {
                publishRecoveryRequired=true;
                record("{\"phase\":\"resume_node_terminal_failure\",\"agent_id\":"+EncodeJsonString(peer.endpoint.agentId)+
                    ",\"operation_id\":"+EncodeJsonString(request.operationId)+",\"message\":"+
                    EncodeJsonString(RemoteObservationError(priorResult.message.c_str()))+"}");
            } else throw std::runtime_error("Original Publish operation did not reach a terminal state");
        }
        record("{\"phase\":\"observations_complete\",\"all_submitted_operations_terminal\":true,\"safe_to_resume_same_replacement\":true}");
        if (publishRecoveryRequired)
            Mutation(SupervisorCommand::Catalog,newPath+"_publish",nullptr,experiment,run,{},"","","",{},true,"publish",replacementId,true);
        Mutation(SupervisorCommand::Catalog,newPath+"_finalize",nullptr,experiment,run,{},"","","",{},true,"finalize",replacementId,true);
        CheckRoot();
        std::lock_guard<std::mutex> stateLock(stateMutex);
        status.needsReconciliation=false;
        status.message=publishRecoveryRequired ? "Catalog Publish recovery and old-name Finalize explicitly completed on every fixed node" :
            "Catalog old-name Finalize explicitly resumed on every fixed node";
    }

} // namespace GetDAQ
