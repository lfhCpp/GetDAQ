#ifndef GETDAQ_REMOTE_FAILED_PLAN_COMPLETION_CASE_H
#define GETDAQ_REMOTE_FAILED_PLAN_COMPLETION_CASE_H
// Real HTTP nodes, real receiver cycles, synthetic board servants only.
{
    const auto originalPath=o.auditRoot+"/operations/manual/retire-plans";
    const std::string successorScope(64,'e'),successorEpoch(64,'f');
    std::vector<EccReceiverPlanTarget> targets;
    const auto executableSha=ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024));
    for(std::size_t i=0;i<plans.size();++i) {
        auto changed=plans[i];changed.auditRoot=root+"/next-data-"+std::to_string(i);changed.deploymentId="reviewed-successor-"+std::to_string(i);
        const auto bytes=IO::EncodeOptions(changed,executableSha),file=root+"/next-plan-"+std::to_string(i)+".json";
        IO::WriteNew(file,bytes);Require(!::chmod(file.c_str(),0400),"Seal reviewed failed-retirement target");
        EccReceiverPlanTarget target;target.agentId=remote.endpoints[i].agentId;target.planFile=file;target.planSha256=ConfigurationSha256(bytes);targets.push_back(target);
    }
    const auto status=data.Status();
    const auto failedId=ConfigurationSha256("getdaq.remote-data-operation:1\n"+status.controllerScopeSha256+'\n'+status.controllerEpoch+'\n'+originalPath+"\nretire-plan\n"+remote.endpoints[1].agentId);
    const auto failedDirectory=plans[1].auditRoot+"/control/operations/"+failedId;
    Require(!::symlink((failedDirectory+"/request.json").c_str(),(root+"/accepted-retirement-marker").c_str()),"Create owned not-yet-accepted fault marker");
    Rejects([&]{data.RetirePlans(originalPath,targets,successorScope,successorEpoch);},"Remote operation failed");
    const auto originalFailure=services[1]->Operation(failedId);
    Require(originalFailure.At("phase").text=="failed" && services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,"Original failed child was not actually accepted once");
    const auto oldRequest=IO::ReadFile(failedDirectory+"/request.json",8192);
    const auto intentBytes=IO::ReadFile(originalPath+"/intent.json",65536),journalBytes=IO::ReadFile(originalPath+"/remote.jsonl",65536);
    Require(!::rename((root+"/next-plan-1.backup").c_str(),targets[1].planFile.c_str()),"Restore exact reviewed target inode");
    IO::CreateDirectory(o.auditRoot+"/operations/complete");
    const auto actions=r.w.actions.size();
    // Restoring an input is not an explicit node completion. Parent sends GET only.
    Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/not-repaired");},"Remote supervisor HTTP 503");
    Require(services[1]->Commands("complete-plan-retirement")==0,"Parent automatically repaired an accepted failed child");
    auto repair=DecodeSupervisorControlRequest(oldRequest);repair.command=SupervisorCommand::CompletePlanRetirement;
    repair.failedRetirementOperationId=failedId;repair.operationId="explicit-node-repair";
    SupervisorHttpClient client(services[1]->direct);
    Require(client.Submit(repair).code==202,"Operator's explicit node repair was not accepted");
    Wait([&]{const auto phase=services[1]->Operation(repair.operationId).At("phase").text;Require(phase!="failed" && phase!="unknown","Explicit node repair failed");return phase=="complete";});
    const auto repaired=services[1]->Operation(repair.operationId);
    Require(services[1]->Operation(failedId).At("phase").text=="failed" && IO::ReadFile(failedDirectory+"/request.json",8192)==oldRequest,"Node repair rewrote original failure/request");
    ecc.CheckHandoffCompletionBoundary();
    data.CompletePendingHandoff(o.auditRoot+"/operations/complete/repaired");
    const auto receipt=data.CompletedControlHandoffJson();const auto value=ParseStrictJson(receipt);
    Require(value.At("nodes").items.size()==2 && value.At("kind").text=="getdaq.remote-plan-retirement","Parent omitted a completed retirement node");
    for(std::size_t i=0;i<plans.size();++i) {
        const auto &node=value.At("nodes").items[i];const auto request=DecodeSupervisorControlRequest(node.At("request").text);
        Require(request.command==SupervisorCommand::RetirePlan && request.successorPlanSha256==targets[i].planSha256 &&
            request.successorPlanFile==targets[i].planFile && services[i]->Commands("retire-plan")==1,"Parent changed or resent an original retirement");
    }
    Require(value.At("nodes").items[1].At("proof_sha256").text==repaired.At("plan_retirement").At("proof_sha256").text,"Parent used a different explicit repair proof");
    RemoteHandoffEvidence evidence(originalPath,data.BindingSha256(),data.Status().controllerScopeSha256);
    Require(evidence.Nodes().size()==2 && evidence.Nodes()[0].rejection==0 && evidence.Nodes()[1].rejection==0,"Historical evidence lost accepted repaired node");
    Require(IO::ReadFile(originalPath+"/intent.json",65536)==intentBytes && IO::ReadFile(originalPath+"/remote.jsonl",65536)==journalBytes,"Parent completion rewrote original audit");
    Require(services[1]->Commands("complete-plan-retirement")==1 && services[1]->Operation(failedId).At("phase").text=="failed","Parent repeated repair or changed original failure");
    Require(r.w.actions.size()==actions && data.NeedsReconciliation() && ecc.Status().needsReconciliation,"Repair observation changed hardware or released old control");
    Rejects([&]{data.VerifyStopped();},"UNKNOWN");
    for(const auto &entry:oldFiles)Require(ConfigurationSha256(IO::ReadFile(entry.first,1024*1024))==entry.second,"Parent completion changed old raw");
    IO::WriteNew(root+"/control-handoff-result.json",receipt);
    IO::WriteNew(root+"/failed-retirement-parent-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(encodingMinor)+
        ",\"both_original_children_accepted_once\":true,\"original_failure_preserved\":true,\"operator_node_repair_count\":1,\"parent_repair_posts\":0,\"old_control_fenced\":true,\"hardware_writes\":0,\"raw_forwarded\":false}\n");
}
#endif
