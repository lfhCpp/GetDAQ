#include "system_acquisition_fixture.h"
namespace GetDAQ {
struct HardwareSystemAcquisitionTestAccess {
    static std::shared_ptr<CoBoLifecycleLedger> Lifecycle(SystemPreparationClient &prepared) {
        return prepared.CoBoLifecycleLedgerShared();
    }
};
}
using namespace GetDAQAcquisitionFixture;
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN); Ice::CommunicatorPtr communicator;
    try {
        Require(argc==3 || argc==4,"Pass actual CoBo and MuTaNT description fixtures, optional evidence index");
        std::ofstream index; if (argc==4) { index.open(argv[3]); Require(bool(index),"Create acquisition evidence index"); }
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator=Ice::initialize(init);
        char directory[]="/tmp/getdaq-system-acquisition-XXXXXX"; Require(mkdtemp(directory)!=nullptr,"Create run fixture directory");
        unsigned int sequence=0; const auto path=[&] { return std::string(directory)+"/operation-"+std::to_string(sequence++); };
        for (int minor:{0,1}) {
            auto adapter=communicator->createObjectAdapterWithEndpoints("SystemAcquisition"+std::to_string(minor),"tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,minor); adapter->activate();
            SystemPreparationClient prepared(r.options);
            Rejects([&] { SystemAcquisitionClient run(prepared,r.senders); },"same Configured");
            prepared.Prepare(path()); prepared.Configure(); r.Clear();
            const auto baseline=[&] {
                r.Baseline();
                const auto ledger=GetDAQ::HardwareSystemAcquisitionTestAccess::Lifecycle(prepared);
                for(std::size_t i=0;i<r.options.cobos.size();++i) if(r.options.cobos[i].enabled) {
                    const auto state=ledger->StateAt(i);
                    if(state==CoBoLifecycleState::Unknown) {
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Prepared);
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Configuring);
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Configured);
                    } else if(state==CoBoLifecycleState::Ready || state==CoBoLifecycleState::Stopped) {
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Configured);
                    } else if(state==CoBoLifecycleState::Paused) {
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Stopped);
                        ledger->Transition(r.options.cobos[i].id,i,CoBoLifecycleState::Configured);
                    } else if(state!=CoBoLifecycleState::Configured) {
                        throw std::runtime_error("Test fixture cannot reconcile unexpected CoBo lifecycle state");
                    }
                }
                ledger->RequireConfiguredForAcquisition();
            };
            auto bad=r.senders; bad[0].circularBufferMask=9; Rejects([&] { SystemAcquisitionClient run(prepared,bad); },"mask/encoding");
            bad=r.senders; bad[0].senderId="C2"; Rejects([&] { SystemAcquisitionClient run(prepared,bad); },"mask/encoding");
            bad=r.senders; bad.pop_back(); Rejects([&] { SystemAcquisitionClient run(prepared,bad); },"mask/encoding");
            bad=r.senders; bad[0].encodingMinor=1-minor; Rejects([&] { SystemAcquisitionClient run(prepared,bad); },"mask/encoding");
            Require(r.Actions().empty(),"Local construction/invalid binding mutated hardware");
            { SystemAcquisitionClient run(prepared,r.senders); const auto before=r.w.Writes(); r.sender["M1"]->valid=false;
              Rejects([&] { run.ConnectData(path()); },"does not implement mdaq::DaqCtrlNode"); Require(r.w.Writes()==before && r.Actions().empty(),"Last sender interface failure left changes"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path());
              Require(run.GetState()==SystemAcquisitionClient::State::Ready && run.SenderStatus("M1").state==DataSenderSnapshot::Connected,"Data link readiness");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"ready\"")!=std::string::npos,"Connect did not expose per-CoBo ready state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"ready\"")!=std::string::npos,"Preparation view did not observe shared ready state");
              r.Before("C0.sender-connect","C1.sender-connect"); r.Before("C1.sender-connect","M0.sender-connect"); r.Before("M0.sender-connect","M1.sender-connect");
              const auto prior=r.Actions().size(); const auto existing=path(); mkdir(existing.c_str(),0700);
              Rejects([&] { run.Start(existing); },"create"); Require(run.GetState()==SystemAcquisitionClient::State::Ready && r.Actions().size()==prior,"Existing audit changed ready state/hardware");
              r.Clear(); const auto start=path(); run.Start(start); r.Index(index,start,"start"); r.StartOrder(false); Require(run.GetState()==SystemAcquisitionClient::State::Running,"Start state");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"running\"")!=std::string::npos,"Run start did not expose per-CoBo running state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"running\"")!=std::string::npos,"Preparation view did not observe shared running state");
              Rejects([&] { run.DisconnectData(path()); },"Ready/Stopped");
              r.Clear(); const auto pause=path(); run.Pause(pause); r.Index(index,pause,"pause"); r.StopOrder(); Require(run.GetState()==SystemAcquisitionClient::State::Paused,"Pause state");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"paused\"")!=std::string::npos,"Pause did not expose per-CoBo paused state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"paused\"")!=std::string::npos,"Preparation view did not observe shared paused state");
              r.Clear(); const auto resume=path(); run.Resume(resume); r.Index(index,resume,"resume"); r.StartOrder(true);
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"running\"")!=std::string::npos,"Resume did not restore per-CoBo running state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"running\"")!=std::string::npos,"Preparation view did not observe resumed shared state");
              r.Clear(); const auto stop=path(); run.Stop(stop); r.Index(index,stop,"stop"); r.StopOrder(); Require(run.GetState()==SystemAcquisitionClient::State::Stopped,"Stop state");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"stopped\"")!=std::string::npos,"Run stop did not expose per-CoBo stopped state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"stopped\"")!=std::string::npos,"Preparation view did not observe shared stopped state");
              r.Clear(); const auto audit=path(); run.DisconnectData(audit); Require(run.GetState()==SystemAcquisitionClient::State::Closed,"Closed state");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"configured\"")!=std::string::npos,"Disconnect did not restore per-CoBo configured state");
              Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"configured\"")!=std::string::npos,"Preparation view did not observe shared configured state");
              Require(r.Actions().size()==4 && r.Count("M1.sender-disconnect")==1,"Unused slave sender not disconnected exactly once");
              const auto text=GetDAQPrepareFixture::ReadFile(audit+"/system-run.jsonl"); Require(text.find("\"receiver_sealed\":false")!=std::string::npos,"Control closure overclaimed receiver seal");
            }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear();
              auto &remote=r.devices[1]["ctrl"]->fConfig; const auto saved=remote;
              for (auto &reg:remote.registers) if (reg.descr.name=="aget_sc_configuration") reg.fields.pop_back();
              const auto before=r.w.Writes(); Rejects([&] { run.Start(path()); },"bound description");
              Require(r.w.Writes()==before && r.Actions().empty(),"Late start metadata failure left early RunInit writes"); remote=saved;
              Rejects([&] { run.Start(path()); },"Ready"); Require(r.Actions().empty(),"Failed outcome restarted"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear();
              auto &start=r.w.c[1].ctrl["memStart_asad3"]; const auto saved=start; ++start; const auto before=r.w.Writes();
              Rejects([&] { run.Start(path()); },"mismatch"); Require(r.w.Writes()==before && r.Actions().empty(),"Changed buffer was accepted or left RunInit writes"); start=saved; }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear(); r.sender["C0"]->failStart=true;
              Rejects([&] { run.Start(path()); },"UNKNOWN"); Require(run.GetState()==SystemAcquisitionClient::State::Failed && run.SenderStatus("C0").outcomeUnknown,"Start ACK loss did not retain unknown");
              Require(r.Count("C0.sender-start")==1 && !r.Has("C0.ctrl.pipeCtrl.firmwareEnable=1") && !r.Has("C1.sender-start") && !r.Has("M1.CARD_B_EMC/TRIGGER_CTRL/run=1"),"Start ACK loss retried/continued");
              const auto count=r.Actions().size(); Rejects([&] { run.Stop(path()); },"recovery"); Require(r.Actions().size()==count,"Failed recovery silently replayed stop");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos,"Start ACK loss did not mark affected CoBo unknown");
              const auto lifecycleBeforeRejectedRun=prepared.CoBoLifecycleJson();
              Rejects([&] { SystemAcquisitionClient replacement(prepared,r.senders); },"must be Configured/Disabled");
              Require(prepared.CoBoLifecycleJson()==lifecycleBeforeRejectedRun,"new acquisition erased an unknown board outcome");
              r.Clear(); run.StopProductionAfterFailure(path());
              Require(run.GetState()==SystemAcquisitionClient::State::Failed && run.SenderStatus("C0").outcomeUnknown && r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && !r.Has("C0.sender-stop"),"Explicit producer stop reconciled/cleaned unknown sender");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"stopped\"")!=std::string::npos,"Confirmed negative hardware stop did not update per-CoBo state");
              r.Clear(); run.StopProductionAfterFailure(path()); Require(r.Actions().empty(),"Repeated failure-stop replayed producer commands"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear(); r.nodes[0]->failPulserStart=true;
              Rejects([&] { run.Start(path()); },"UNKNOWN"); Require(run.GetState()==SystemAcquisitionClient::State::Failed && r.Count("C0.pulser-start")==1 && !r.Has("C1.sender-start"),"Pulser ACK loss swallowed/replayed"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear(); r.w.c[0].mode=3;
              Rejects([&] { run.Start(path()); },"UNKNOWN");
              Require(r.w.c[0].Get("pipeCtrl","firmwareEnable")==1 && !r.Has("C1.sender-start"),"Firmware enable ACK loss continued start");
              r.Clear(); run.StopProductionAfterFailure(path());
              Require(r.w.c[0].Get("pipeCtrl","firmwareEnable")==0 && run.SenderStatus("C0").state==DataSenderSnapshot::Running && !r.Has("C0.sender-stop"),"Failure-stop did not quiesce production/retained sender"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); run.Start(path()); r.Clear(); r.devices[0]["ctrl"]->failFirmwareStop=true;
              Rejects([&] { run.Stop(path()); },"producer stop incomplete");
              Require(r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && r.Has("C0.ctrl.pipeCtrl.intEnable=0") && r.Has("C1.ctrl.pipeCtrl.firmwareEnable=0"),"Stop ACK loss retried or skipped independent producer stops");
              Require(run.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                      run.CoBoLifecycleJson().find("\"id\":\"C1\",\"state\":\"stopped\"")!=std::string::npos,
                      "Partial producer stop did not isolate unknown and confirmed CoBo states");
              Require(!r.Has("C0.sender-stop") && !r.Has("M0.sender-stop") && !r.Has("C0.sender-disconnect"),"Unconfirmed production stop flushed/closed senders");
              r.Clear(); r.devices[0]["ctrl"]->failFirmwareStop=false;
              Rejects([&] { run.StopProductionAfterFailure(path()); },"stop unconfirmed"); Require(r.Actions().empty(),"Unknown negative stop command was replayed after service recovered"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); run.Start(path()); r.Clear(); r.nodes[0]->failPulserStop=true;
              Rejects([&] { run.Stop(path()); },"producer stop incomplete"); Require(r.Has("C0.ctrl.pipeCtrl.firmwareEnable=0") && r.Has("C1.ctrl.pipeCtrl.intEnable=0") && !r.Has("C1.sender-stop"),"Pulser stop failure aborted independent stop or flushed"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); run.Start(path()); r.Clear(); r.sender["C0"]->failStop=true;
              Rejects([&] { run.Stop(path()); },"sender flush incomplete"); r.StopOrder();
              Require(run.SenderStatus("C0").outcomeUnknown && run.SenderStatus("C1").state==DataSenderSnapshot::Stopped && r.Count("C0.sender-stop")==1,"Flush failure retried or skipped other sender");
              Rejects([&] { run.DisconnectData(path()); },"Ready/Stopped"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); run.Start(path()); run.Pause(path()); r.Clear(); const auto before=r.w.Writes();
              run.Stop(path()); Require(r.Actions().empty() && r.w.Writes()==before,"Stop from Paused repeated hardware/sender stop");
              r.sender["C1"]->failDisconnect=true; Rejects([&] { run.DisconnectData(path()); },"UNKNOWN");
              Require(r.Count("C0.sender-disconnect")==1 && r.Count("C1.sender-disconnect")==1 && !r.Has("M0.sender-disconnect"),"Disconnect ACK loss retried/continued"); }
            baseline();
            { SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); run.Start(path()); r.Clear();
              r.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); Rejects([&] { prepared.CheckInterlock(); },"alarm");
              run.Stop(path()); r.StopOrder(); Require(run.GetState()==SystemAcquisitionClient::State::Stopped,"Latched alarm blocked explicit Stop");
              run.DisconnectData(path()); }
            std::cout<<"PASS system acquisition encoding 1."<<minor<<": same-object preparation/configuration, run order, pause/resume, global preflight, unknown outcomes and explicit alarm Stop\n";
            for (unsigned int i=0;i<2;++i) prepared.Alarms("C"+std::to_string(i))->Unsubscribe(path()+".jsonl");
            adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy();
        }
        for (bool shared:{true,false}) {
            auto adapter=communicator->createObjectAdapterWithEndpoints(shared ? "StandaloneRun" : "DisabledMasterTriggerRun","tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,1); r.options.cobos.resize(1); r.options.mutants.resize(shared ? 0 : 1);
            r.options.cobos[0].configure.asad.generator["periodicModeEnabled"]="false";
            if (shared) {
                r.options.cobos[0].configure.readout.bufferLayout=CoBoReadoutSettings::SharedBuffer;
                r.options.cobos[0].prepare.settings.foundation.bufferLayout=CoBoReadoutSettings::SharedBuffer;
            } else {
                auto &m=r.options.mutants[0].settings;
                m.level0Enabled=m.level1Enabled=m.level2Enabled=m.clearScalers=m.clearCounters=false;
            }
            r.senders.erase(std::remove_if(r.senders.begin(),r.senders.end(),[&](const DataSenderOptions &s) { return s.senderId!="C0" && (shared || s.senderId!="M0"); }),r.senders.end());
            adapter->activate(); SystemPreparationClient prepared(r.options); prepared.Prepare(path()); prepared.Configure();
            SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path()); r.Clear(); const auto start=path(); run.Start(start); r.Index(index,start,"start");
            Require(!r.Has("M0.sender-start") && !r.Has("M0.CARD_B_EMC/TRIGGER_CTRL/run=1") && !r.Has("C0.pulser-start"),"Disabled master trigger/periodic pulser started");
            if (shared) {
                Require(r.Has("C0.ctrl.readPtr=134217728"),"Shared read pointer omitted");
                for (const auto &s:r.Actions()) Require(s.find("ctrl.readPtr_asad")==std::string::npos,"Shared layout wrote per-AsAd pointer");
            } else for (const auto &s:r.Actions()) Require(s.find("/TRIGGER_CMD/")==std::string::npos && s.find("/SCALER_CTRL/cs")==std::string::npos,"Disabled RunInit counters executed");
            run.Pause(path()); r.Clear(); const auto resume=path(); run.Resume(resume); r.Index(index,resume,"resume"); Require(!r.Has("C0.ctrl.resetTime=0"),"Alternate-mode Resume reset counters");
            run.Stop(path()); run.DisconnectData(path()); prepared.Alarms("C0")->Unsubscribe(path()+".jsonl");
            std::cout<<"PASS "<<(shared ? "standalone CoBo shared buffer" : "master trigger/counter initialization disabled")<<" run and resume\n";
            adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy();
        }
        communicator->destroy(); return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; if (communicator) communicator->destroy(); return 1; }
}
