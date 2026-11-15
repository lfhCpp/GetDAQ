#ifndef GETDAQ_ECC_PLAN_COMPLETION_NETWORK_CASE_H
#define GETDAQ_ECC_PLAN_COMPLETION_NETWORK_CASE_H
// Included at the failed original retirement boundary. It does not rewrite the
// failed parent operation; a separate explicit completion enables its receipt.
{
    Require(operation.At("phase").text=="failed","Fixture did not retain original partial retirement");
    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==0,"Original partial retirement differs");
    IO::WriteNew(root+"/partial-retirement-operation.json",direct.Operation(request.operationId).body);
    Require(direct.ControlHandoff().code==503,"Incomplete original exported a receipt");
    if(tool) {
        const auto originalAudit=root+"/controller/operations/"+request.operationId+"/data-handoff";
        const std::vector<std::string> review={"review-cold",config,originalAudit,inputs+"/cold-review-original"};
        HandoffTool(tool,review,inputs+"/cold-review-original.log");HandoffTool(tool,review,inputs+"/cold-review-repeat-refused.log",false);
        const auto report=ParseStrictJson(IO::ReadFile(inputs+"/cold-review-original/review.json",1024*1024));
        Require(!IO::Boolean(report.At("control_granted")) && !IO::Boolean(report.At("cold_recovery_executed")) &&
            report.At("nodes").items.size()==2 && report.At("nodes").items[0].At("historical_delivery").text=="uncertain_or_accepted" &&
            report.At("nodes").items[1].At("historical_delivery").text=="explicit_rejection","Cold review granted control or lost original delivery evidence");
    }
    const auto originalEvidence=inputs+"/partial-evidence";IO::CreateDirectory(originalEvidence);
    IO::WriteNew(originalEvidence+"/request.json",EncodeEccControlRequest(request));IO::WriteNew(originalEvidence+"/original-status.json",originalStatusForCompletion);
    auto completion=Request(identity,"complete-data-handoff","complete-original-handoff");completion.failedOperationId=request.operationId;
    auto wrong=completion;wrong.failedOperationId="prepare";
    Require(direct.PostOperation(EncodeEccControlRequest(wrong)).code==409,"Completion accepted a different original operation");
    wrong=completion;wrong.clientEpoch=std::string(64,'b');
    Require(direct.PostOperation(EncodeEccControlRequest(wrong)).code==409,"Completion accepted another client epoch");
    if(scenario.back()=='0') {
        auto missing=completion;missing.operationId="complete-missing-plan";
        const auto reply=direct.PostOperation(EncodeEccControlRequest(missing));
        Require(reply.code==202,("Explicit failed completion intent not accepted: "+std::to_string(reply.code)+" "+reply.body).c_str());
        Require(Terminal(direct,missing.operationId).At("phase").text=="failed","Missing reviewed plan was ignored");
        Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==0,"Failed explicit completion replayed a retired sibling");
        Require(direct.ControlHandoff().code==503,"Failed completion exported receipt");
    }
    const auto bytes=IO::EncodeOptions(nextPlans[1],ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)));
    Require(ConfigurationSha256(bytes)==targets[1].planSha256,"Restore would change the reviewed plan");
    IO::WriteNew(targets[1].planFile,bytes);Require(!::chmod(targets[1].planFile.c_str(),0400),"Restore exact second reviewed plan");
    if(scenario.back()=='1') {
        Require(tool,"Completion CLI path must be provided");
        const std::vector<std::string> arguments={"complete-submit",config,nextFile,planFile,originalEvidence,completion.operationId,inputs+"/completion-evidence"};
        HandoffTool(tool,arguments,inputs+"/completion-submit.log");
        auto inspect=arguments;inspect[0]="complete-inspect";HandoffTool(tool,inspect,inputs+"/completion-inspect.log");
        HandoffTool(tool,arguments,inputs+"/completion-repeat-refused.log",false);
    } else {
        const auto reply=direct.PostOperation(EncodeEccControlRequest(completion));
        Require(reply.code==202,("Explicit completion request not accepted: "+std::to_string(reply.code)+" "+reply.body).c_str());
    }
    operation=Terminal(direct,completion.operationId);
    Require(operation.At("phase").text=="complete","Original plan retirement did not explicitly complete");
    IO::WriteNew(root+"/completion-operation.json",direct.Operation(completion.operationId).body);
    Require(direct.Operation(request.operationId).code==200 && Terminal(direct,request.operationId).At("phase").text=="failed","Completion rewrote the original failure history");
    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,"Completion omitted/replayed a retired child");
    Require(direct.PostOperation(EncodeEccControlRequest(completion)).code==202,"Same completion ID did not observe its original result");
    auto repeated=completion;repeated.operationId="another-completion";
    Require(direct.PostOperation(EncodeEccControlRequest(repeated)).code==409,"Already completed handoff accepted another completion");
    Require(rig.w.actions.size()==hardwareBefore,"Completion touched board hardware");
    IO::WriteNew(root+"/partial-completion-result.json","{\"passed\":true,\"original_failure_retained\":true,\"both_original_children_accepted_once\":true,\"old_controller_still_fenced\":true,\"no_completion_hardware_writes\":true,\"operator_cli\":"+std::string(scenario.back()=='1' ? "true" : "false")+"}\n");
}
#endif
