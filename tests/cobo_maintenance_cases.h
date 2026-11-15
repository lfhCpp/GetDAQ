#ifndef GETDAQ_COBO_MAINTENANCE_CASES_H
#define GETDAQ_COBO_MAINTENANCE_CASES_H
#include "system_preparation_fixture.h"
namespace GetDAQMaintenanceFixture {
using namespace GetDAQSystemFixture;
inline void Run(World &w,const SystemPreparationOptions &base,
                const std::array<IceUtil::Handle<ConfigureNode>,3> &nodes,
                const std::function<std::string()> &path)
{
    unsigned int cases=0;
    const auto close=[&](SystemPreparationClient &client,const std::string &root) {
        for (unsigned int i=0;i<2;++i) {
            auto *alarm=client.Alarms("C"+std::to_string(i));
            if (alarm && alarm->Snapshot().subscription==CoBoAlarmSnapshot::Active)
                alarm->Unsubscribe(root+"/close-"+std::to_string(i)+".jsonl");
        }
    };
    for (int minor:{0,1}) {
        w.Reset();auto options=base;options.encodingMinor=minor;
        SystemPreparationClient client(options);const auto root=path();client.Prepare(root);
        auto *otherAlarm=client.Alarms("C1");
        const auto otherCtrl=w.c[1].ctrl;const auto otherBoards=w.c[1].boards;const auto otherChips=w.c[1].chips;
        const auto targetCalibrationStarts=w.c[0].firmwareStarts,otherCalibrationStarts=w.c[1].firmwareStarts;
        const auto dormantCtrl=w.c[2].ctrl;const auto otherWrites=w.c[1].writes;const auto dormantWrites=w.c[2].writes;
        const auto links=w.links.load(),tdc=w.tdc.load(),multi=w.multi.load();const auto result=client.AlignmentResult();
        std::array<std::map<std::string,std::uint64_t>,3> mutantValues;
        std::array<std::size_t,3> mutantWrites;
        for (unsigned int i=0;i<3;++i) {mutantValues[i]=w.m[i].values;mutantWrites[i]=w.m[i].writes.size();}
        const auto before=w.Writes();
        Rejects([&] {client.ReprepareCoBo("absent",path(),true);},"selected active");
        Rejects([&] {client.ReprepareCoBo("C2",path(),true);},"selected active");
        Rejects([&] {client.ReprepareCoBo("C0",path(),false);},"freshly confirmed");
        Require(w.Writes()==before && w.alarms[0]->unsubscribes==0,"Rejected maintenance changed hardware or subscription");
        const auto maintenance=path();client.ReprepareCoBo("C0",maintenance,true);
        Require(client.GetState()==SystemPreparationClient::State::Prepared,"Maintenance did not retain Prepared");
        Require(w.c[0].writes>0 && w.c[0].powered==options.cobos[0].prepare.settings.inputs.asadMask,"Selected CoBo was not fully prepared");
        Require(w.c[1].writes==otherWrites && w.c[2].writes==dormantWrites && w.c[1].ctrl==otherCtrl &&
                w.c[1].boards==otherBoards && w.c[1].chips==otherChips && w.c[2].ctrl==dormantCtrl,"Maintenance changed another CoBo");
        Require(w.links==links && w.tdc==tdc && w.multi==multi,"Maintenance repeated system alignment");
        for (unsigned int i=0;i<3;++i)
            Require(w.m[i].values==mutantValues[i] && w.m[i].writes.size()==mutantWrites[i],"Maintenance changed MuTaNT");
        Require(client.Alarms("C1")==otherAlarm && w.alarms[1]->subscribes==1 && w.alarms[1]->unsubscribes==0,"Maintenance replaced another alarm owner");
        const auto beforeSingleConfigure=w.Writes(),otherConfigureWrites=w.c[1].writes,dormantConfigureWrites=w.c[2].writes;
        const auto mutantConfigureWrites=w.m[0].writes.size()+w.m[1].writes.size()+w.m[2].writes.size();
        Rejects([&] {client.ConfigureCoBo("C0",path(),false);},"freshly confirmed");
        Rejects([&] {client.ConfigureCoBo("C2",path(),true);},"selected active");
        Require(w.Writes()==beforeSingleConfigure,"Rejected single-CoBo Configure changed hardware");
        const auto singleConfigure=path();client.ConfigureCoBo("C0",singleConfigure,true);
        Require(client.GetState()==SystemPreparationClient::State::Prepared && w.c[0].writes>0 &&
                w.c[1].writes==otherConfigureWrites && w.c[2].writes==dormantConfigureWrites,
                "Single-CoBo Configure changed the parent state or touched another CoBo");
        Require(w.m[0].writes.size()+w.m[1].writes.size()+w.m[2].writes.size()==mutantConfigureWrites &&
                w.c[0].Get("mutantConfig","initDone")==0 && w.c[0].Get("pipeCtrl","firmwareEnable")==0 &&
                w.c[0].Get("pipeCtrl","intEnable")==0,
                "Single-CoBo Configure wrote MuTaNT/readiness or started board production");
        const auto configuredLifecycleWrites=w.Writes();
        Rejects([&] {client.ConfigureCoBo("C0",path(),true);},"repeated Configure is rejected");
        Require(w.Writes()==configuredLifecycleWrites &&
                client.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"configured\"")!=std::string::npos,
                "Single-CoBo lifecycle repeated Configure or lost its configured state");
        const auto singleConfigureAudit=GetDAQPrepareFixture::ReadFile(singleConfigure+"/configure-cobo.jsonl");
        Require(singleConfigureAudit.find("\"phase\":\"complete\"")!=std::string::npos &&
                singleConfigureAudit.find("\"mutant_configure_sent\":false")!=std::string::npos &&
                singleConfigureAudit.find("\"system_ready_claimed\":false")!=std::string::npos,
                "Single-CoBo Configure audit omitted its limited scope");
        const auto invalidPulserWrites=w.Writes();
        const auto targetPulserCalls=w.pulserCalls[0],otherPulserCalls=w.pulserCalls[1],dormantPulserCalls=w.pulserCalls[2];
        Rejects([&] {client.TriggerAsAdPulser("C0",1001,path());},"-1000..1000");
        Rejects([&] {client.TriggerAsAdPulser("C2",100,path());},"active target");
        Require(w.Writes()==invalidPulserWrites && w.pulserCalls[0]==targetPulserCalls && w.pulserCalls[1]==otherPulserCalls,
                "Rejected pulser maintenance wrote hardware or called a manager");
        const auto pulserAudit=path();client.TriggerAsAdPulser("C0",321,pulserAudit);
        Require(client.GetState()==SystemPreparationClient::State::Prepared && w.pulserCalls[0]==targetPulserCalls+1 &&
                w.pulserCalls[1]==otherPulserCalls && w.pulserCalls[2]==dormantPulserCalls && w.pulserVoltages[0]==321,
                "Single pulser action did not call exactly the selected board once at the reviewed voltage");
        const auto pulserIntent=GetDAQPrepareFixture::ReadFile(pulserAudit+"/pulser.jsonl");
        Require(pulserIntent.find("\"voltage_mv\":321")!=std::string::npos &&
                pulserIntent.find("\"physical_pulse_verified\":false")!=std::string::npos,
                "Pulser maintenance audit omitted voltage or overstated physical verification");
        const auto callsAfterPulser=w.pulserCalls;
        Rejects([&] {client.TriggerAsAdPulser("C0",321,pulserAudit);},"exists");
        Require(w.pulserCalls==callsAfterPulser,"Reused pulser audit path triggered a second pulse");
        Require(w.alarms[0]->subscribes==2 && w.alarms[0]->unsubscribes==1 && w.alarms[0]->resets==0,"Selected alarm ownership was not explicitly replaced");
        const auto after=client.AlignmentResult();
        Require(after.located.size()==result.located.size() && after.crates.size()==result.crates.size() && after.interCrate.size()==result.interCrate.size(),"Saved alignment changed shape");
        for (std::size_t i=0;i<result.crates.size();++i)
            Require(after.crates[i].gmcShelfMask==result.crates[i].gmcShelfMask && after.crates[i].wscaShelfMask==result.crates[i].wscaShelfMask,"Saved alignment mask changed");
        const auto text=GetDAQPrepareFixture::ReadFile(maintenance+"/maintenance.jsonl");
        Require(text.find("\"phase\":\"complete\"")!=std::string::npos && text.find("\"physical_alignment_verified\":false")!=std::string::npos,"Maintenance audit missing or overclaims physical result");
        // Explicit subsequent all-system Configure remains available. Maintenance itself starts nothing.
        // The original automatic delay calibration itself pulses firmwareEnable.
        // It must be repeated exactly for the selected board, then left stopped.
        Require(w.c[0].firmwareStarts==2*targetCalibrationStarts && w.c[1].firmwareStarts==otherCalibrationStarts &&
                w.c[0].Get("pipeCtrl","firmwareEnable")==0 && w.c[0].Get("pipeCtrl","intEnable")==0,
                "Maintenance did not retain the stopped calibrated state");
        client.Configure();Require(client.GetState()==SystemPreparationClient::State::Configured,"Explicit Configure after maintenance failed");
        const auto writes=w.Writes();Rejects([&] {client.ReprepareCoBo("C0",path(),true);},"retained Prepared");
        Require(w.Writes()==writes,"Configured maintenance rejection changed hardware");close(client,root);
        ++cases;std::cout<<"PASS selected CoBo maintenance and explicit Configure encoding 1."<<minor<<std::endl;
    }
    // A lost acknowledgement after any selected-board maintenance write makes
    // only that board UNKNOWN; the parent fails closed and never replays it.
    const std::array<std::string,5> uncertainNames{{"SetCoBoLed","BlinkCoBoLeds","FlipCoBoLed","PulseCoBoLed","TriggerAsAdPulser"}};
    for (std::size_t operation=0;operation<uncertainNames.size();++operation) {
        w.Reset();SystemPreparationClient client(base);const auto root=path();client.Prepare(root);
        const auto audit=path();const auto pulsesBefore=w.pulserCalls[0];
        const auto ledCallsBefore=nodes[0]->ledCalls.load(),blinkCallsBefore=nodes[0]->blinkCalls.load(),
                   flipCallsBefore=nodes[0]->flipCalls.load(),flipAllCallsBefore=nodes[0]->flipAllCalls.load(),
                   pulseCallsBefore=nodes[0]->pulseCalls.load();
        if (operation==4) w.failPulserAfterTrigger=true;
        else w.failLedAfterCommand=true;
        bool failed=false;
        try {
            switch (operation) {
            case 0: client.SetCoBoLed("C0",0,HardwareStep::LedP,HardwareStep::LedOn,audit);break;
            case 1: client.BlinkCoBoLeds("C0",audit);break;
            case 2: client.FlipCoBoLed("C0",0,audit);break;
            case 3: client.PulseCoBoLed("C0",0,10,audit);break;
            case 4: client.TriggerAsAdPulser("C0",321,audit);break;
            }
        } catch (const std::exception &) {failed=true;}
        const auto lifecycle=client.CoBoLifecycleJson();
        Require(failed && client.GetState()==SystemPreparationClient::State::Failed,("Uncertain maintenance acknowledgement did not fail closed: "+uncertainNames[operation]).c_str());
        Require(lifecycle.find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                lifecycle.find("\"id\":\"C1\",\"state\":\"prepared\"")!=std::string::npos &&
                lifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                ("Uncertain maintenance changed the wrong CoBo lifecycle: "+uncertainNames[operation]).c_str());
        const auto writesAfter=w.Writes();const auto pulsesAfter=w.pulserCalls[0];
        if(operation==4)Require(pulsesAfter>pulsesBefore,"Pulser failure injection did not reach the selected trigger");
        const auto ledCallsAfter=nodes[0]->ledCalls.load(),blinkCallsAfter=nodes[0]->blinkCalls.load(),
                   flipCallsAfter=nodes[0]->flipCalls.load(),flipAllCallsAfter=nodes[0]->flipAllCalls.load(),
                   pulseCallsAfter=nodes[0]->pulseCalls.load();
        if(operation<4) {
            const bool reached=operation==0 ? ledCallsAfter>ledCallsBefore : operation==1 ? blinkCallsAfter>blinkCallsBefore :
                operation==2 ? (flipCallsAfter>flipCallsBefore || flipAllCallsAfter>flipAllCallsBefore) : pulseCallsAfter>pulseCallsBefore;
            Require(reached,("Failure injection did not reach the selected LED service: "+uncertainNames[operation]).c_str());
        }
        bool retryRejected=false;
        try {
            switch (operation) {
            case 0: client.SetCoBoLed("C0",0,HardwareStep::LedP,HardwareStep::LedOn,audit);break;
            case 1: client.BlinkCoBoLeds("C0",audit);break;
            case 2: client.FlipCoBoLed("C0",0,audit);break;
            case 3: client.PulseCoBoLed("C0",0,10,audit);break;
            case 4: client.TriggerAsAdPulser("C0",321,audit);break;
            }
        } catch (const std::exception &) {retryRejected=true;}
        Require(retryRejected && w.Writes()==writesAfter && w.pulserCalls[0]==pulsesAfter &&
                nodes[0]->ledCalls.load()==ledCallsAfter && nodes[0]->blinkCalls.load()==blinkCallsAfter &&
                nodes[0]->flipCalls.load()==flipCallsAfter && nodes[0]->flipAllCalls.load()==flipAllCallsAfter &&
                nodes[0]->pulseCalls.load()==pulseCallsAfter,
                ("Failed maintenance was automatically retried: "+uncertainNames[operation]).c_str());
        close(client,root);++cases;
    }
    // All-node stopped/name observations must fail before touching the target subscription.
    for (int fault:{0,1}) {
        w.Reset();SystemPreparationClient client(base);const auto root=path();client.Prepare(root);const auto before=w.Writes();
        if (!fault) nodes[1]->fName="changed-other-node";
        else w.c[2].Set("pipeCtrl","firmwareEnable",1);
        Rejects([&] {client.ReprepareCoBo("C0",path(),true);},fault ? "production stopped" : "name changed");
        if (!fault) nodes[1]->fName="CoBo1";
        Require(w.Writes()==before && w.alarms[0]->unsubscribes==0 && client.GetState()==SystemPreparationClient::State::Failed,"Failed global preflight changed target or cleared hold");
        close(client,root);++cases;
    }
    // Applied-but-unacknowledged unsubscribe or subscribe is not automatically replayed.
    for (int mode:{3,2}) {
        w.Reset();auto options=base;options.encodingMinor=1;SystemPreparationClient client(options);const auto root=path();client.Prepare(root);
        const auto before=w.Writes();w.alarms[0]->mode=mode;
        Rejects([&] {client.ReprepareCoBo("C0",path(),true);},"UNKNOWN");
        const auto writes=w.Writes(),subscribes=w.alarms[0]->subscribes,unsubscribes=w.alarms[0]->unsubscribes;
        Require(client.GetState()==SystemPreparationClient::State::Failed && unsubscribes==1 && w.alarms[0]->resets==0,"Unknown subscription outcome was not held");
        if (mode==3) Require(writes==before && subscribes==1,"Unknown unsubscribe allowed reprepare");
        else Require(subscribes==2 && writes>before,"New subscription failure did not follow selected board preparation");
        Rejects([&] {client.ReprepareCoBo("C0",path(),true);},"retained Prepared");
        Require(w.Writes()==writes && w.alarms[0]->subscribes==subscribes && w.alarms[0]->unsubscribes==unsubscribes,"Failed maintenance was automatically repeated");
        Require(w.alarms[1]->subscribes==1 && w.alarms[1]->unsubscribes==0,"Failure changed unrelated subscription");
        close(client,root);++cases;
    }
    std::cout<<"PASS "<<cases<<" native selected CoBo maintenance cases; no receiver/raw; physical alignment unverified"<<std::endl;
}
}
#endif
