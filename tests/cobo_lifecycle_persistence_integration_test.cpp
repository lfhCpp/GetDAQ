#include "system_acquisition_fixture.h"
#include "cobolifecyclejournal.h"
#include "strictjson.h"

using namespace GetDAQAcquisitionFixture;
namespace {
CoBoLifecycleCheckpoint Checkpoint(const std::string &row) {
    // The production mirror emits the bounded checkpoint as its final member.
    const std::string key="\"lifecycle_checkpoint\":";const auto at=row.find(key);
    Require(at!=std::string::npos && row.back()=='}',"Missing durable checkpoint in operation audit");
    return DecodeCoBoLifecycleCheckpoint(row.substr(at+key.size(),row.size()-at-key.size()-1));
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN);Ice::CommunicatorPtr communicator;
    try {
        Require(argc==3,"Pass CoBo and MuTaNT description fixtures");
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator=Ice::initialize(init);
        char directory[]="/tmp/getdaq-lifecycle-integration-XXXXXX";Require(::mkdtemp(directory)!=nullptr,"Create integration directory");
        unsigned sequence=0;
        const auto path=[&] {return std::string(directory)+"/operation-"+std::to_string(sequence++);};
        for(int scenario=0;scenario<3;++scenario) {
            const auto adapter=communicator->createObjectAdapterWithEndpoints("LifecycleJournal"+std::to_string(scenario),"tcp -h 127.0.0.1 -p 0");
            Rig rig(communicator,adapter,cobo,mutant,1);
            rig.options.cobos.resize(1);rig.options.mutants.clear();
            rig.senders.erase(std::remove_if(rig.senders.begin(),rig.senders.end(),[](const DataSenderOptions &s){return s.senderId!="C0";}),rig.senders.end());
            adapter->activate();const auto preparation=path();const auto journal=preparation+"/cobo-lifecycle.jsonl";
            std::vector<std::string> operations;
            const auto operation=[&]{const auto next=path();operations.push_back(next+"/system-run.jsonl");return next;};
            std::string failedStopAudit;
            {
                SystemPreparationClient prepared(rig.options);prepared.Prepare(preparation);prepared.Configure();
                SystemAcquisitionClient run(prepared,rig.senders);run.ConnectData(operation());
                if(scenario==0) {
                    run.Start(operation());run.Pause(operation());run.Resume(operation());run.Stop(operation());run.DisconnectData(operation());
                } else if(scenario==1) {
                    {std::ofstream out(journal,std::ios::app);out<<"partial";Require(bool(out),"Inject journal corruption");}
                    rig.Clear();const auto writes=rig.w.Writes();
                    Rejects([&]{run.Start(path());},"CoBo lifecycle");
                    Require(rig.Actions().empty() && rig.w.Writes()==writes,"Journal damage permitted positive hardware command");
                    // Parent UNKNOWN authorization is explicit. A failed positive
                    // precheck must not make emergency production-stop impossible.
                    const auto stop=path();failedStopAudit=stop+"/system-run.jsonl";run.StopProductionAfterFailure(stop,true);
                    Require(rig.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count("C0.ctrl.pipeCtrl.intEnable=0")==1,
                            "Journal failure blocked independent producer stops");
                    Require(!rig.Has("C0.sender-start") && !rig.Has("C0.sender-disconnect"),"Negative stop started/disconnected sender");
                    const auto unchanged=GetDAQPrepareFixture::ReadFile(journal);
                    rig.Clear();run.StopProductionAfterFailure(path());
                    Require(rig.Actions().empty() && GetDAQPrepareFixture::ReadFile(journal)==unchanged,"Repeated emergency stop replayed commands or journal append");
                } else {
                    run.Start(path());rig.Clear();Require(::chmod(journal.c_str(),0644)==0,"Inject journal mode change");
                    const auto stop=path();failedStopAudit=stop+"/system-run.jsonl";
                    Rejects([&]{run.Stop(stop);},"CoBo lifecycle");
                    Require(rig.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count("C0.ctrl.pipeCtrl.intEnable=0")==1 &&
                            rig.Count("C0.pulser-stop")==1,"Damaged journal blocked normal producer stop");
                    Require(run.GetState()==SystemAcquisitionClient::State::Failed,"Lost journal claimed completed Run Stop");
                    Require(::chmod(journal.c_str(),0600)==0,"Restore journal mode");
                    rig.Clear();run.StopProductionAfterFailure(path());
                    Require(rig.Actions().empty(),"Stop recovery repeated already confirmed hardware commands");
                    Rejects([&]{run.Start(path());},"Ready/Stopped");
                }
                if(scenario) {
                    Require(run.CoBoLifecycleJson().find("\"state\":\"unknown\"")!=std::string::npos &&
                            prepared.CoBoLifecycleJson()==run.CoBoLifecycleJson(),"Journal failure published Stopped or diverged shared state");
                    Rejects([&]{SystemAcquisitionClient next(prepared,rig.senders);},"CoBo lifecycle");
                }
                prepared.Alarms("C0")->Unsubscribe(path()+".jsonl");
            }
            if(scenario==0) {
                std::string scope;
                CoBoLifecycleCheckpoint initial,preparedEndpoint;
                std::istringstream input(GetDAQPrepareFixture::ReadFile(preparation+"/system.jsonl"));std::string row;
                while(std::getline(input,row)) {
                    const auto value=ParseStrictJson(row);
                    if(value.At("phase").text=="cobo_lifecycle_journal") {
                        scope=value.At("scope_sha256").text;initial=Checkpoint(row);
                    }
                    if(value.At("phase").text=="cobo_lifecycle")preparedEndpoint=Checkpoint(row);
                }
                Require(!scope.empty() && initial.scopeSha256==scope && initial.transitions==0 && preparedEndpoint.transitions>=4,
                        "Preparation did not bind initial and transition checkpoints");
                const auto history=CoBoLifecycleJournal::ReadHistory(journal,scope,{{"C0",true}});
                Require(history.states==std::vector<CoBoLifecycleState>{CoBoLifecycleState::Configured} && history.transitions>=14,
                        "Prepare/acquisition did not share durable lifecycle history");
                const auto text=GetDAQPrepareFixture::ReadFile(journal);
                const auto prefixFile=std::string(directory)+"/checkpoint-prefix.jsonl";
                const auto verifyPrefix=[&](const CoBoLifecycleCheckpoint &checkpoint) {
                    Require(checkpoint.bytes<=text.size(),"Audit checkpoint extends beyond persisted journal");
                    {std::ofstream out(prefixFile,std::ios::binary|std::ios::trunc);out<<text.substr(0,checkpoint.bytes);Require(bool(out),"Write test prefix");}
                    Require(::chmod(prefixFile.c_str(),0600)==0,"Set test prefix permissions");
                    Require(CoBoLifecycleJournal::ReadHistory(prefixFile,scope,{{"C0",true}},checkpoint).terminalVerified,
                            "Persisted audit checkpoint does not match actual history prefix");
                };
                verifyPrefix(initial);verifyPrefix(preparedEndpoint);
                CoBoLifecycleCheckpoint finalEndpoint;unsigned checkpoints=0;
                for(const auto &audit:operations) {
                    std::istringstream operationInput(GetDAQPrepareFixture::ReadFile(audit));
                    while(std::getline(operationInput,row)) {
                        const auto value=ParseStrictJson(row);
                        if(value.At("phase").text!="cobo_lifecycle")continue;
                        const auto checkpoint=Checkpoint(row);verifyPrefix(checkpoint);finalEndpoint=checkpoint;++checkpoints;
                    }
                }
                Require(checkpoints>=10 && finalEndpoint.transitions==history.transitions && finalEndpoint.bytes==text.size(),
                        "Acquisition audit does not retain the final shared journal checkpoint");
                Require(CoBoLifecycleJournal::ReadHistory(journal,scope,{{"C0",true}},finalEndpoint).terminalVerified,
                        "Independent operation checkpoint did not verify final history");
                Rejects([&]{CoBoLifecycleJournal::ReadHistory(journal,scope,{{"C0",true}},preparedEndpoint);},"checkpoint mismatch");
                // Lose one complete, otherwise valid final transition after the
                // writer has exited; independent operation evidence must catch it.
                const auto prefix=text.substr(0,text.rfind('\n',text.size()-2)+1);
                {std::ofstream out(journal,std::ios::binary|std::ios::trunc);out<<prefix;Require(bool(out),"Truncate final complete record");}
                Require(CoBoLifecycleJournal::ReadHistory(journal,scope,{{"C0",true}}).transitions+1==history.transitions,
                        "Truncated complete prefix fixture is not structurally valid");
                Rejects([&]{CoBoLifecycleJournal::ReadHistory(journal,scope,{{"C0",true}},finalEndpoint);},"checkpoint mismatch");
                {std::ofstream out(journal,std::ios::binary|std::ios::trunc);out<<text;Require(bool(out),"Restore test history");}
                Require(::unlink(prefixFile.c_str())==0,"Remove test prefix");
                for(const auto stage:{"preparing","configuring","ready","starting","running","paused","resuming","stopping","stopped"})
                    Require(text.find(std::string("\"state\":\"")+stage+"\"")!=std::string::npos,"Missing cross-workflow state evidence");
            } else {
                std::istringstream stopInput(GetDAQPrepareFixture::ReadFile(failedStopAudit));std::string row;unsigned unavailable=0;
                while(std::getline(stopInput,row)) {
                    const auto value=ParseStrictJson(row);
                    if(value.At("phase").text!="cobo_lifecycle")continue;
                    Require(value.At("lifecycle_persisted").type==JsonValue::Boolean && value.At("lifecycle_persisted").text=="false" &&
                            value.At("lifecycle_checkpoint").type==JsonValue::Null && value.At("state").text=="unknown",
                            "Damaged lifecycle journal exported a durable negative-stop checkpoint");++unavailable;
                }
                Require(unavailable>=2,"Independent negative stops did not record absent checkpoints");
            }
            adapter->deactivate();adapter->waitForDeactivate();adapter->destroy();
            std::cout<<"PASS lifecycle persistence integration scenario "<<scenario<<std::endl;
        }
        communicator->destroy();return EXIT_SUCCESS;
    } catch(const std::exception &e) {std::cerr<<e.what()<<'\n';if(communicator)communicator->destroy();return EXIT_FAILURE;}
}
