#include "receiverprocessmanager.h"
#include "controlprotocol.h"
#include "source_observation_barrier.h"
#include "source_observation_pause.h"
#include <sys/wait.h>
namespace {
using namespace GetDAQReceiverFixture;
namespace IO=GetDAQ::SupervisorIO;
void Cleanup(ReceiverProcessManager &manager,std::map<std::string,std::unique_ptr<IceProducer>> &sources) {
    for(auto &source:sources)source.second->Stop();
    for(const auto &receiver:manager.Status().receivers) if(receiver.pid>0 && !receiver.reaped) {
        siginfo_t info={};if(::waitid(P_PID,id_t(receiver.pid),&info,WEXITED|WNOHANG|WNOWAIT))continue;
        if(!info.si_pid)::kill(receiver.pid,SIGTERM);
        int result;while(::waitpid(receiver.pid,&result,0)<0 && errno==EINTR) {}
    }
}
}
int main(int argc,char **argv) {
    Ice::CommunicatorPtr communicator;
    try {
        Require(argc==6 || argc==7,"Pass CoBo/MuTaNT descriptions, ICE receiver, ROOT, ENCODING, optional source-fault/observe-reset");const std::string root=argv[4],mode=argc==7 ? argv[6] : "";
        Require(mode.empty() || mode=="source-fault" || mode=="source-recovery" || mode=="observe-reset","Unsupported local ICE ECC mode");
        Ice::InitializationData init;init.properties=Ice::createProperties();init.properties->setProperty("Ice.ThreadPool.Server.Size","4");communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("LocalIceEcc","tcp -h 127.0.0.1 -p 0");Rig rig(communicator,adapter,LoadHardwareDescription(argv[1]),LoadHardwareDescription(argv[2]),std::stoi(argv[5]));
        rig.options.mutants.clear();rig.senders.erase(std::remove_if(rig.senders.begin(),rig.senders.end(),[](const DataSenderOptions &s){return s.senderId[0]!='C';}),rig.senders.end());
        IO::CreateDirectory(root+"/outputs");DataSupervisorOptions plan;plan.receiverExecutable=plan.iceReceiverExecutable=argv[3];plan.iceReceiverSha256=ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024));
        plan.auditRoot=root+"/deployment";plan.deploymentId="local-ice-ecc";plan.startupTimeoutMs=plan.finishTimeoutMs=5000;
        std::map<std::string,std::unique_ptr<IceProducer>> producers;std::map<std::string,IceUtil::Handle<ObservationBarrierSource>> sources;
        for(auto &sender:rig.senders) {
            ReceiverSourcePlan source;source.id=sender.senderId;source.inputProtocol=sender.protocol="ICE";source.dataPort=Port();source.statusPort=Port();source.bindAddress="127.0.0.1";source.outputRoot=root+"/outputs";source.queueMiB=source.maxFrameMiB=1;
            sender.receiverAddress=source.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort);plan.sources.push_back(source);producers[sender.senderId].reset(new IceProducer(rig.w,sender.senderId));
            SourceControlOptions options;options.sourceId=sender.senderId;options.receiver.port=source.dataPort;options.receiver.encodingMinor=unsigned(rig.options.encodingMinor);options.receiver.timeoutMs=1000;
            sources[sender.senderId]=new ObservationBarrierSource(*producers.at(sender.senderId),options);sender.controlProxy=communicator->proxyToString(adapter->addWithUUID(sources.at(sender.senderId)));
        }
        const auto native=Options(rig,root);const auto artifact=DecodeSystemExperimentArtifact(native.experimentArtifactBytes);plan.configurationSha256=artifact.configurationSha256;plan.artifactSha256=artifact.artifactSha256;
        adapter->activate();ReceiverProcessManager data(plan);NativeEccCoordinator ecc(native,data);
        try {
            Rejects([&] {data.IceReceiverSessions();},"not Ready");
            ecc.Execute(EccCommand::Describe,"describe");ecc.Execute(EccCommand::Prepare,"prepare");ecc.Execute(EccCommand::Configure,"configure");State(ecc,EccState::Ready);
            const auto ready=data.Status();const auto bindings=data.IceReceiverSessions();Require(bindings.size()==2 && bindings[0].sessionId==ready.sessionId && bindings[1].sessionId==ready.sessionId,"Local source sessions differ from actual receiver cycle");
            if(mode=="source-fault" || mode=="source-recovery") {
                ecc.Execute(EccCommand::Start,"start");
                const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                for(const auto &entry:producers) {
                    while((entry.second->read[0]!=entry.second->write[0] || entry.second->read[1]!=entry.second->write[1]) && std::chrono::steady_clock::now()<deadline)std::this_thread::yield();
                    Require(entry.second->read[0]==entry.second->write[0] && entry.second->read[1]==entry.second->write[1],"Initial source frames still sending");
                }
                sources.begin()->second->ReportFailure("Synthetic asynchronous source device fault");rig.Clear();
                const auto observed=ParseStrictJson(ecc.ObserveSourcesJson());Require(observed.At("sources").items.size()==2 && !ecc.Status().outcomeUnknown,"Readonly observation changed controller state");
                bool found=false;for(const auto &node:observed.At("sources").items)found=found || IO::String(node.At("state"))=="unknown";
                Require(found,"Async source fault missing from readonly observation");
                {std::lock_guard<std::mutex> lock(rig.w.logMutex);Require(rig.w.actions.empty(),"Observation caused mutation");}
                ecc.StopProductionAfterFailure("source-failure-stop");Require(ecc.Status().outcomeUnknown && ecc.Status().state==EccState::Running,"Source failure stop cleared parent unknown");
                const auto stopped=ParseStrictJson(ecc.ObserveSourcesJson());
                for(const auto &node:stopped.At("sources").items)Require(IO::String(node.At("state"))=="unknown" && IO::Boolean(node.At("failure_stop_acknowledged")) && !IO::Boolean(node.At("source_end_acknowledged")),"Source failure stop did not retain UNKNOWN/no-End");
                for(const auto &entry:producers)Require(!entry.second->readout->Status().workerRunning && entry.second->readout->Status().deviceStopAcknowledged,"Readout worker not stopped/joined");
                const auto retained=data.Status();Require(retained.sessionId==ready.sessionId,"Failure stop replaced receiver session");
                for(const auto &receiver:retained.receivers)Require(!receiver.reaped && !receiver.sealVerified && !receiver.sourceEndConfirmed,"Failure stop fabricated receiver end");
                Rejects([&] {ecc.Execute(EccCommand::Start,"retry-start");},"UNKNOWN");
                ecc.HoldReceiverFailure("start");
                const auto writes=rig.w.Writes();
                Rejects([&] {ecc.BreakupAfterFailure("unknown-source-connections");},"unknown sender");
                for(const auto &id:{"source-held-recovery","source-held-recovery-again"})
                    Rejects([&] {ecc.ReconcileAcquisition(id,"start");},"same original");
                Require(ecc.Status().outcomeUnknown && ecc.Status().needsReconciliation && ecc.Status().operationId=="start" &&
                    ecc.Operation("start").phase=="complete" && rig.w.Writes()==writes,
                    "Recovery rewrote successful Start, cleared source/receiver hold or replayed hardware");
                const auto after=data.Status();Require(after.sessionId==retained.sessionId && after.receivers.size()==retained.receivers.size(),
                    "Denied source recovery changed retained receivers");
                for(std::size_t i=0;i<after.receivers.size();++i)Require(after.receivers[i].pid==retained.receivers[i].pid &&
                    !after.receivers[i].reaped && !after.receivers[i].sealVerified && !after.receivers[i].sourceEndConfirmed,
                    "Denied source recovery terminated, replaced or sealed original receivers");
                IO::WriteNew(root+"/source-recovery-denial-proof.json","{\"passed\":true,\"encoding_minor\":"+
                    std::to_string(rig.options.encodingMinor)+",\"denied_recovery_attempts\":2,\"unknown_source_disconnect_rejected\":true,\"original_start_complete\":true,\"original_receivers_retained\":true,\"recovery_hardware_writes\":0,\"complete_retirement_not_reached\":true}\n");
                IO::WriteNew(root+"/sources.json",ecc.ObserveSourcesJson());
                if(mode=="source-recovery") {
                    ecc.RetireFailedSources("original-source-retirement");
                    Require(ecc.Status().outcomeUnknown && ecc.Operation("start").phase=="complete","Source retirement cleared parent/original result");
                    Rejects([&]{ecc.RetireFailedSources("repeat-source-retirement");},"prior attempt");
                    ecc.BreakupAfterFailure("original-source-breakup");
                    ecc.AbortFailedReceivers("original-source-receiver-abort");
                    ecc.RetireFailedReceivers("original-source-receiver-retirement");
                    ecc.UnprepareAcquisitionAfterFailure("original-source-unprepare");
                    const auto old=data.Status();Require(old.state==DataSupervisorStatus::Retired && old.sessionId==ready.sessionId,"Source cleanup did not retire original data");
                    for(std::size_t i=0;i<old.receivers.size();++i)Require(old.receivers[i].pid==ready.receivers[i].pid && old.receivers[i].reaped &&
                        !old.receivers[i].sealVerified && !old.receivers[i].sourceEndConfirmed,"Source cleanup changed identity or fabricated End/seal");
                    Rejects([&]{ecc.ReconcileAcquisition("ordinary-source-recovery-denied","start");},"same original");
                    rig.Clear();const auto beforeWrites=rig.w.Writes();ecc.ReconcileAcquisitionWithSources("verified-source-acquisition","start");
                    Require(ecc.Status().state==EccState::Described && !ecc.Status().outcomeUnknown && !ecc.Status().needsReconciliation &&
                        rig.w.Writes()==beforeWrites && ecc.Operation("start").phase=="complete","Explicit retired source recovery failed or rewrote original Start");
                    IO::WriteNew(root+"/old-retired-data.json",EncodeSupervisorStatus(old));
                    ecc.Execute(EccCommand::Prepare,"fresh-prepare");ecc.Execute(EccCommand::Configure,"fresh-configure");
                    Require(data.Status().sessionId!=old.sessionId,"Fresh source cycle reused failed data session");
                    ecc.Execute(EccCommand::Start,"fresh-start");ecc.Execute(EccCommand::Stop,"fresh-stop");ecc.Execute(EccCommand::Breakup,"fresh-breakup");
                    for(const auto &receiver:data.Status().receivers)Require(receiver.reaped && receiver.sealVerified && receiver.sourceEndConfirmed && receiver.frames==2,"Fresh source cycle lacks real End/seal");
                    for(const auto &receiver:data.Status().receivers){const auto seal=ParseStrictJson(IO::ReadFile(receiver.outputDirectory+"/.getdaq-ice-receiver-seal.json",65536));
                        std::string actual;for(const auto &file:seal.At("files").items)actual+=IO::ReadFile(receiver.outputDirectory+'/'+IO::String(file.At("name")),1024*1024);
                        Require(actual==producers.at(receiver.id)->Expected(),"Fresh source cycle raw bytes differ");}

                    for(const auto &sender:rig.senders){auto proxy=communicator->stringToProxy(sender.controlProxy)->ice_collocationOptimized(false);
                        const auto recovery=DataSourceRecoveryPrx::checkedCast(proxy);Require(bool(recovery),"Retirement extension missing");
                        const auto archived=recovery->describeFailedSourceRetirement(old.sessionId);
                        Require(archived.retired && archived.dataConnectionClosed && archived.source.state=="unknown" && archived.source.receiverSession==old.sessionId &&
                            !archived.source.sourceEndAcknowledged,"Fresh cycle rewrote original source retirement history");}
                    ecc.Execute(EccCommand::Reset,"fresh-unprepare");ecc.Execute(EccCommand::Reset,"fresh-forget");
                    IO::WriteNew(root+"/source-recovery-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                        ",\"original_start_complete\":true,\"original_raw_unsealed\":true,\"retired_source_unknown_preserved\":true,\"recovery_hardware_writes\":0,\"fresh_cycle_end_and_seal_verified\":true}\n");
                }

            } else {
            ecc.Execute(EccCommand::Start,"start");ecc.Execute(EccCommand::Pause,"pause");ecc.Execute(EccCommand::Resume,"resume");ecc.Execute(EccCommand::Stop,"stop");
            Require(data.Status().sessionId==ready.sessionId,"Local pause/resume replaced receiver");ecc.Execute(EccCommand::Breakup,"breakup");State(ecc,EccState::Prepared);
            const auto stopped=data.Status();for(const auto &receiver:stopped.receivers) {
                Require(receiver.reaped && receiver.exitCode==0 && receiver.sealVerified && receiver.sourceEndConfirmed && receiver.frames==4 && !receiver.sourceUnusedConfirmed,"Local ICE end/exit proof differs");
                const auto seal=ParseStrictJson(IO::ReadFile(receiver.outputDirectory+"/.getdaq-ice-receiver-seal.json",65536));Require(IO::Boolean(seal.At("sender_end_confirmed")) && !IO::Boolean(seal.At("sender_eof_confirmed")),"Local ICE became TCP EOF");
                std::string actual;for(const auto &file:seal.At("files").items)actual+=IO::ReadFile(receiver.outputDirectory+'/'+IO::String(file.At("name")),1024*1024);
                Require(actual==producers.at(receiver.id)->Expected(),"Local ICE raw data differs");
            }
            if(mode=="observe-reset") {
                auto &source=*sources.begin()->second;source.ArmObservation();
                ObservationPause pending([&] {return ecc.ObserveSourcesJson();});
                try {
                    source.WaitForObservation();pending.Suspend();source.ReleaseObservation();
                    Require(pending.Pending(),"Observation not pending before Reset");
                    ecc.Execute(EccCommand::Reset,"unprepare");State(ecc,EccState::Described);
                    Require(pending.Pending(),"Observation did not span completed Reset/object release");
                    const auto current=ParseStrictJson(ecc.ObserveSourcesJson());Require(current.At("sources").items.empty(),"Reset retained old acquisition source set");
                }catch(...) {source.ReleaseObservation();pending.Resume();throw;}
                const auto observed=ParseStrictJson(pending.Result());
                Require(observed.At("sources").items.size()==2 && IO::String(observed.At("controller_operation_id"))=="breakup","Pending observation adopted Reset state or lost retained handles");
                for(const auto &s:observed.At("sources").items) {
                    const auto phase=IO::String(s.At("phase"));Require((phase=="available" || phase=="unavailable") && !IO::String(s.At("source_instance_id")).empty() && IO::String(s.At("receiver_session"))==ready.sessionId,"Retained source disappeared or changed binding during Reset");
                    if(phase=="available")Require(IO::String(s.At("state"))=="disconnected" && IO::Boolean(s.At("source_end_acknowledged")),"Retained source end proof differs after Reset");
                }
                IO::WriteNew(root+"/source-reset-proof.json","{\"passed\":true,\"observation_spanned_completed_reset\":true,\"new_source_set_empty\":true,\"captured_operation_id\":\"breakup\"}\n");
            }else ecc.Execute(EccCommand::Reset,"unprepare");
            ecc.Execute(EccCommand::Reset,"forget");State(ecc,EccState::Idle);IO::WriteNew(root+"/stopped.json",EncodeSupervisorStatus(stopped));
            }
        } catch(...) {Cleanup(data,producers);throw;}Cleanup(data,producers);adapter->deactivate();adapter->waitForDeactivate();adapter->destroy();communicator->destroy();communicator=nullptr;
        IO::WriteNew(root+"/result.json","{\"passed\":true,\"real_hardware\":false,\"raw_budget_bytes\":1048576}\n");std::cout<<"PASS local native ECC ICE/source-session/receiver lifecycle\n";return 0;
    } catch(const std::exception &e) {if(communicator)try {communicator->destroy();} catch(...) {}std::cerr<<e.what()<<'\n';return 1;}
}
