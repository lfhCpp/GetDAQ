#include "system_acquisition_fixture.h"
using namespace GetDAQAcquisitionFixture;
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        const bool invalidationFailure=argc==4 && std::string(argv[3])=="--invalidation-failure";
        const bool invalidationSuccess=argc==4 && std::string(argv[3])=="--invalidation-success";
        const bool invalidationCase=invalidationFailure || invalidationSuccess;
        Require(argc==3 || invalidationCase,"Pass CoBo/MuTaNT descriptions and optional --invalidation-success/--invalidation-failure");
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.ThreadPool.Server.Size","4");
        communicator=Ice::initialize(init);
        char directory[]="/tmp/getdaq-broken-stop-XXXXXX";Require(::mkdtemp(directory),"Create audit-only fixture root");
        unsigned int sequence=0;const auto path=[&] {return std::string(directory)+"/operation-"+std::to_string(sequence++);};
        if(invalidationCase) {
            auto adapter=communicator->createObjectAdapterWithEndpoints("InvalidationFailure","tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,0);adapter->activate();
            SystemPreparationClient prepared(r.options);prepared.Prepare(path());prepared.Configure();
            SystemAcquisitionClient run(prepared,r.senders);run.ConnectData(path());
            if(invalidationSuccess) {
                run.Breakup(path());
                const auto preparationLifecycle=prepared.CoBoLifecycleJson(),runLifecycle=run.CoBoLifecycleJson();
                Require(run.GetState()==SystemAcquisitionClient::State::Broken && prepared.GetState()==SystemPreparationClient::State::Prepared &&
                        preparationLifecycle.find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                        preparationLifecycle.find("\"id\":\"C1\",\"state\":\"prepared\"")!=std::string::npos &&
                        preparationLifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C1\",\"state\":\"prepared\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                        "Successful Breakup invalidation did not return active CoBos to Prepared");
            } else {
                r.w.c[1].onAction=[](const std::string &) {throw std::runtime_error("injected invalidation acknowledgement loss");};
                bool failed=false;try{run.Breakup(path());}catch(const std::exception &){failed=true;}
                const auto preparationLifecycle=prepared.CoBoLifecycleJson(),runLifecycle=run.CoBoLifecycleJson();
                Require(failed && run.GetState()==SystemAcquisitionClient::State::Failed && prepared.GetState()==SystemPreparationClient::State::Failed,
                        "Partial invalidation did not fail both retained coordinators");
                Require(preparationLifecycle.find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                        preparationLifecycle.find("\"id\":\"C1\",\"state\":\"unknown\"")!=std::string::npos &&
                        preparationLifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C0\",\"state\":\"unknown\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C1\",\"state\":\"unknown\"")!=std::string::npos &&
                        runLifecycle.find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                        "Partial Breakup invalidation did not preserve the per-CoBo UNKNOWN projection");
            }
            for(unsigned int i=0;i<2;++i)if(prepared.Alarms("C"+std::to_string(i)))prepared.Alarms("C"+std::to_string(i))->Unsubscribe(path()+".jsonl");
            adapter->deactivate();adapter->waitForDeactivate();adapter->destroy();communicator->destroy();
            std::cout<<"PASS Breakup invalidation lifecycle "<<(invalidationSuccess ? "success" : "partial failure")<<std::endl;return 0;
        }
        for(int minor:{0,1}) for(int fault:{0,1,2}) {
            auto adapter=communicator->createObjectAdapterWithEndpoints("BrokenStop"+std::to_string(sequence),"tcp -h 127.0.0.1 -p 0");
            Rig r(communicator,adapter,cobo,mutant,minor);adapter->activate();
            SystemPreparationClient prepared(r.options);prepared.Prepare(path());prepared.Configure();
            SystemAcquisitionClient run(prepared,r.senders);run.ConnectData(path());
            if(fault!=2) {run.Start(path());run.Stop(path());}
            run.Breakup(path());Require(run.GetState()==SystemAcquisitionClient::State::Broken,"Breakup did not finish");
            Require(prepared.CoBoLifecycleJson().find("\"id\":\"C0\",\"state\":\"prepared\"")!=std::string::npos &&
                    prepared.CoBoLifecycleJson().find("\"id\":\"C2\",\"state\":\"disabled\"")!=std::string::npos,
                    "Successful Breakup invalidation did not return active CoBos to Prepared");
            Rejects([&] {run.StopProductionAfterFailure(path());},"confirmed UNKNOWN");
            if(fault==1) {std::lock_guard<std::mutex> lock(r.w.c[1].mutex);r.w.c[1].Set("pipeCtrl","firmwareEnable",1);}
            r.Clear();const auto writes=r.w.Writes();
            if(!fault) {
                run.StopProductionAfterFailure(path(),true);
                Require(run.GetState()==SystemAcquisitionClient::State::Failed && prepared.GetState()==SystemPreparationClient::State::Prepared,"Readback cleared failure or changed preparation");
                run.StopProductionAfterFailure(path(),true);
            } else {
                Rejects([&] {run.StopProductionAfterFailure(path(),true);},fault==1 ? "expectation mismatch" : "retained successful producer stop");
                if(fault==1) {std::lock_guard<std::mutex> lock(r.w.c[1].mutex);r.w.c[1].Set("pipeCtrl","firmwareEnable",0);}
                Rejects([&] {run.StopProductionAfterFailure(path(),true);},"verification failed");
            }
            Require(r.Actions().empty() && r.w.Writes()==writes,"Broken stop verification replayed a write/sender command");
            for(unsigned int i=0;i<2;++i)prepared.Alarms("C"+std::to_string(i))->Unsubscribe(path()+".jsonl");
            adapter->deactivate();adapter->waitForDeactivate();adapter->destroy();
            std::cout<<"PASS Broken stopped readback encoding 1."<<minor<<" fault "<<fault<<"; no repeated board/sender actions\n";
        }
        communicator->destroy();
    } catch(const std::exception &e) {
        std::cerr<<"FAIL "<<e.what()<<'\n';if(communicator)try{communicator->destroy();}catch(...){}return 1;
    }
}
