#ifndef GETDAQ_REMOTE_PLAN_COMPLETION_CASE_H
#define GETDAQ_REMOTE_PLAN_COMPLETION_CASE_H
// Included after the original real receiver cycle is normally sealed.
{
    const auto originalPath=o.auditRoot+"/operations/manual/retire-plans";
    const std::string successorScope(64,'e'),successorEpoch(64,'f');
    std::vector<EccReceiverPlanTarget> targets;std::vector<std::string> reviewed;
    const auto executableSha=ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024));
    for(std::size_t i=0;i<plans.size();++i) {
        auto changed=plans[i];changed.auditRoot=root+"/next-data-"+std::to_string(i);changed.deploymentId="reviewed-successor-"+std::to_string(i);
        const auto bytes=IO::EncodeOptions(changed,executableSha),file=root+"/next-plan-"+std::to_string(i)+".json";
        IO::WriteNew(file,bytes);Require(!::chmod(file.c_str(),0400),"Seal reviewed receiver plan");
        EccReceiverPlanTarget target;target.agentId=remote.endpoints[i].agentId;target.planFile=file;target.planSha256=ConfigurationSha256(bytes);
        targets.push_back(target);reviewed.push_back(bytes);
    }
    Require(!::unlink(targets[1].planFile.c_str()),"Remove only second owned reviewed plan");
    Rejects([&]{data.RetirePlans(originalPath,targets,successorScope,successorEpoch);},"Remote supervisor HTTP");
    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==0,"Partial retirement acceptance differs");
    RemoteHandoffEvidence originalEvidence(originalPath,data.BindingSha256(),data.Status().controllerScopeSha256);
    Require(originalEvidence.Nodes().size()==2 && originalEvidence.Nodes()[0].rejection==0 &&
        (originalEvidence.Nodes()[1].rejection==409 || originalEvidence.Nodes()[1].rejection==412),"Cold review lost original rejection or accepted sibling");
    Require(originalEvidence.Epoch()==data.Status().controllerEpoch,"Cold review changed original epoch");
    Rejects([&]{RemoteHandoffEvidence wrong(originalPath,std::string(64,'0'),data.Status().controllerScopeSha256);},"binding");
    Rejects([&]{RemoteHandoffEvidence wrong(originalPath,data.BindingSha256(),std::string(64,'0'));},"snapshot/root");
    ecc.CheckHandoffCompletionBoundary();
    Require(ecc.Status().needsReconciliation && data.NeedsReconciliation(),"Control-boundary check cleared the data fence");
    Rejects([&]{data.CompletedControlHandoffJson();},"No complete");
    IO::CreateDirectory(o.auditRoot+"/operations/complete");
    // A still-missing reviewed plan must not undo the first node or restart it.
    Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/missing-plan");},"Remote supervisor HTTP");
    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==0,"Explicit failed completion replayed retired sibling");
    Rejects([&]{originalEvidence.Check();},"changed");
    RemoteHandoffEvidence afterRejectedAttempt(originalPath,data.BindingSha256(),data.Status().controllerScopeSha256);
    Require(afterRejectedAttempt.Nodes()[0].rejection==0 &&
        (afterRejectedAttempt.Nodes()[1].rejection==409 || afterRejectedAttempt.Nodes()[1].rejection==412),"Cold review failed to combine durable explicit rejections");
    IO::WriteNew(targets[1].planFile,reviewed[1]);Require(!::chmod(targets[1].planFile.c_str(),0400),"Restore exact reviewed plan");
    const auto before=r.w.actions.size();
    data.CompletePendingHandoff(o.auditRoot+"/operations/complete/data-handoff");
    Require(r.w.actions.size()==before,"Plan completion wrote hardware");
    const auto receipt=data.CompletedControlHandoffJson();const auto value=ParseStrictJson(receipt);
    RemoteHandoffEvidence completedEvidence(originalPath,data.BindingSha256(),data.Status().controllerScopeSha256);
    Require(completedEvidence.Nodes()[0].rejection==0 && completedEvidence.Nodes()[1].rejection==0,"Cold review permits replay after later accepted completion");
    Require(value.At("kind").text=="getdaq.remote-plan-retirement" && value.At("audit_directory").text==originalPath &&
        value.At("successor_scope_sha256").text==successorScope && value.At("successor_epoch").text==successorEpoch,"Completed retirement changed original target");
    Require(value.At("nodes").items.size()==2,"Completed retirement omitted a data agent");
    for(std::size_t i=0;i<targets.size();++i) {
        const auto request=DecodeSupervisorControlRequest(value.At("nodes").items[i].At("request").text);
        Require(request.command==SupervisorCommand::RetirePlan && request.successorPlanSha256==targets[i].planSha256 &&
            request.successorPlanFile==targets[i].planFile && request.parentAuditReference==originalPath,"Completion changed original child request");
        Require(services[i]->Commands("retire-plan")==1,"Completion accepted a child request more than once");
    }
    Require(data.NeedsReconciliation(),"Completion cleared the permanent original fence");
    Rejects([&]{data.VerifyStopped();},"UNKNOWN");
    Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/again");},"retained original unfinished");
    RemoteReceiverManager cold(remote);
    Rejects([&]{cold.CompletePendingHandoff(o.auditRoot+"/operations/complete/cold");},"retained original unfinished");
    const auto contextFile=originalPath+"/handoff-context.json",contextBytes=IO::ReadFile(contextFile,1024*1024);
    {std::fstream file(contextFile,std::ios::in|std::ios::out|std::ios::binary);file.put('!');file.flush();Require(bool(file),"Inject owned context drift");}
    Rejects([&]{data.CompletedControlHandoffJson();},"file changed");
    {std::ofstream file(contextFile,std::ios::binary|std::ios::trunc);file<<contextBytes;file.close();Require(bool(file),"Restore owned context bytes");}
    Rejects([&]{data.CompletedControlHandoffJson();},"storage fault remains latched");
    for(const auto &service:services)Require(service->Commands("retire-plan")==1,"Context drift/review replayed a child request");
    for(const auto &entry:oldFiles)Require(ConfigurationSha256(IO::ReadFile(entry.first,1024*1024))==entry.second,"Completion changed old raw");
    IO::WriteNew(root+"/control-handoff-result.json",receipt);
    IO::WriteNew(root+"/handoff-proof.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(encodingMinor)+
        ",\"reply_loss\":false,\"original_manager_fenced\":true,\"old_raw_retained\":true,\"raw_forwarded\":false,\"explicit_plan_completion\":true}\n");
}
#endif
