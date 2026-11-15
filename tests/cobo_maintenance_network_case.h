#ifndef GETDAQ_COBO_MAINTENANCE_NETWORK_CASE_H
#define GETDAQ_COBO_MAINTENANCE_NETWORK_CASE_H
// Included at the original healthy Prepared boundary of the full network fixture.
// The production GUI submits the only maintenance request; normal data cycles follow.
{
    const auto targetsReply=direct.CoBoMaintenanceTargets();Require(targetsReply.code==200,("Fixed maintenance target metadata unavailable: HTTP "+std::to_string(targetsReply.code)+" "+targetsReply.body).c_str());
    const auto targets=DecodeEccControlMaintenanceTargets(targetsReply.body);
    Require(targets.instanceId==identity.At("instance_id").text && targets.snapshotSha256==identity.At("snapshot_sha256").text && targets.cobos.size()==2,
            "Maintenance metadata identity or activity set differs");
    IO::WriteNew(root+"/maintenance-targets.json",targetsReply.body);
    const auto initialWrites=rig.w.Writes();const auto initialJournal=IO::ReadFile(root+"/controller/network/operations.jsonl",1024*1024);
    auto invalid=Request(identity,"reprepare-cobo","inactive-maintenance");invalid.coboId="C2";
    Require(direct.PostOperation(EncodeEccControlRequest(invalid)).code==412,"Inactive maintenance target was accepted");
    Require(rig.w.Writes()==initialWrites && IO::ReadFile(root+"/controller/network/operations.jsonl",1024*1024)==initialJournal,"Rejected target persisted or mutated hardware");
    const auto otherWrites=rig.w.c[1].writes;const auto dormantWrites=rig.w.c[2].writes;const auto otherCtrl=rig.w.c[1].ctrl;
    const auto otherBoards=rig.w.c[1].boards;const auto otherChips=rig.w.c[1].chips;
    const auto links=rig.w.links.load(),tdc=rig.w.tdc.load(),multi=rig.w.multi.load();
    const auto otherLedCalls=rig.nodes[1]->ledCalls.load(),dormantLedCalls=rig.nodes[2]->ledCalls.load();
    const auto otherBlinkCalls=rig.nodes[1]->blinkCalls.load(),dormantBlinkCalls=rig.nodes[2]->blinkCalls.load();
    const auto otherFlipCalls=rig.nodes[1]->flipCalls.load(),dormantFlipCalls=rig.nodes[2]->flipCalls.load();
    const auto otherFlipAllCalls=rig.nodes[1]->flipAllCalls.load(),dormantFlipAllCalls=rig.nodes[2]->flipAllCalls.load();
    const auto selectedFlipCalls=rig.nodes[0]->flipCalls.load();
    const auto selectedPulseCalls=rig.nodes[0]->pulseCalls.load();
    const auto selectedBlinkCalls=rig.nodes[0]->blinkCalls.load();
    const auto otherSubscribes=rig.w.alarms[1]->subscribes,otherUnsubscribes=rig.w.alarms[1]->unsubscribes;
    auto invalidConfigure=Request(identity,"configure-cobo","inactive-configure-cobo");invalidConfigure.coboId="C2";
    Require(direct.PostOperation(EncodeEccControlRequest(invalidConfigure)).code==412 && rig.w.Writes()==initialWrites,
            "Inactive selected-CoBo Configure was accepted or wrote hardware");
    const auto selectedConfigureBefore=rig.w.Writes();
    const auto otherConfigureBefore=rig.w.c[1].writes,dormantConfigureBefore=rig.w.c[2].writes;
    auto configureCoBo=Request(identity,"configure-cobo","configure-cobo-network");configureCoBo.coboId="C0";
    Require(direct.PostOperation(EncodeEccControlRequest(configureCoBo)).code==202,"Selected-CoBo Configure was not accepted by ECC HTTP");
    Terminal(client,configureCoBo.operationId);
    const auto configureCoBoReply=direct.Operation(configureCoBo.operationId);const auto configureCoBoOperation=DecodeEccControlOperation(configureCoBoReply.body);
    Require(configureCoBoReply.code==200 && configureCoBoOperation.phase=="complete","Selected-CoBo Configure operation did not complete");
    Require(EncodeEccControlRequest(configureCoBoOperation.request)==EncodeEccControlRequest(configureCoBo),"Selected-CoBo Configure did not persist its exact target");
    Require(rig.w.Writes()>selectedConfigureBefore,"Selected-CoBo Configure completion had no observed hardware writes");
    Require(rig.w.c[1].writes==otherConfigureBefore && rig.w.c[2].writes==dormantConfigureBefore,"Selected-CoBo Configure touched another board");
    const auto configuredTargets=DecodeEccControlMaintenanceTargets(direct.CoBoMaintenanceTargets().body);
    Require(configuredTargets.cobos[0].lifecycleState=="configured" && configuredTargets.cobos[0].operation=="configure-cobo" &&
            configuredTargets.cobos[0].operationId==configureCoBo.operationId && configuredTargets.cobos[0].operationPhase=="complete" &&
            configuredTargets.cobos[1].lifecycleState=="prepared",
            "Single-CoBo Configure did not advance only the selected board lifecycle projection");
    const auto afterConfigureWrites=rig.w.Writes();const auto afterConfigureJournal=IO::ReadFile(root+"/controller/network/operations.jsonl",1024*1024);
    auto configureAgain=Request(identity,"configure-cobo","configure-cobo-network-again");configureAgain.coboId="C0";
    Require(direct.PostOperation(EncodeEccControlRequest(configureAgain)).code==412,
            "Single-CoBo Configure ignored the selected board's Configured lifecycle state");
    const bool duplicateConfigureNoMutation=rig.w.Writes()==afterConfigureWrites &&
        IO::ReadFile(root+"/controller/network/operations.jsonl",1024*1024)==afterConfigureJournal;
    Require(duplicateConfigureNoMutation,
            "Rejected duplicate CoBo Configure persisted an operation or changed hardware");
    State(client,EccState::Prepared);
    IO::WriteNew(root+"/configure-cobo-operation.json",configureCoBoReply.body);
    std::string nodes="[";
    for(std::size_t i=0;i<plans.size();++i) {if(nodes.back()!='[')nodes+=',';nodes+="{\"id\":"+std::to_string(i)+",\"role\":\"cobo\",\"host\":\"127.0.0.1\",\"native_sender_id\":"+EncodeJsonString(plans[i].sources[0].id)+",\"native_agent_id\":"+EncodeJsonString(remote.endpoints[i].agentId)+"}";}
    nodes+=']';
    const auto gui="{\"experiment\":\"NativeIntegration\",\"run_number\":0,\"getdaq\":{\"simulation\":false,\"config_dir\":"+EncodeJsonString(root+"/unused-legacy-config")+",\"output_dir\":"+EncodeJsonString(root+"/gui-runs")+
        ",\"poll_ms\":60,\"timeout_ms\":2000,\"step_timeout_ms\":180000,\"native_ecc\":{\"address\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+
        ",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"client_epoch\":"+EncodeJsonString(std::string(64,'a'))+",\"snapshot_sha256\":"+EncodeJsonString(identity.At("snapshot_sha256").text)+
        ",\"input_binding_sha256\":"+EncodeJsonString(identity.At("input_binding_sha256").text)+",\"timeout_ms\":2000,\"status_max_age_ms\":10000,\"record_dir\":"+EncodeJsonString(root+"/gui-client-records")+"},\"nodes\":"+nodes+"}}\n";
    IO::WriteNew(root+"/gui-maintenance.json",gui);IO::CreateDirectory(root+"/gui-maintenance-evidence");
    std::vector<std::string> words={argv[9],root+"/gui-maintenance.json",root+"/gui-maintenance-evidence"};std::vector<char *> arguments;
    for(auto &word:words)arguments.push_back(&word[0]);
    arguments.push_back(nullptr);
    std::vector<std::string> childEnvironment;const char *guard=std::getenv("GETDAQ_RAW_READ_GUARD"),*rawLog=std::getenv("GETDAQ_GUI_RAW_READ_LOG");
    Require(guard && *guard && rawLog && *rawLog,"Raw payload read guard is required for the production GUI probe");
    for(char **entry=environ;entry && *entry;++entry) {
        const std::string value(*entry);
        if(value.compare(0,11,"LD_PRELOAD=")==0 || value.compare(0,19,"GETDAQ_RAW_READ_LOG=")==0)continue;
        childEnvironment.push_back(value);
    }
    childEnvironment.push_back(std::string("LD_PRELOAD=")+guard);childEnvironment.push_back(std::string("GETDAQ_RAW_READ_LOG=")+rawLog);
    std::vector<char *> childEnv;for(auto &entry:childEnvironment)childEnv.push_back(&entry[0]);childEnv.push_back(nullptr);
    pid_t probe=0;Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),childEnv.data()),"Start production selected CoBo maintenance window");
    int status=0;Require(::waitpid(probe,&status,0)==probe && WIFEXITED(status) && WEXITSTATUS(status)==0,"Production maintenance window failed");
    IO::WriteNew(root+"/gui-programs.json","{\"probe\":"+EncodeJsonString(argv[9])+",\"probe_sha256\":"+EncodeJsonString(ConfigurationSha256(IO::ReadFile(argv[9],128*1024*1024)))+
        ",\"guard\":"+EncodeJsonString(guard)+",\"guard_sha256\":"+EncodeJsonString(ConfigurationSha256(IO::ReadFile(guard,1024*1024)))+"}\n");
    const auto request=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/request.json",8192));
    Require(request.command=="reprepare-cobo" && request.coboId=="C0" && request.instanceId==identity.At("instance_id").text,"GUI saved the wrong maintenance target");
    const auto operationReply=direct.Operation(request.operationId);const auto operation=DecodeEccControlOperation(operationReply.body);
    Require(operationReply.code==200 && operation.phase=="complete" && EncodeEccControlRequest(operation.request)==EncodeEccControlRequest(request),"GUI maintenance original operation did not complete");
    const auto repreparedReply=direct.CoBoMaintenanceTargets();IO::WriteNew(root+"/reprepared-maintenance-targets.json",repreparedReply.body);
    const auto repreparedTargets=DecodeEccControlMaintenanceTargets(repreparedReply.body);
    const auto latestPulseRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/pulse-request.json",8192));
    Require(repreparedTargets.cobos[0].lifecycleState=="prepared" && repreparedTargets.cobos[0].operation=="pulse-cobo-led" &&
            repreparedTargets.cobos[0].operationId==latestPulseRequest.operationId && repreparedTargets.cobos[0].operationPhase=="complete" &&
            repreparedTargets.cobos[1].lifecycleState=="prepared" && repreparedTargets.cobos[1].operationPhase=="idle",
            ("Selected/other CoBo lifecycle projection or latest operation evidence differs: "+repreparedReply.body).c_str());
    const auto ledRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/led-request.json",8192));
    Require(ledRequest.command=="set-cobo-led" && ledRequest.coboId=="C0" && ledRequest.ledAsadId==3 && ledRequest.ledType==2 && ledRequest.ledState==3 &&
        ledRequest.instanceId==identity.At("instance_id").text && ledRequest.snapshotSha256==identity.At("snapshot_sha256").text,
        "GUI did not persist the reviewed exact LED target/type/state");
    const auto ledReply=direct.Operation(ledRequest.operationId);const auto ledOperation=DecodeEccControlOperation(ledReply.body);
    Require(ledReply.code==200 && ledOperation.phase=="complete" && ledOperation.request.command=="set-cobo-led" &&
        EncodeEccControlRequest(ledOperation.request)==EncodeEccControlRequest(ledRequest),"Selected LED original operation did not complete");
    const auto blinkRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/blink-request.json",8192));
    Require(blinkRequest.command=="blink-cobo-leds" && blinkRequest.coboId=="C0" &&
        blinkRequest.instanceId==identity.At("instance_id").text && blinkRequest.snapshotSha256==identity.At("snapshot_sha256").text,
        "GUI did not persist the reviewed exact LED blink target");
    const auto blinkReply=direct.Operation(blinkRequest.operationId);const auto blinkOperation=DecodeEccControlOperation(blinkReply.body);
    Require(blinkReply.code==200 && blinkOperation.phase=="complete" && blinkOperation.request.command=="blink-cobo-leds" &&
        EncodeEccControlRequest(blinkOperation.request)==EncodeEccControlRequest(blinkRequest),"Selected LED blink original operation did not complete");
    const auto flipRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/flip-request.json",8192));
    Require(flipRequest.command=="flip-cobo-led" && flipRequest.coboId=="C0" && flipRequest.ledFlipId==10 &&
        flipRequest.instanceId==identity.At("instance_id").text && flipRequest.snapshotSha256==identity.At("snapshot_sha256").text,
        "GUI did not persist the exact reviewed GPIO LED bit");
    const auto flipReply=direct.Operation(flipRequest.operationId);const auto flipOperation=DecodeEccControlOperation(flipReply.body);
    Require(flipReply.code==200 && flipOperation.phase=="complete" && flipOperation.request.command=="flip-cobo-led" &&
        EncodeEccControlRequest(flipOperation.request)==EncodeEccControlRequest(flipRequest),"Selected LED flip original operation did not complete");
    const auto pulseRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/pulse-request.json",8192));
    Require(pulseRequest.command=="pulse-cobo-led" && pulseRequest.coboId=="C0" && pulseRequest.ledPulseId==10 && pulseRequest.ledPulsePeriodMs==725 &&
        pulseRequest.instanceId==identity.At("instance_id").text && pulseRequest.snapshotSha256==identity.At("snapshot_sha256").text,
        "GUI did not persist the reviewed exact LED pulse target/period");
    const auto pulseReply=direct.Operation(pulseRequest.operationId);const auto pulseOperation=DecodeEccControlOperation(pulseReply.body);
    Require(pulseReply.code==200 && pulseOperation.phase=="complete" && pulseOperation.request.command=="pulse-cobo-led" &&
        EncodeEccControlRequest(pulseOperation.request)==EncodeEccControlRequest(pulseRequest),"Selected LED pulse original operation did not complete");
    auto apiPulseRequest=Request(identity,"pulse-cobo-led","api-led-pulse");apiPulseRequest.coboId="C0";apiPulseRequest.ledPulseId=10;apiPulseRequest.ledPulsePeriodMs=725;
    Require(direct.PostOperation(EncodeEccControlRequest(apiPulseRequest)).code==202,"LED pulse was not accepted by the ECC API");
    Require(IO::String(Terminal(direct,apiPulseRequest.operationId).At("phase"))=="complete","API LED pulse did not complete");
    const auto pulserRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-maintenance-evidence/pulser-request.json",8192));
    Require(pulserRequest.command=="trigger-asad-pulser" && pulserRequest.coboId=="C0" && pulserRequest.pulserVoltageMv==321 &&
        pulserRequest.instanceId==identity.At("instance_id").text && pulserRequest.snapshotSha256==identity.At("snapshot_sha256").text,
        "GUI did not persist the reviewed exact pulser target/voltage");
    const auto pulserReply=direct.Operation(pulserRequest.operationId);const auto pulserOperation=DecodeEccControlOperation(pulserReply.body);
    Require(pulserReply.code==200 && pulserOperation.phase=="complete" && pulserOperation.request.command=="trigger-asad-pulser" &&
        EncodeEccControlRequest(pulserOperation.request)==EncodeEccControlRequest(pulserRequest),"Selected pulser original operation did not complete");
    IO::WriteNew(root+"/maintenance-flip-operation.json",flipReply.body);
    IO::WriteNew(root+"/maintenance-led-pulse-operation.json",pulseReply.body);
    IO::WriteNew(root+"/maintenance-pulser-operation.json",pulserReply.body);
    IO::WriteNew(root+"/maintenance-blink-operation.json",blinkReply.body);
    State(client,EccState::Prepared);
    Require(rig.nodes[0]->pulseCalls==selectedPulseCalls+2 && rig.nodes[0]->lastPulseLed==10 && rig.nodes[0]->lastPulsePeriod==725 &&
            rig.nodes[1]->pulseCalls==0 && rig.nodes[2]->pulseCalls==0,"GUI and API LED pulse requests did not each call only C0 once with the exact period");
    const auto pulseDirectory=root+"/controller/operations/"+pulseRequest.operationId;
    const auto pulseAudit=GetDAQPrepareFixture::ReadFile(pulseDirectory+"/maintenance/led-pulse.jsonl");
    Require(pulseAudit.find("\"led_id\":10")!=std::string::npos && pulseAudit.find("\"period_ms\":725")!=std::string::npos &&
            pulseAudit.find("\"automatic_retry\":false")!=std::string::npos && pulseAudit.find("\"hardware_acknowledged\":true")!=std::string::npos,
            "Selected LED pulse audit does not bind the exact period and one-shot result");
    const auto apiPulseAudit=GetDAQPrepareFixture::ReadFile(root+"/controller/operations/"+apiPulseRequest.operationId+"/maintenance/led-pulse.jsonl");
    Require(apiPulseAudit.find("\"led_id\":10")!=std::string::npos && apiPulseAudit.find("\"period_ms\":725")!=std::string::npos &&
            apiPulseAudit.find("\"automatic_retry\":false")!=std::string::npos,"Direct API pulse audit lost its target or period binding");
    IO::WriteNew(root+"/maintenance-led-operation.json",ledReply.body);State(client,EccState::Prepared);
    IO::WriteNew(root+"/maintenance-operation.json",operationReply.body);State(client,EccState::Prepared);
    Require(rig.w.c[1].writes==otherWrites && rig.w.c[2].writes==dormantWrites && rig.w.c[1].ctrl==otherCtrl && rig.w.c[1].boards==otherBoards && rig.w.c[1].chips==otherChips,
            "Selected network maintenance changed another CoBo");
    Require(rig.w.links==links && rig.w.tdc==tdc && rig.w.multi==multi && rig.w.alarms[1]->subscribes==otherSubscribes && rig.w.alarms[1]->unsubscribes==otherUnsubscribes,
            "Selected network maintenance repeated alignment or changed another subscription");
    {
        std::lock_guard<std::mutex> lock(rig.w.logMutex);const std::string expected="C0.pulser-trigger=321";
        Require(std::count(rig.w.actions.begin(),rig.w.actions.end(),expected)==1,"Selected network pulser was not executed exactly once at the reviewed voltage");
        Require(std::count_if(rig.w.actions.begin(),rig.w.actions.end(),[](const std::string &action) {return action.find(".pulser-trigger=321")!=std::string::npos && action.compare(0,2,"C0")!=0;})==0,
                "Selected network pulser changed another CoBo");
    }
    Require(rig.nodes[1]->ledCalls==otherLedCalls && rig.nodes[2]->ledCalls==dormantLedCalls &&
            rig.nodes[1]->blinkCalls==otherBlinkCalls && rig.nodes[2]->blinkCalls==dormantBlinkCalls && rig.nodes[0]->blinkCalls==selectedBlinkCalls+1,
            "Selected LED request changed another/dormant manager or did not blink only C0 once");
    Require(rig.nodes[0]->flipCalls==selectedFlipCalls+1 && rig.nodes[0]->lastFlipLed==10 &&
            rig.nodes[1]->flipCalls==otherFlipCalls && rig.nodes[2]->flipCalls==dormantFlipCalls &&
            rig.nodes[1]->flipAllCalls==otherFlipAllCalls && rig.nodes[2]->flipAllCalls==dormantFlipAllCalls,
            "Selected LED flip did not invert exactly GPIO bit 10 on only C0");
    {
        std::lock_guard<std::mutex> lock(rig.w.logMutex);const std::string expected="C0.led=2:3:3";
        Require(std::count(rig.w.actions.begin(),rig.w.actions.end(),expected)==1,"Exact selected LED action was not issued exactly once");
    }
    auto changed=request;changed.coboId="C1";Require(direct.PostOperation(EncodeEccControlRequest(changed)).code==412,"Same maintenance ID accepted a different target");
    auto changedLed=ledRequest;changedLed.ledState=2;Require(direct.PostOperation(EncodeEccControlRequest(changedLed)).code==412,"Same LED operation ID accepted a different LED state");
    auto changedPulser=pulserRequest;changedPulser.pulserVoltageMv=320;Require(direct.PostOperation(EncodeEccControlRequest(changedPulser)).code==412,"Same pulser operation ID accepted a different voltage");
    auto changedFlip=flipRequest;changedFlip.ledFlipId=11;Require(direct.PostOperation(EncodeEccControlRequest(changedFlip)).code==412,"Same non-idempotent flip ID accepted a different GPIO bit");
    auto changedPulse=apiPulseRequest;changedPulse.ledPulsePeriodMs=726;Require(direct.PostOperation(EncodeEccControlRequest(changedPulse)).code==412,"Same LED pulse ID accepted a different period");
    Require(direct.PostOperation(EncodeEccControlRequest(request)).code==202,"Original maintenance result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(ledRequest)).code==202,"Original LED operation result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(pulserRequest)).code==202,"Original pulser operation result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(blinkRequest)).code==202,"Original blink operation result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(flipRequest)).code==202,"Original LED flip operation result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(pulseRequest)).code==202,"Original GUI LED pulse result not observable by same request");
    Require(direct.PostOperation(EncodeEccControlRequest(apiPulseRequest)).code==202,"Original API LED pulse result not observable by same request");
    IO::WriteNew(root+"/maintenance-result.json","{\"passed\":true,\"actual_main_window\":true,\"reprepare_operation_id\":"+EncodeJsonString(request.operationId)+
        ",\"led_operation_id\":"+EncodeJsonString(ledRequest.operationId)+",\"blink_operation_id\":"+EncodeJsonString(blinkRequest.operationId)+
        ",\"flip_operation_id\":"+EncodeJsonString(flipRequest.operationId)+",\"flip_led_id\":10,\"selected_flip_command_count\":1,\"led_pulse_operation_id\":"+EncodeJsonString(pulseRequest.operationId)+",\"led_pulse_id\":10,\"led_pulse_period_ms\":725,\"selected_led_pulse_count\":1,\"changed_led_pulse_period_rejected\":true,\"pulser_operation_id\":"+EncodeJsonString(pulserRequest.operationId)+
        ",\"pulser_voltage_mv\":321,\"selected_pulser_command_count\":1,\"led_type\":2,\"led_asad_id\":3,\"led_state\":3,\"selected_led_command_count\":1,\"selected_blink_command_count\":1,\"changed_flip_bit_rejected\":true,\"configured_board_repeat_rejected\":true,\"duplicate_configure_no_mutation\":"+(duplicateConfigureNoMutation ? "true" : "false")+",\"other_board_unchanged\":true,\"other_subscription_unchanged\":true,\"system_alignment_not_repeated\":true,\"physical_lamp_state_verified\":false,\"physical_pulse_verified\":false}\n");
}
#endif
