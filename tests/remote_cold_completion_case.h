#ifndef GETDAQ_REMOTE_COLD_COMPLETION_CASE_H
#define GETDAQ_REMOTE_COLD_COMPLETION_CASE_H
// Original normal receiver cycle exists; native fixture has no hardware Run.
{
    const auto originalPath=o.auditRoot+"/operations/manual/"+(coldRetirement ? "retire-plans" : "control-handoff");
    const std::string command=coldRetirement ? "retire-plan" : "handoff-control";
    const std::size_t missing=encodingMinor==0 ? 1 : 0,accepted=1-missing;
    const std::string successorScope(64,'e'),successorEpoch(64,'f');
    std::vector<EccReceiverPlanTarget> targets;std::vector<std::string> reviewed;
    const auto executableSha=ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024));
    for(std::size_t i=0;coldRetirement && i<plans.size();++i) {
        auto changed=plans[i];changed.auditRoot=root+"/next-data-"+std::to_string(i);changed.deploymentId="reviewed-cold-successor-"+std::to_string(i);
        const auto bytes=IO::EncodeOptions(changed,executableSha),file=root+"/next-plan-"+std::to_string(i)+".json";
        IO::WriteNew(file,bytes);Require(!::chmod(file.c_str(),0400),"Seal reviewed cold target");
        EccReceiverPlanTarget target;target.agentId=remote.endpoints[i].agentId;target.planFile=file;target.planSha256=ConfigurationSha256(bytes);
        targets.push_back(target);reviewed.push_back(bytes);
    }
    if(coldRetirement) {
        Require(!::unlink(targets[missing].planFile.c_str()),"Remove owned missing cold target");
        Rejects([&]{data.RetirePlans(originalPath,targets,successorScope,successorEpoch);},"Remote supervisor HTTP");
    }else Rejects([&]{data.HandoffControl(originalPath,successorScope,successorEpoch);},"Incomplete supervisor HTTP");
    Require(services[accepted]->Commands(command)==1 && services[missing]->Commands(command)==(coldRetirement ? 0 : 1),"Original partial cold handoff differs");
    const auto nativePath=o.auditRoot+"/ecc.jsonl",nativeBytes=IO::ReadFile(nativePath,1024*1024);
    const auto originalIntent=IO::ReadFile(originalPath+"/intent.json",1024*1024);
    const auto originalJournal=IO::ReadFile(originalPath+"/remote.jsonl",1024*1024);
    const auto originalContext=IO::ReadFile(originalPath+"/handoff-context.json",1024*1024);
    coldBinding=data.BindingSha256();const auto originalScope=data.Status().controllerScopeSha256,originalEpoch=data.Status().controllerEpoch;
    const auto actions=r.w.actions.size();unsigned int boundaryChecks=0;
    const auto boundary=[&]{++boundaryChecks;Require(r.w.actions.size()==actions,"Cold data completion wrote hardware");};
    Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-while-live",boundary);},"locked");
    Require(::access((o.auditRoot+"/operations/cold-while-live").c_str(),F_OK)!=0,"Live-controller rejection wrote a cold intent");
    eccOwner.reset();dataOwner.reset();
    Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-empty-boundary",{});},"requires retained");
    Require(::access((o.auditRoot+"/operations/cold-empty-boundary").c_str(),F_OK)!=0,"Missing boundary created a cold attempt");
    const auto artifactPath=o.auditRoot+"/experiment.json",artifactBytes=IO::ReadFile(artifactPath,1024*1024);
    {std::ofstream changed(artifactPath,std::ios::binary|std::ios::trunc);changed<<"{}\n";changed.close();Require(bool(changed),"Inject owned artifact drift");}
    Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-artifact-drift",boundary);},"experiment artifact differs");
    {std::ofstream restored(artifactPath,std::ios::binary|std::ios::trunc);restored<<artifactBytes;restored.close();Require(bool(restored),"Restore owned artifact");}
    Require(::access((o.auditRoot+"/operations/cold-artifact-drift").c_str(),F_OK)!=0,"Changed artifact created a cold attempt");
    if(coldRetirement) {
        Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-missing-plan",boundary);},"Remote supervisor HTTP");
        Require(services[accepted]->Commands(command)==1 && services[missing]->Commands(command)==0,"Cold failure replayed accepted sibling");
        IO::WriteNew(targets[missing].planFile,reviewed[missing]);Require(!::chmod(targets[missing].planFile.c_str(),0400),"Restore exact original cold plan");
        relays[accepted]->corruptStatusSession=true;
        Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-current-cycle",boundary);},"cycle changed");
        Require(services[accepted]->Commands(command)==1 && services[missing]->Commands(command)==0,
            "An earlier missing node was submitted before verifying the later accepted node cycle");
        relays[accepted]->corruptStatusSession=false;
    }else {
        relays.front()->hideOperations=true;
        Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-ambiguous-404",boundary);},"delivery was uncertain");
        for(const auto &service:services)Require(service->Commands(command)==1,"Cold ambiguous 404 caused another POST");
        relays.front()->hideOperations=false;
    }
    const auto receipt=RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-complete",boundary);
    const auto value=ParseStrictJson(receipt);
    Require(value.At("kind").text==(coldRetirement ? "getdaq.remote-plan-retirement" : "getdaq.remote-control-handoff") && value.At("original_scope_sha256").text==originalScope &&
        value.At("original_epoch").text==originalEpoch && value.At("successor_scope_sha256").text==successorScope &&
        value.At("successor_epoch").text==successorEpoch && value.At("audit_directory").text==originalPath,
        "Cold completion changed original controller/target");
    Require(value.At("nodes").items.size()==2,"Cold receipt lost an original data node");
    for(std::size_t i=0;i<value.At("nodes").items.size();++i) {
        const auto request=DecodeSupervisorControlRequest(value.At("nodes").items[i].At("request").text);
        Require(request.parentAuditReference==originalPath && (coldRetirement ? request.command==SupervisorCommand::RetirePlan &&
            request.successorPlanSha256==targets[i].planSha256 && request.successorPlanFile==targets[i].planFile : request.command==SupervisorCommand::HandoffControl),
            "Cold completion changed original child request");
        Require(services[i]->Commands(command)==1,"Cold child accepted more than once");
    }
    Require(IO::ReadFile(nativePath,1024*1024)==nativeBytes && IO::ReadFile(originalPath+"/intent.json",1024*1024)==originalIntent &&
        IO::ReadFile(originalPath+"/remote.jsonl",1024*1024)==originalJournal && IO::ReadFile(originalPath+"/handoff-context.json",1024*1024)==originalContext,
        "Cold completion changed historical native/remote evidence");
    Rejects([&]{RemoteReceiverManager::CompleteColdHandoff(remote,o,originalPath,"cold-complete",boundary);},"exclusively create");
    RemoteHandoffEvidence after(originalPath,coldBinding,originalScope);
    Require(after.Nodes()[0].rejection==0 && after.Nodes()[1].rejection==0,"Cold result was not durably classified as accepted");
    for(const auto &entry:oldFiles)Require(ConfigurationSha256(IO::ReadFile(entry.first,1024*1024))==entry.second,"Cold completion changed old raw");
    Require(boundaryChecks>10,"Fixed-input/successor boundary was not rechecked through cold delivery");
    IO::WriteNew(root+"/control-handoff-result.json",receipt);
    IO::WriteNew(root+"/handoff-proof.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(encodingMinor)+
        ",\"reply_loss\":"+std::string(coldRetirement ? "false" : "true")+",\"cold_missing_agent\":\"node-"+std::to_string(missing)+"\",\"original_manager_fenced\":true,\"old_raw_retained\":true,\"raw_forwarded\":false,\"cold_data_completion\":true,\"original_objects_destroyed\":true,\"native_journal_unchanged\":true,\"production_cli_integrated\":false}\n");
    coldCompleted=true;
}
#endif
