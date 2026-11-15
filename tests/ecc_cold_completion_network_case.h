#ifndef GETDAQ_ECC_COLD_COMPLETION_NETWORK_CASE_H
#define GETDAQ_ECC_COLD_COMPLETION_NETWORK_CASE_H
// Original full ECC cycle is normally released. Preserve the failed network
// parent; cold completion is a separate tool process with its own evidence.
{
    Require(tool,"Cold completion needs the actual handoff CLI executable");
    const std::string childCommand=changedPlans ? "retire-plan" : "handoff-control";
    Require(operation.At("phase").text=="failed" && services[0]->Commands(childCommand)==1 && services[1]->Commands(childCommand)==(changedPlans ? 0 : 1),
        "Cold CLI fixture lacks the original partial retirement");
    const auto failedBytes=direct.Operation(request.operationId).body;
    IO::WriteNew(root+"/partial-retirement-operation.json",failedBytes);IO::WriteNew(root+"/handoff-operation.json",failedBytes);
    IO::WriteNew(root+"/original-fenced-status.json",direct.Status().body);
    Require(direct.PostOperation(EncodeEccControlRequest(Request(identity,"prepare","before-cold-fenced"))).code==409,
        "Original ECC became usable before cold completion");
    const auto originalEvidence=inputs+"/cold-original-evidence";IO::CreateDirectory(originalEvidence);
    IO::WriteNew(originalEvidence+"/request.json",EncodeEccControlRequest(request));
    IO::WriteNew(originalEvidence+"/original-status.json",originalStatusForCompletion);
    const char *recoveryExecutable=::getenv("GETDAQ_TEST_COLD_RECOVERY_SERVICE");
    std::unique_ptr<ColdRecoveryFixture> recovery;
    if(recoveryExecutable)recovery.reset(new ColdRecoveryFixture(recoveryExecutable,tool,config,nextFile,planFile,originalEvidence,inputs));
    const auto invoke=[&](const std::vector<std::string> &arguments,const std::string &log,bool okay) {
        if(recovery)recovery->Call(arguments,log,okay);else HandoffTool(tool,arguments,log,okay);
    };
    std::vector<std::string> live={"cold-complete-submit",config,nextFile,planFile,originalEvidence,"cold-while-live",inputs+"/cold-live-refused"};
    if(recovery)live[6]=recovery->Attempt(live[5]);
    invoke(live,inputs+"/cold-live-refused.log",false);
    Require(IO::ReadFile(inputs+"/cold-live-refused.log",65536).find("locked")!=std::string::npos,"Live rejection did not reach the native lock boundary");
    Require(::access((root+"/controller/operations/cold-while-live").c_str(),F_OK)!=0,"Live ECC rejection started a cold data attempt");
    process->Stop();
    IO::WriteNew(root+"/cold-original-ecc-exit.json","{\"normal_exit\":true,\"exit_code\":0}\n");
    const auto nativeBefore=IO::ReadFile(root+"/controller/ecc.jsonl",64*1024*1024);
    const auto networkBefore=IO::ReadFile(root+"/controller/network/operations.jsonl",64*1024*1024);
    IO::WriteNew(root+"/cold-original-native.jsonl",nativeBefore);IO::WriteNew(root+"/cold-original-network.jsonl",networkBefore);
    if(coldSame)coldDataRelay->hideOperations=true;
    auto missing=live;missing[5]=coldSame ? "cold-uncertain" : "cold-missing-plan";missing[6]=inputs+"/cold-missing-evidence";
    if(recovery)missing[6]=recovery->Attempt(missing[5]);
    invoke(missing,inputs+"/cold-missing-submit.log",false);
    Require(IO::ReadFile(inputs+"/cold-missing-submit.log",65536).find(coldSame ? "delivery was uncertain" : "Remote supervisor HTTP")!=std::string::npos,"Rejected completion did not reach its expected original node boundary");
    auto missingInspect=missing;missingInspect[0]="cold-complete-inspect";
    invoke(missingInspect,inputs+"/cold-missing-inspect.log",false);
    Require(IO::ReadFile(inputs+"/cold-missing-inspect.log",65536).find("not durably complete")!=std::string::npos,"Failed inspection did not verify its durable terminal state");
    Require(services[0]->Commands(childCommand)==1 && services[1]->Commands(childCommand)==(changedPlans ? 0 : 1),"Cold failed attempt replayed a child");
    if(changedPlans) {
        const auto exact=IO::EncodeOptions(nextPlans[1],ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)));
        Require(ConfigurationSha256(exact)==targets[1].planSha256,"Cold fixture changed the originally reviewed plan");
        IO::WriteNew(targets[1].planFile,exact);Require(!::chmod(targets[1].planFile.c_str(),0400),"Restore exact cold reviewed plan");
    }else coldDataRelay->hideOperations=false;
    auto arguments=live;arguments[5]="cold-data-completion";arguments[6]=inputs+"/cold-completion-evidence";
    if(recovery)arguments[6]=recovery->Attempt(arguments[5]);
    const char *windowProbe=::getenv("GETDAQ_TEST_COLD_RECOVERY_WINDOW");
    if(recovery && windowProbe) {
        arguments[5]=recovery->Window(windowProbe,"submit");arguments[6]=recovery->Attempt(arguments[5]);
        IO::WriteNew(inputs+"/cold-submit.log","Production ColdHandoffWindow submitted fixed recovery task\n");
    }else invoke(arguments,inputs+"/cold-submit.log",true);
    invoke(arguments,inputs+"/cold-repeat-submit-refused.log",bool(recovery));
    auto inspect=arguments;inspect[0]="cold-complete-inspect";invoke(inspect,inputs+"/cold-inspect.log",true);
    receipt.body=IO::ReadFile(arguments[6]+"/receipt.json",1024*1024);receipt.code=200;
    Require(!::unlink((arguments[6]+"/receipt.json").c_str()),"Remove owned receipt to simulate lost external export");
    if(recovery && windowProbe) {recovery->Window(windowProbe,"inspect",arguments[5]);IO::WriteNew(inputs+"/cold-reexport.log","Production window read-only inspection re-exported receipt\n");}
    else invoke(inspect,inputs+"/cold-reexport.log",true);
    Require(IO::ReadFile(arguments[6]+"/receipt.json",1024*1024)==receipt.body,"Cold inspection changed re-exported receipt");
    auto wrong=inspect;wrong[5]="foreign-completion";invoke(wrong,inputs+"/cold-foreign-id-refused.log",false);
    if(recovery)recovery->RestartAndCheck(arguments[5]);
    if(recovery) {
        const auto crashRoot=inputs+"/crash-case";IO::CreateDirectory(crashRoot);
        const auto gate=crashRoot+"/gate",marker=crashRoot+"/child.pid",wrapper=crashRoot+"/gated-cli.py";
        IO::WriteNew(gate,"owned gate\n");
        IO::WriteNew(wrapper,"#!/usr/bin/python3\nimport hashlib,os,sys,time\nfrom pathlib import Path\ntool="+EncodeJsonString(tool)+"\nassert hashlib.sha256(Path(tool).read_bytes()).hexdigest()=="+EncodeJsonString(ConfigurationSha256(IO::ReadFile(tool,64*1024*1024)))+"\ngate=Path("+EncodeJsonString(gate)+")\nif gate.exists():\n fd=os.open("+EncodeJsonString(marker)+",os.O_WRONLY|os.O_CREAT|os.O_EXCL,0o600)\n os.write(fd,str(os.getpid()).encode());os.close(fd)\n while gate.exists():time.sleep(0.01)\nos.execv(tool,[tool]+sys.argv[1:])\n");
        Require(!::chmod(wrapper.c_str(),0700),"Seal gated crash wrapper");
        ColdRecoveryFixture crashed(recoveryExecutable,wrapper,config,nextFile,planFile,originalEvidence,crashRoot);crashed.CrashAndInspect(marker,gate);
    }
    Require(services[0]->Commands(childCommand)==1 && services[1]->Commands(childCommand)==1,"Cold tool omitted/replayed an original child");
    Require(IO::ReadFile(root+"/controller/ecc.jsonl",64*1024*1024)==nativeBefore &&
        IO::ReadFile(root+"/controller/network/operations.jsonl",64*1024*1024)==networkBefore,"Cold tool changed original native/network history");
    Require(rig.w.actions.size()==hardwareBefore,"Cold data tool touched hardware");
    IO::WriteNew(root+"/cold-completion-result.json","{\"passed\":true,\"old_ecc_exited_before_completion\":true,\"independent_cli_processes\":true,\"original_parent_failure_preserved\":true,\"both_original_children_accepted_once\":true,\"native_network_audits_unchanged\":true,\"lost_receipt_reexported_without_post\":true,\"old_hardware_authority_restored\":false,\"raw_payload_forwarded\":false}\n");
}
#endif
