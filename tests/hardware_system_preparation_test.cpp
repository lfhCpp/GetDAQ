#include "system_preparation_fixture.h"
#include "cobo_maintenance_cases.h"
using namespace GetDAQSystemFixture;
int main(int argc,char **argv)
{
    Ice::CommunicatorPtr communicator;
    try {
        const bool artifactMode=argc==5 && std::string(argv[3])=="--artifact";
        const bool maintenanceMode=argc==4 && std::string(argv[3])=="--maintenance";
        const bool lifecyclePreflightMode=argc==4 && std::string(argv[3])=="--lifecycle-preflight";
        Require(argc == 3 || argc == 4 || artifactMode,"Pass actual description fixtures, optional audit index, --maintenance or --lifecycle-preflight");
        std::ofstream index;
        if (argc == 4 && !maintenanceMode) { index.open(argv[3]); Require(bool(index),"Create system audit index"); }
        const auto cobo = LoadHardwareDescription(argv[1]), mutant = LoadHardwareDescription(argv[2]);
        Ice::InitializationData init; init.properties = Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator = Ice::initialize(init);
        auto adapter = communicator->createObjectAdapterWithEndpoints("SystemPreparationTest","tcp -h 127.0.0.1 -p 0");
        World w; SystemPreparationOptions base; base.timeoutMs = 1000; base.exclusiveOwnerConfirmed = base.dataProcessesStopped = true;
        std::array<IceUtil::Handle<ConfigureNode>,3> nodes;
        for (unsigned int i = 0; i < 3; ++i) {
            auto &state = w.m[i]; state.onAction = [&,i](const std::string &s) { w.Record("M"+std::to_string(i)+'.'+s); };
            IceUtil::Handle<GetDAQTest::TestNode> node = new GetDAQTest::TestNode; node->fName = "MuTaNT"+std::to_string(i);
            for (const auto &d : mutant.devices) node->fDevices[d.name] = mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(new GetDAQMutantFixture::MutantDevice(d,state)));
            SystemMutantSettings m; m.endpoint.id = "M"+std::to_string(i); m.endpoint.expectedNodeName = node->fName;
            m.endpoint.nodeProxy = communicator->proxyToString(adapter->addWithUUID(node)); m.endpoint.master = i == 0;
            m.endpoint.alignmentProxy = communicator->proxyToString(adapter->addWithUUID(new Alignment(w,i))); m.endpoint.description = mutant;
            m.settings.isSlave = i != 0; m.settings.slaveId = i == 2; m.settings.slavePortA = i != 2;
            if (i) m.settings.level0Enabled = m.settings.level1Enabled = m.settings.level2Enabled = false;
            m.settings.amcTagOnly = 1<<5; base.mutants.push_back(m);
        }
        for (unsigned int i = 0; i < 3; ++i) {
            auto &state = w.c[i]; for (const auto &d : cobo.devices) state.descriptions[d.name] = d;
            state.onAction = [&,i](const std::string &s) { w.Record("C"+std::to_string(i)+'.'+s); };
            w.alarms[i] = new GetDAQPrepareFixture::PrepareAlarmService;
            nodes[i] = new ConfigureNode(*w.alarms[i],w,i);
            nodes[i]->service = get::mt::AlarmServicePrx::uncheckedCast(adapter->addWithUUID(w.alarms[i]));
            for (const auto &d : cobo.devices) nodes[i]->fDevices[d.name] = mdaq::hw::DevicePrx::uncheckedCast(adapter->addWithUUID(new GetDAQPrepareFixture::PrepareDevice(d,state)));
            SystemCoBoSettings c; c.id = "C"+std::to_string(i); c.enabled = i != 2;
            c.prepare.nodeProxy = communicator->proxyToString(adapter->addWithUUID(nodes[i])); c.prepare.expectedNodeName = nodes[i]->fName;
            c.linkProxy = communicator->proxyToString(adapter->addWithUUID(new Link(w,i))); c.prepare.description = cobo;
            c.prepare.settings = GetDAQPrepareFixture::Settings(); c.prepare.callbackEndpoints = "tcp -h 127.0.0.1 -p 0";
            c.configure = ConfigureSettings(); base.cobos.push_back(c);
        }
        adapter->activate(); w.Reset();
        char directory[] = "/tmp/getdaq-system-preparation-XXXXXX"; Require(mkdtemp(directory) != nullptr,"Create system fixture directory");
        unsigned int count = 0; const auto path = [&] { return std::string(directory)+"/system-"+std::to_string(count++); };
        const auto noWrites = [&] { Require(w.Writes() == 0 && w.tdc == 0 && w.multi == 0 && w.links == 0,"Global preflight failure left hardware changes"); };
        if (lifecyclePreflightMode) {
            const auto audit=path();SystemPreparationClient x(base);x.Prepare(audit);
            // A stale production bit fails the all-node stopped precondition
            // after metadata validation but before the first Configure write.
            w.c[1].Set("pipeCtrl","firmwareEnable",1);const auto before=w.Writes();
            Rejects([&] {x.Configure();},"expectation mismatch");
            const auto lifecycle=x.CoBoLifecycleJson();
            Require(x.GetState()==SystemPreparationClient::State::Failed && w.Writes()==before,
                    "Precondition failure wrote hardware or failed to hold the parent coordinator");
            Require(lifecycle.find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                    lifecycle.find("\"id\":\"C1\",\"state\":\"prepared\"")!=std::string::npos &&
                    lifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Precondition-only Configure failure changed per-CoBo lifecycle");
            Require(w.c[0].Get("pipeCtrl","firmwareEnable")==0 &&
                    w.c[0].Get("pipeCtrl","intEnable")==0 &&
                    w.c[1].Get("pipeCtrl","firmwareEnable")==1 &&
                    w.c[1].Get("pipeCtrl","intEnable")==0,
                    "Precondition-only Configure failure changed a CoBo production-state bit");
            for(unsigned int i=0;i<3;++i)
                Require(w.c[i].Get("mutantConfig","initDone")==0,
                        "Precondition-only Configure failure changed CoBo readiness");
            for(unsigned int i=0;i<2;++i)x.Alarms("C"+std::to_string(i))->Unsubscribe(audit+"/preflight-close-"+std::to_string(i)+".jsonl");
            communicator->destroy();std::cout<<"PASS Configure precondition failure preserves per-CoBo Prepared/disabled stages before any write\n";return 0;
        }
        if (artifactMode) {
            const auto a=LoadSystemExperimentArtifact(argv[4]);
            Require(a.settings.cobos.size()<=3 && a.settings.mutants.size()<=3,"Artifact exceeds local fixture nodes");
            auto endpoints=base; endpoints.cobos.resize(a.settings.cobos.size()); endpoints.mutants.resize(a.settings.mutants.size());
            for (std::size_t i=0; i<endpoints.cobos.size(); ++i) { endpoints.cobos[i].id=a.settings.cobos[i].id; endpoints.cobos[i].enabled=a.settings.cobos[i].enabled; }
            for (std::size_t i=0; i<endpoints.mutants.size(); ++i) { endpoints.mutants[i].endpoint.id=a.settings.mutants[i].id; endpoints.mutants[i].endpoint.master=!a.settings.mutants[i].settings.isSlave; }
            auto reversed=endpoints; std::reverse(reversed.cobos.begin(),reversed.cobos.end()); std::reverse(reversed.mutants.begin(),reversed.mutants.end());
            const auto ordered=BindSystemExperiment(a,reversed);
            Require(ordered.cobos[0].id==a.settings.cobos[0].id && ordered.cobos[0].prepare.nodeProxy==endpoints.cobos[0].prepare.nodeProxy,"Binding changed artifact order or endpoint identity");
            for (int minor:{0,1}) {
                w.Reset(); endpoints.encodingMinor=minor;
                for (std::size_t i=0; i<a.settings.cobos.size(); ++i) for (const auto &entry:a.settings.cobos[i].prepare.calibration.chipVersions)
                    w.c[i].chips[entry.first]["reg5"]=UINT64_C(0xa500000000000000)|entry.second;
                const auto bound=BindSystemExperiment(a,endpoints); noWrites(); const auto audit=path();
                SystemPreparationClient client(bound); client.Prepare(audit); client.Configure();
                Require(client.GetState()==SystemPreparationClient::State::Configured,"Loaded artifact did not reach native Configure");
                const auto text=GetDAQPrepareFixture::ReadFile(audit+"/system.jsonl");
                Require(text.find(a.configurationSha256)!=std::string::npos && text.find(a.artifactSha256)!=std::string::npos && text.find("\"actual_loaded_configuration_verified\":false")!=std::string::npos,"Audit did not bind received artifact without inflating physical state");
                for (std::size_t i=0; i<bound.cobos.size(); ++i) {
                    const auto &c=bound.cobos[i]; if (!c.enabled) { Require(w.c[i].powered==0,"Disabled board was powered"); continue; }
                    const auto image=CompileCoBoConfigure(c.configure);
                    Require(w.c[i].Get("mutantConfig","initDone")==1 && w.c[i].ctrl["readDepth"]==image.readout.aget.readoutDepth,"Artifact readout parameters were not applied");
                    Require(w.c[i].Get("aget_sc_configuration","SC_En")==image.readout.aget.agetMask,"Artifact AGET activity was not applied");
                    if (client.Alarms(c.id)) client.Alarms(c.id)->Unsubscribe(audit+"/unsubscribe-"+std::to_string(i)+".jsonl");
                }
                std::cout<<"PASS compiled experiment artifact -> bound native Prepare/Configure, encoding 1."<<minor<<", audit "<<audit<<'\n';
            }
            communicator->destroy(); return 0;
        }
        if (maintenanceMode) {
            GetDAQMaintenanceFixture::Run(w,base,nodes,path);
            communicator->destroy();return 0;
        }
        auto bad = base; bad.mutants[2].settings.slaveId = 0; Rejects([&] { SystemPreparationClient x(bad); },"unique matching");
        bad = base; bad.cobos[1].configure.readout.aget.chips[0].settings.chipVersion = 0x202;
        Rejects([&] { SystemPreparationClient x(bad); },"chip version mismatch");
        bad = base; bad.cobos[1].configure.readout.aget.chips.pop_back(); bad.cobos[1].configure.readout.chips.pop_back();
        Rejects([&] { SystemPreparationClient x(bad); },"activity");
        for (int minor : {0,1}) {
            auto options = base; options.encodingMinor = minor;
            w.Reset(); nodes[1]->wrongService = true;
            { SystemPreparationClient x(options); Rejects([&] { x.Prepare(path()); },"implement AlarmService"); noWrites(); }
            nodes[1]->wrongService = false;
            w.Reset(); auto missing = options; GetDAQPrepareFixture::RemoveField(missing.cobos[1].prepare.description,"mutantConfig","l2Trigger");
            { SystemPreparationClient x(missing); Rejects([&] { x.Prepare(path()); },"description"); noWrites(); }
            w.Reset(); auto duplicate = options; duplicate.cobos[1].prepare.nodeProxy = options.cobos[0].prepare.nodeProxy;
            Rejects([&] { SystemPreparationClient x(duplicate); x.Prepare(path()); },"Duplicate"); noWrites();
            w.Reset(); w.c[2].Set("pipeCtrl","firmwareEnable",1);
            { SystemPreparationClient x(options); Rejects([&] { x.Prepare(path()); },"mismatch"); noWrites(); }
            w.Reset(); const auto audit = path(); SystemPreparationClient x(options); x.Prepare(audit);
            Require(x.GetState() == SystemPreparationClient::State::Prepared,"System Prepare did not terminate successfully");
            Require(x.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                    x.CoBoLifecycleJson().find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Per-CoBo lifecycle did not retain prepared/disabled state");
            Require(w.multi == 24 && w.tdc == 18 && w.c[2].writes == 0,"System skipped alignment or initialized inactive CoBo");
            Require(x.AlignmentResult().crates[0].gmcShelfMask == 1 && x.AlignmentResult().crates[2].gmcShelfMask == 2048,"Saved in-crate masks lost");
            const auto before = w.Writes(); options.cobos[0].configure.readout.module["triggerDelay"] = "1"; // 已构造的计划不会随外部改变。
            x.Configure(); Require(x.GetState() == SystemPreparationClient::State::Configured && w.Writes() > before,"System Configure did not execute");
            const auto lifecycle=x.CoBoLifecycleJson();
            Require(lifecycle.find("\"id\":\"C0\",\"state\":\"configured\"")!=std::string::npos &&
                    lifecycle.find("\"id\":\"C1\",\"state\":\"configured\"")!=std::string::npos &&
                    lifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Per-CoBo lifecycle did not retain configured/disabled state");
            if (index) index << "{\"directory\":" << JsonString(audit) << ",\"mutants\":3,\"cobos\":3,\"active\":3,\"level2\":true}\n";
            for (unsigned int i = 0; i < 3; ++i) {
                Require(w.c[i].Get("mutantConfig","initDone") == unsigned(i != 2),"Inactive initDone or active readiness differs");
                Require(w.c[i].Get("mutantConfig","mutantMode") == unsigned(i != 2),"Mutant mode policy differs");
                Require(w.c[i].Get("mutantConfig","l2Trigger") == unsigned(i != 2),"Level2 differs from actual master");
                Require(w.m[i].values["CARD_B_EMC/SHELF_MASK"] == (i == 0 ? 1u : i == 2 ? 2048u : 0u),"Configure used post-multi masks or included tag outputs");
                Require(w.m[i].values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG1"] == (std::uint64_t((i == 0 ? 1u : i == 2 ? 2048u : 0u)|32)<<12 | ((i == 0 ? 1u : i == 2 ? 2048u : 0u)|32)),"Restored clocks/tag-only outputs differ");
            }
            Require(w.c[0].ctrl["triggerDelay"] == 800,"Copied Configure plan changed externally");
            const auto c1Configure = w.Find("C1.configure-begin"), mode = w.Find("C0.ctrl.mutantConfig.mutantMode=1");
            Require(c1Configure < mode,"mutantMode set before all AsAd Configure");
            std::size_t lastHigh = 0, firstLow = w.actions.size();
            for (unsigned int i = 0; i < 3; ++i) {
                const auto high = w.Find("M"+std::to_string(i)+".GPIO_CLK_TREE/CLOCK_TREE_CONFIG2/start_pulse=1");
                const auto low = w.Find("M"+std::to_string(i)+".GPIO_CLK_TREE/CLOCK_TREE_CONFIG2/start_pulse=0");
                Require(mode < high && low < w.Find("C0.ctrl.mutantConfig.initDone=1"),"Sync pulse crossed mode/initDone boundary");
                lastHigh = std::max(lastHigh,high); firstLow = std::min(firstLow,low);
            }
            Require(lastHigh < firstLow,"Old all-high then all-low pulse order lost");
            Require(w.Find("C1.ctrl.mutantConfig.initDone=1") < w.Find("M0.CARD_B_EMC/SHELF_MASK/=1"),"MuTaNT Configure occurred before all CoBo readiness");
            for (unsigned int i = 0; i < 2; ++i) Require(x.Alarms("C"+std::to_string(i))->Snapshot().subscription == CoBoAlarmSnapshot::Active,"Alarm owner not retained across Configure");
            Require(!x.Alarms("C2"),"Inactive board subscribed");
            const auto text = GetDAQPrepareFixture::ReadFile(audit+"/system.jsonl");
            Require(text.find("global_preflight_complete") != std::string::npos && text.find("\"production_ecc_state_committed\":false") != std::string::npos,"Audit inflated production state");
            Rejects([&] { x.Configure(); },"cannot be replayed");
            w.alarms[0]->sendAsadAlarm("CoBo0",8,Ice::Current()); Rejects([&] { x.CheckInterlock(); },"alarm interlock");
            for (unsigned int i = 0; i < 2; ++i) x.Alarms("C"+std::to_string(i))->Unsubscribe(audit+"/unsubscribe-"+std::to_string(i)+".jsonl");
            std::cout << "PASS full three-MuTaNT/two-CoBo Prepare+Configure encoding 1." << minor << ": global preflight, snapshots, sync boundary, retained alarms\n";
        }
        // 已准备的C0在C1第一项写入时告警；本地检查必须阻止C1后续动作，而非仅检查C1。
        w.Reset(); bool sent = false;
        w.c[1].onAction = [&](const std::string &s) { w.Record("C1."+s); if (!sent) { sent = true; w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); } };
        { SystemPreparationClient x(base); Rejects([&] { x.Prepare(path()); },"alarm interlock");
          Require(x.GetState() == SystemPreparationClient::State::Failed && w.c[1].writes == 1 && w.alarms[1]->subscribes == 0,"Cross-board alarm allowed subsequent Prepare writes");
          Require(w.alarms[0]->unsubscribes == 0,"Partial failure silently unsubscribed"); }
        w.c[1].onAction = [&](const std::string &s) { w.Record("C1."+s); };
        w.Reset();
        { SystemPreparationClient x(base); x.Prepare(path()); const auto before = w.Writes(); nodes[1]->fName = "wrong-after-prepare";
          Rejects([&] { x.Configure(); },"actual node name changed");
          Require(w.Writes() == before,"Late Configure name failure allowed previous board mutation");
          Require(x.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                  x.CoBoLifecycleJson().find("\"id\":\"C1\",\"state\":\"prepared\"")!=std::string::npos,
                  "Preflight-only Configure failure changed per-board hardware lifecycle"); }
        nodes[1]->fName = "CoBo1";
        w.Reset();
        { SystemPreparationClient x(base); const auto audit = path(); x.Prepare(audit); w.m[0].failure = 3;
          Rejects([&] { x.Configure(); },"UNKNOWN");
          Require(w.m[0].failedWrites == 1,"Unknown sync pulse was retried");
          Require(std::count(w.actions.begin(),w.actions.end(),"M0.GPIO_CLK_TREE/CLOCK_TREE_CONFIG2/start_pulse=1") == 1 &&
                  std::count(w.actions.begin(),w.actions.end(),"M1.GPIO_CLK_TREE/CLOCK_TREE_CONFIG2/start_pulse=1") == 0 &&
                  std::count(w.actions.begin(),w.actions.end(),"M0.GPIO_CLK_TREE/CLOCK_TREE_CONFIG2/start_pulse=0") == 0,"Unknown sync continued to later pulse or cleanup");
          Require(w.c[0].Get("mutantConfig","initDone") == 0 && w.c[1].Get("mutantConfig","initDone") == 0,"Unknown sync pulse set readiness");
          Require(w.m[0].values["GPIO_CLK_TREE/CLOCK_TREE_CONFIG2"]%16 < 8,"Unknown sync auto-restored CLK2");
          Require(w.alarms[0]->unsubscribes == 0 && x.GetState() == SystemPreparationClient::State::Failed,"Unknown failure cleaned up or committed");
          Require(x.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                  x.CoBoLifecycleJson().find("\"id\":\"C1\",\"state\":\"unknown\"")!=std::string::npos,
                  "Unconfirmed system Configure outcome did not mark affected CoBos unknown"); }
        // 无MuTaNT的原始独立运行模式仍执行完整单板Prepare/Configure；没有伪造对齐结果。
        w.Reset(); auto standalone = base; standalone.mutants.clear(); standalone.cobos.resize(1);
        { SystemPreparationClient x(standalone); const auto audit = path(); x.Prepare(audit); x.Configure();
          if (index) index << "{\"directory\":" << JsonString(audit) << ",\"mutants\":0,\"cobos\":1,\"active\":1,\"level2\":false}\n";
          Require(x.AlignmentResult().crates.empty() && w.c[0].Get("mutantConfig","mutantMode") == 0 && w.tdc == 0 && w.multi == 0,"Standalone mode performed MuTaNT actions"); }
        w.Reset();
        { SystemPreparationClient x(standalone); x.Prepare(path()); const auto before = w.c[0].writes; w.alarmDuringConfigure = true;
          Rejects([&] { x.Configure(); },"alarm interlock");
          Require(w.c[0].writes == before && x.GetState() == SystemPreparationClient::State::Failed,"Alarm during Configure allowed subsequent register writes"); }
        communicator->destroy();
        if (index) { index.flush(); Require(bool(index),"Flush system audit index"); }
        std::cout << "PASS cross-board alarm, unknown sync no replay/cleanup, and standalone mode\n";
        return 0;
    } catch (const std::exception &e) { if (communicator) communicator->destroy(); std::cerr << e.what() << '\n'; return 1; }
}
