#include "cobo_prepare_fixture.h"
using namespace GetDAQPrepareFixture;
int main(int argc, char **argv)
{
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc == 2,"Expected actual CoBo hardware description fixture");
        const auto policy = LoadHardwareDescription(argv[1]);
        PrepareFixtureState state; for (const auto &d : policy.devices) state.descriptions[d.name] = d;
        Ice::InitializationData init; init.properties = Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator = Ice::initialize(init);
        auto adapter = communicator->createObjectAdapterWithEndpoints("PrepareTest","tcp -h 127.0.0.1 -p 0");
        IceUtil::Handle<PrepareAlarmService> alarms = new PrepareAlarmService;
        IceUtil::Handle<PrepareNode> node = new PrepareNode(*alarms);
        for (const auto &d : policy.devices) node->fDevices[d.name] = DevicePrx::uncheckedCast(adapter->addWithUUID(new PrepareDevice(d,state)));
        node->service = get::mt::AlarmServicePrx::uncheckedCast(adapter->addWithUUID(alarms));
        const auto address = communicator->proxyToString(adapter->addWithUUID(node)); adapter->activate();
        state.notify = [&] { alarms->sendAsadAlarm("CoBo0",8,Ice::Current()); };
        char directory[] = "/tmp/getdaq-prepare-tcp-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Create fixture directory");
        unsigned int count = 0; const auto path = [&] { return std::string(directory)+"/operation-"+std::to_string(count++); };
        const auto reset = [&](int mode) { node->Join(); state.Reset(mode); alarms->Reset(mode); node->alarmMode = mode; node->monitorCalls = node->ledCalls = 0;
            node->missingService = node->wrongService = false; node->fName = "CoBo0"; alarms->subscribers["other-client"] = nullptr; };
        for (int minor : {0,1}) {
            CoBoPrepareOptions options; options.nodeProxy = address; options.expectedNodeName = "CoBo0"; options.description = policy;
            options.settings = Settings(); options.encodingMinor = minor; options.timeoutMs = 1000;
            options.callbackEndpoints = "tcp -h 127.0.0.1 -p 0"; options.ownsStoppedHardware = options.dataProcessesStopped = true;
            reset(0); const auto audit = path(); CoBoPrepareClient client(options); client.Prepare(audit);
            Require(client.Status() == CoBoPrepareClient::Complete && client.Alarms()->Snapshot().subscription == CoBoAlarmSnapshot::Active,"Prepare did not retain active owned listener");
            Require(state.powered == 9 && state.firmwareStarts == 1 && state.finalThresholds == 3,"Prepare omitted power/automatic delay/global thresholds");
            Require(node->monitorCalls == 1 && node->ledCalls == 3 && alarms->subscribes == 1 && !alarms->resets && !alarms->unsubscribes,"Prepare service coverage or implicit reset/unsubscribe");
            const auto image = CompileAsAdInputs(options.settings.inputs);
            for (const auto &b : image.boards) {
                Require(state.boards[b.asadId]["ADC0"] == b.adc0,"ADC reset lost final sparse/disabled channel bits");
                Require(state.boards[b.asadId]["ADC2"] == 0x200 && state.boards[b.asadId]["ADC5"] == 0x401,"Final channel application reset ADC data format");
                Require(state.boards[b.asadId]["IM0"] == b.im0 && state.boards[b.asadId]["IM1"] == b.im1,"ADC reset changed independent input manager");
                Require(state.boards[b.asadId]["DAC0"] == 0x81 && state.boards[b.asadId].count("DAC2") == 0,"DAC initialization wrote readonly version");
            }
            Require(state.boards[1]["ADC0"] == 0x1e && state.boards[2]["ADC0"] == 0x1e,"Inactive ADC touched");
            for (unsigned int id : {2,13,15}) Require(state.chips[id]["reg1"] == 0x3e3a41a7 && state.chips[id]["reg2"] == 0x0028a197,"Auto-delay overwrite omitted; cannot act as final Configure");
            Require(state.Get("pipeCtrl","firmwareEnable") == 0 && state.Get("pipeCtrl","intEnable") == 0 && state.Get("pipeCtrl","SCRAutoDelayEnable") == 0,"Final compute/use separation or stopping observations");
            Require(state.Get("aget_sc_configuration","SC_En") == image.agetMask && state.Get("ASAD_sc_mask","select") == 9 && state.Get("mutantConfig","initDone") == 0,"Final active masks/initDone");
            const auto journal = ReadFile(audit+"/prepare.jsonl");
            Require(journal.find("\"full_system_prepared\":false") != std::string::npos && journal.find("\"hardware_policy\":") != std::string::npos,"Prepare inflated system state or omitted bound policy");
            struct stat st; Require(stat(audit.c_str(),&st) == 0 && (st.st_mode&0777) == 0700,"Audit directory access");
            Require(stat((audit+"/prepare.jsonl").c_str(),&st) == 0 && (st.st_mode&0777) == 0600,"Audit file access");
            alarms->sendAsadAlarm("CoBo0",1,Ice::Current()); Require(client.Alarms()->Snapshot().stopRequested,"Alarm listener died after Prepare return");
            client.Alarms()->Unsubscribe(audit+"/unsubscribe.jsonl"); Require(alarms->subscribers.count("other-client") == 1,"Unsubscribe changed unrelated owner");
            Rejects([&] { client.Prepare(path()); },"single-use");
            reset(0); node->fName = "other"; CoBoPrepareClient wrong(options); Rejects([&] { wrong.Prepare(path()); },"actual node name"); Require(!state.writes && !alarms->subscribes,"Name failure mutated hardware");
            reset(0); state.hardwareVersion = 5; CoBoPrepareClient version(options); Rejects([&] { version.Prepare(path()); },"mismatch"); Require(!state.writes,"Hardware version rejected only after mutation");
            reset(0); auto missing = options; RemoveField(missing.description,"pipeCtrl","SCRAutoDelayEnable");
            CoBoPrepareClient late(missing); Rejects([&] { late.Prepare(path()); },"description"); Require(!state.writes && !alarms->subscribes,"Late missing auto-delay field allowed foundation writes");
            reset(0); node->missingService = true; CoBoPrepareClient noService(options); Rejects([&] { noService.Prepare(path()); },"null AlarmService"); Require(!state.writes,"Missing alarm service failed after power/clock mutation");
            reset(0); node->wrongService = true; CoBoPrepareClient wrongService(options); Rejects([&] { wrongService.Prepare(path()); },"implement AlarmService"); Require(!state.writes,"Wrong service type failed after mutation");
            reset(0); auto noOwner = options; noOwner.ownsStoppedHardware = false; const auto absent = path(); CoBoPrepareClient ownership(noOwner);
            Rejects([&] { ownership.Prepare(absent); },"exclusive stopped"); Require(access(absent.c_str(),F_OK) != 0 && !state.writes,"Ownership gate contacted/mutated hardware");
            reset(0); const auto existing = path(); Require(mkdir(existing.c_str(),0700) == 0,"Precreate path"); CoBoPrepareClient collision(options);
            Rejects([&] { collision.Prepare(existing); },"exclusively create"); Require(!state.writes,"Existing audit path reused");
            for (int mode : {1,2,3,4,5}) {
                reset(mode); const auto failedPath = path();
                { CoBoPrepareClient failed(options); Rejects([&] { failed.Prepare(failedPath); },(mode == 2 || mode == 3) ? "UNKNOWN" : "alarm interlock");
                  node->Join(); Require(failed.Status() == CoBoPrepareClient::Failed,"Partial Prepare reported complete");
                  Require(alarms->subscribes == 1 && !alarms->resets && !alarms->unsubscribes,"Failed operation replayed or cleaned subscription");
                  if (mode <= 2) Require(node->monitorCalls == 0 && state.firmwareStarts == 0 && state.finalThresholds == 0,"Subscribe failure continued into post-subscribe hardware");
                  if (mode == 3 || mode == 4) Require(state.firmwareStarts == 1 && state.Get("pipeCtrl","firmwareEnable") == 1 && state.Get("pipeCtrl","SCW") == 0,"Partial auto-delay command replayed/continued/cleaned up");
                  if (mode == 5) Require(!state.firmwareStarts && !state.finalThresholds,"Alarm during wait allowed subsequent threshold/calibration writes");
                  if (mode >= 3) Require(ReadFile(failedPath+"/after-subscription.jsonl").find(mode == 3 ? "\"phase\":\"unknown\"" : "\"phase\":\"interlock_stopped\"") != std::string::npos,"Missing unknown/interlock audit evidence");
                  Rejects([&] { failed.Prepare(path()); },"single-use"); }
                Require(!alarms->unsubscribes,"Destructor sent unsubscribe after partial failure");
            }
            reset(0); auto disabled = options; disabled.settings.monitoring.subscribeAlarms = false; disabled.callbackEndpoints.clear();
            disabled.settings.calibration.computeAutomaticReadDelay = false; disabled.settings.adcDoubleDataRate = false;
            CoBoPrepareClient legacy(disabled); legacy.Prepare(path()); Require(!legacy.Alarms() && !alarms->subscribes && state.firmwareStarts == 0,"Explicit unsubscribed/no-compute policy ignored");
            Require(state.boards[0]["ADC5"] == 0x403 && state.chips[2]["reg1"] == (UINT64_C(2)<<19),"SDR/no-compute final stage differs");
            reset(0); HardwareClient guarded(address,minor,1000); guarded.UseDescription(policy);
            HardwareStep masked; masked.kind = HardwareStep::MaskedWriteValue; masked.device = "ctrl"; masked.reg = "triggerMask";
            masked.field = "external"; masked.mask = 1; masked.value = 1; // 实际字段名在下方从描述查出。
            for (const auto &r : state.descriptions.at("ctrl").registers) if (r.name == "triggerMask") { Require(!r.fields.empty(),"TriggerMask fixture fields"); masked.field = r.fields.front().name; }
            state.triggerReadInterlock = true;
            Rejects([&] { guarded.Execute({masked},path(),[&] { if (state.readInterlock) throw std::runtime_error("fixture stop after read"); }); },"fixture stop after read");
            Require(!state.writes,"Alarm during RMW observation allowed following write");
            reset(0); HardwareStep delay; delay.kind = HardwareStep::Delay; delay.delayMs = 500;
            const auto start = std::chrono::steady_clock::now(); const auto guardLog = path();
            Rejects([&] { guarded.Execute({delay,masked},guardLog,[&] { if (std::chrono::steady_clock::now()-start > std::chrono::milliseconds(40)) throw std::runtime_error("fixture wait stop"); }); },"fixture wait stop");
            Require(!state.writes && ReadFile(guardLog).find("interlock_stopped") != std::string::npos,"Wait guard allowed later write or lost audit");
            std::cout << "PASS composed CoBo Prepare encoding 1." << minor << ": real 1310-register policy, final ADC, owned callbacks, preflight and unknown/interlock stop\n";
        }
        node->Join(); communicator->destroy(); return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << '\n'; return 1; }
}
