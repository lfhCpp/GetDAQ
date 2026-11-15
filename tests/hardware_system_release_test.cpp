#include "system_acquisition_fixture.h"
namespace {
using namespace GetDAQAcquisitionFixture;
void Index(std::ofstream &index,const std::string &directory,int minor,const std::string &phase,const std::string &from="") {
    if (index.is_open()) {
        index<<"{\"directory\":"<<JsonString(directory)<<",\"encoding_minor\":"<<minor<<",\"phase\":"<<JsonString(phase)<<",\"from\":"<<JsonString(from)<<"}\n";
        index.flush(); Require(bool(index),"Save release evidence index");
    }
}
void Broken(Rig &r,SystemPreparationClient &prepared,SystemAcquisitionClient &run,const std::string &from) {
    Require(run.GetState()==SystemAcquisitionClient::State::Broken && prepared.GetState()==SystemPreparationClient::State::Prepared,"Breakup did not retain Prepared system");
    for (const auto &id:{"C0","C1","M0","M1"}) {
        Require(run.SenderStatus(id).state==DataSenderSnapshot::Disconnected,"Breakup left an owned link");
        Require(r.Count(std::string(id)+".sender-disconnect")==unsigned(from!="closed"),"Disconnect replayed/skipped");
    }
    const auto a=r.Actions(); std::vector<std::string> flags;
    for (const auto &s:a) if (s.find("mutantConfig.initDone=0")!=std::string::npos || s.find("mutantConfig.mutantMode=0")!=std::string::npos) flags.push_back(s);
    Require(flags==std::vector<std::string>({"C0.ctrl.mutantConfig.initDone=0","C1.ctrl.mutantConfig.initDone=0","C2.ctrl.mutantConfig.initDone=0",
        "C0.ctrl.mutantConfig.mutantMode=0","C1.ctrl.mutantConfig.mutantMode=0","C2.ctrl.mutantConfig.mutantMode=0"}),"Breakup lost all-initDone then all-mutantMode order");
    if (from!="closed") r.Before("M1.sender-disconnect","C0.ctrl.mutantConfig.initDone=0");
    if (from=="running" || from=="alarm-running") { r.StopOrder(false); r.Before("C1.sender-stop","C0.sender-disconnect"); }
    else for (const auto &id:{"C0","C1","M0","M1"}) Require(!r.Has(std::string(id)+".sender-stop"),"Breakup redundantly stopped/flushed sender");
    for (unsigned int i=0;i<3;++i) {
        Require(r.w.c[i].Get("mutantConfig","initDone")==0 && r.w.c[i].Get("mutantConfig","mutantMode")==0,"Breakup status readback differs");
        Require(r.w.c[i].Get("mutantConfig","l2Trigger")==unsigned(i<2),"Breakup unexpectedly erased Level2 configuration");
    }
    Require(prepared.AlignmentResult().crates[0].gmcShelfMask==1 && prepared.AlignmentResult().crates[2].gmcShelfMask==2048,"Breakup lost saved alignment snapshot");
}
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc==3 || argc==4 || argc==6,"Pass actual CoBo/MuTaNT fixtures, optional evidence index and --scenario normal-0/normal-1/fault-0..6");
        int selectedMinor=-1,selectedFault=-1; bool normals=true,faults=true;
        if (argc==6) {
            Require(std::string(argv[4])=="--scenario","Expected --scenario");
            const std::string scenario=argv[5];
            if (scenario.size()==8 && scenario.compare(0,7,"normal-")==0 && (scenario[7]=='0' || scenario[7]=='1')) {
                selectedMinor=scenario[7]-'0'; faults=false;
            } else if (scenario.size()==7 && scenario.compare(0,6,"fault-")==0 && scenario[6]>='0' && scenario[6]<='6') {
                selectedFault=scenario[6]-'0'; normals=false;
            } else throw std::invalid_argument("Unknown release scenario");
        }
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        std::ofstream index; if (argc>=4) { index.open(argv[3]); Require(bool(index),"Create release index"); }
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator=Ice::initialize(init);
        char temporary[]="/tmp/getdaq-system-release-XXXXXX"; Require(mkdtemp(temporary),"Create release test directory");
        unsigned int sequence=0; const auto path=[&] { return std::string(temporary)+"/operation-"+std::to_string(sequence++); };
        for (int minor:{0,1}) {
            if (!normals || (selectedMinor>=0 && selectedMinor!=minor)) continue;
            auto adapter=communicator->createObjectAdapterWithEndpoints("SystemRelease"+std::to_string(minor),"tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,minor); adapter->activate();
            SystemPreparationClient prepared(r.options); prepared.Prepare(path());
            std::array<std::string,2> owner{{prepared.Alarms("C0")->SubscriberId(),prepared.Alarms("C1")->SubscriberId()}};
            for (unsigned int i=0;i<2;++i) { std::lock_guard<std::mutex> lock(r.w.alarms[i]->mutex); r.w.alarms[i]->subscribers["foreign-owner"]=nullptr; }
            for (const auto &from:{"ready","running","paused","stopped","closed","alarm-running"}) {
                prepared.Configure(); SystemAcquisitionClient run(prepared,r.senders); run.ConnectData(path());
                const auto before=r.w.Writes(); Rejects([&] { prepared.Unprepare(path(),true); },"requires Prepared"); Require(r.w.Writes()==before,"Configured Unprepare sent power-off");
                if (std::string(from)!="ready" && std::string(from)!="closed") run.Start(path());
                if (std::string(from)=="paused") run.Pause(path());
                if (std::string(from)=="stopped") run.Stop(path());
                if (std::string(from)=="closed") run.DisconnectData(path());
                if (std::string(from)=="alarm-running") r.w.alarms[0]->sendAsadAlarm("CoBo0",8,Ice::Current());
                r.Clear(); const auto audit=path(); run.Breakup(audit); Index(index,audit,minor,"breakup",from); Broken(r,prepared,run,from);
                const auto changes=r.w.Writes(); const auto actions=r.Actions().size();
                Rejects([&] { run.Breakup(path()); },"requires Ready"); Rejects([&] { run.Start(path()); },"requires Ready");
                Require(r.w.Writes()==changes && r.Actions().size()==actions,"Broken run replayed");
                for (unsigned int i=0;i<2;++i) {
                    Require(prepared.Alarms("C"+std::to_string(i))->SubscriberId()==owner[i] && r.w.alarms[i]->unsubscribes==0,"Breakup replaced/closed alarm owner");
                }
                const auto root=GetDAQPrepareFixture::ReadFile(audit+"/system-run.jsonl");
                Require(root.find("\"receiver_sealed\":false")!=std::string::npos && root.find("broken-prepare-snapshot-retained")!=std::string::npos,"Breakup inflated receiver completion");
            }
            Rejects([&] { prepared.CheckInterlock(); },"alarm interlock"); // Breakup不能清掉保持告警。
            Require(prepared.GetState()==SystemPreparationClient::State::Prepared,"Interlock inspection consumed Prepared state");
            const auto chainAudit=path(); prepared.Unprepare(chainAudit,true); Index(index,chainAudit,minor,"unprepare","after-breakup");
            Require(prepared.GetState()==SystemPreparationClient::State::Unprepared,"Same owned preparation did not release after Breakup");
            Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"fresh\"")!=std::string::npos &&
                    prepared.CoBoLifecycleJson().find("\"id\":\"C1\",\"state\":\"fresh\"")!=std::string::npos &&
                    prepared.CoBoLifecycleJson().find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Successful Unprepare did not reset active CoBo lifecycle to Fresh");
            for (unsigned int i=0;i<2;++i) Require(prepared.Alarms("C"+std::to_string(i))->Snapshot().subscription==CoBoAlarmSnapshot::Closed &&
                r.w.alarms[i]->subscribers.count("foreign-owner")==1,"Breakup-to-Unprepare lost subscription ownership");
            // 另一个真实Prepared对象验证仅Prepare后撤销及已变化的实际供电集合。
            adapter->destroy();
            auto resetAdapter=communicator->createObjectAdapterWithEndpoints("SystemReset"+std::to_string(minor),"tcp -h 127.0.0.1 -p 0");
            Rig reset(communicator,resetAdapter,cobo,mutant,minor); resetAdapter->activate();
            SystemPreparationClient p(reset.options); p.Prepare(path());
            for (unsigned int i=0;i<2;++i) { std::lock_guard<std::mutex> lock(reset.w.alarms[i]->mutex); reset.w.alarms[i]->subscribers["foreign-owner"]=nullptr; }
            reset.w.c[0].powered=11; reset.w.c[1].powered=0; reset.w.c[2].powered=2; reset.devices[0].at("asad")->monitoringStatus=8;
            reset.w.alarms[0]->sendAsadAlarm("CoBo0",8,Ice::Current()); reset.Clear();
            const auto writes=reset.w.Writes(); Rejects([&] { p.Unprepare(path(),false); },"fresh stopped"); Require(reset.w.Writes()==writes,"Missing new data-stopped declaration sent writes");
            reset.nodes[2]->fName="changed";
            Rejects([&] { p.Unprepare(path(),true); },"name changed"); Require(reset.w.Writes()==writes,"Late node name error modified earlier board");
            reset.nodes[2]->fName="CoBo2"; reset.w.alarms[1]->valid=false;
            Rejects([&] { p.Unprepare(path(),true); },"no longer implements"); Require(reset.w.Writes()==writes,"Late alarm service error sent power-off");
            reset.w.alarms[1]->valid=true;
            const auto existing=path(); Require(mkdir(existing.c_str(),0700)==0,"Create collision directory");
            Rejects([&] { p.Unprepare(existing,true); },"exclusively create"); Require(p.GetState()==SystemPreparationClient::State::Prepared && reset.w.Writes()==writes,"Collision consumed Unprepare");
            const auto audit=path(); p.Unprepare(audit,true); Index(index,audit,minor,"unprepare");
            Require(p.GetState()==SystemPreparationClient::State::Unprepared,"Unprepare did not complete");
            Require(p.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"fresh\"")!=std::string::npos &&
                    p.CoBoLifecycleJson().find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Standalone Unprepare did not reset active CoBo lifecycle to Fresh");
            for (unsigned int i=0;i<3;++i) {
                Require(reset.w.c[i].powered==0 && reset.Count("C"+std::to_string(i)+".ctrl.asadEnable=0")==1,"Unprepare skipped/repeated power-off, including inactive declared board");
                if (i<2) Require(p.Alarms("C"+std::to_string(i))->Snapshot().subscription==CoBoAlarmSnapshot::Closed &&
                    reset.w.alarms[i]->subscribers.count("foreign-owner")==1 && reset.w.alarms[i]->unsubscribes==1 && reset.w.alarms[i]->resets==0,"Unprepare reset/removed foreign owner");
            }
            const auto final=reset.w.Writes(); Rejects([&] { p.Unprepare(path(),true); },"requires Prepared"); Rejects([&] { p.Prepare(path()); },"single-use");
            Require(reset.w.Writes()==final,"Released preparation reused");
            std::cout<<"PASS system Breakup/Unprepare TCP 1."<<minor<<": all run boundaries, retained snapshots/owners, reconfigure cycles, actual powered mask, inactive board, alarm-latched negative release and fresh data-stopped gate\n";
            resetAdapter->destroy();
        }
        // 故障场景使用两块独立CoBo，仍真实执行完整Prepare；不伪造成功阶段。
        for (unsigned int failure=0;failure<7;++failure) {
            if (!faults || (selectedFault>=0 && unsigned(selectedFault)!=failure)) continue;
            auto adapter=communicator->createObjectAdapterWithEndpoints("ReleaseFault"+std::to_string(failure),"tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,1); r.options.mutants.clear(); r.options.cobos.resize(2);
            r.senders.erase(std::remove_if(r.senders.begin(),r.senders.end(),[](const DataSenderOptions &s) { return s.senderId!="C0" && s.senderId!="C1"; }),r.senders.end());
            if (failure==6) for (auto &c:r.options.cobos) c.prepare.settings.monitoring.subscribeAlarms=false;
            adapter->activate(); SystemPreparationClient p(r.options); p.Prepare(path());
            if (failure<2 || failure==5) {
                p.Configure(); SystemAcquisitionClient run(p,r.senders); run.ConnectData(path());
                if (failure==5) { run.Start(path()); r.devices[0].at("ctrl")->failFirmwareStop=true; }
                if (failure==0) r.sender["C0"]->failDisconnect=true;
                if (failure==1) r.devices[0].at("ctrl")->failStatusClear=true;
                r.Clear(); Rejects([&] { run.Breakup(path()); },failure==5 ? "producer stop incomplete" : "UNKNOWN");
                Require(run.GetState()==SystemAcquisitionClient::State::Failed,"Unknown Breakup did not latch failure");
                if (failure==0) Require(!r.Has("C1.sender-disconnect") && !r.Has("C0.ctrl.mutantConfig.initDone=0") && p.GetState()==SystemPreparationClient::State::Configured,"Disconnect unknown continued status invalidation");
                if (failure==1) Require(r.Has("C1.sender-disconnect") && r.Count("C0.ctrl.mutantConfig.initDone=0")==1 && !r.Has("C1.ctrl.mutantConfig.initDone=0") && p.GetState()==SystemPreparationClient::State::Failed,"Status unknown replayed/continued");
                if (failure==5) Require(!r.Has("C0.sender-stop") && !r.Has("C0.sender-disconnect") && !r.Has("C0.ctrl.mutantConfig.initDone=0") && r.Has("C1.ctrl.pipeCtrl.firmwareEnable=0"),"Production stop unknown continued sender/state release");
                const auto writes=r.w.Writes(); const auto actions=r.Actions().size(); Rejects([&] { run.Breakup(path()); },"requires Ready");
                Require(r.w.Writes()==writes && r.Actions().size()==actions,"Failed Breakup replayed"); Rejects([&] { p.Unprepare(path(),true); },"requires Prepared");
            } else {
                if (failure==2) r.devices[0].at("ctrl")->failPowerOff=true;
                if (failure==3) r.devices[0].at("asad")->failMonitoring=true;
                if (failure==4) r.w.alarms[0]->mode=3;
                r.Clear();
                if (failure==6) {
                    p.Unprepare(path(),true); Require(!p.Alarms("C0") && p.GetState()==SystemPreparationClient::State::Unprepared && r.w.alarms[0]->unsubscribes==0,"Disabled subscription acquired/closed an owner");
                } else {
                    Rejects([&] { p.Unprepare(path(),true); },failure==4 ? "UNKNOWN" : "incomplete");
                    Require(p.GetState()==SystemPreparationClient::State::Failed && r.w.c[0].powered==0 && r.w.c[1].powered==0 &&
                        r.Count("C0.ctrl.asadEnable=0")==1 && r.Count("C1.ctrl.asadEnable=0")==1,"Release failed to attempt independent board power-off exactly once");
                    Require(p.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                            p.CoBoLifecycleJson().find("\"id\":\"C1\",\"state\":\"unknown\"")!=std::string::npos,
                            "Failed Unprepare did not mark touched CoBos unknown");
                    if (failure==4) Require(p.Alarms("C0")->Snapshot().subscription==CoBoAlarmSnapshot::Unknown && r.w.alarms[0]->unsubscribes==1 && r.w.alarms[1]->unsubscribes==0,"Unknown unsubscribe replayed/continued");
                    else Require(r.w.alarms[0]->unsubscribes==0 && r.w.alarms[1]->unsubscribes==0,"Incomplete observation/power-off discarded alarm owners");
                    const auto before=r.w.Writes(); Rejects([&] { p.Unprepare(path(),true); },"requires Prepared"); Require(r.w.Writes()==before,"Failed Unprepare repeated power-off");
                }
            }
            adapter->destroy();
        }
        if (faults) std::cout<<"PASS system release fault scenario "<<selectedFault<<": complete preparation, expected fault/release assertions and no recovery by replay\n";
        communicator->destroy(); return 0;
    } catch (const std::exception &e) {
        std::cerr<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1;
    }
}
