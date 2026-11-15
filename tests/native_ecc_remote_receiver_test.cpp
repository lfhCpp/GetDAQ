#include "remotereceivermanager.h"
#include "remotehandoffevidence.h"
#include "eccsuccessorplan.h"
#include "fixed_ecc_fixture.h"
#include "native_receiver_fixture.h"
#include "native_remote_service_fixture.h"
#include "planio.h"
#include "receivercatalog.h"
#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <fstream>
#include <fcntl.h>
#include <poll.h>
#include <spawn.h>
#include <sstream>
#include <sys/stat.h>
#include <sys/wait.h>
extern char **environ;
namespace {
using namespace GetDAQReceiverFixture;
using namespace GetDAQRemoteServiceFixture;
void CheckFiles(const RemoteReceiverStatus &status,const std::map<std::string,IceUtil::Handle<RawSender>> &raw,unsigned int frames) {
    Require(!status.needsReconciliation,"Remote data status not confirmed");
    for (const auto &node:status.nodes) {
        Require(node.data.state==DataSupervisorStatus::Stopped && !node.data.needsReconciliation,"Remote node not stopped");
        for (const auto &c:node.data.receivers) {
            Require(c.reaped && c.exitCode==0 && c.sealVerified && c.frames==frames,"Remote receiver final status differs");
            const auto seal=ParseStrictJson(IO::ReadFile(c.outputDirectory+"/.getdaq-receiver-seal.json",65536));
            Require(IO::String(seal.At("session_id"))==node.data.sessionId && IO::String(seal.At("sender_id"))==c.id && IO::Boolean(seal.At("sender_eof_confirmed")),"Remote seal identity/EOF differs");
            std::string actual; for (const auto &file:seal.At("files").items) actual+=IO::ReadFile(c.outputDirectory+'/'+IO::String(file.At("name")),1024*1024);
            Require(actual==raw.at(c.id)->Expected(),"Remote raw bytes differ from actual independent fixture sender");
        }
    }
}
void RejectColdCatalogWhenBoundaryChanges(const RemoteReceiverOptions &remote,const NativeEccOptions &controller,
        const std::vector<std::unique_ptr<Service>> &services,const std::string &originalPublishPath,
        const std::string &newOperationId) {
    std::vector<unsigned int> catalogCommands;
    for(const auto &service:services)catalogCommands.push_back(service->Commands("catalog"));
    unsigned int boundaryChecks=0;bool rejected=false;
    try {
        RemoteReceiverManager::ResumeColdCatalogReplacement(remote,controller,originalPublishPath,newOperationId,
            "Exp",1,[&] {
                if(++boundaryChecks==2)throw std::runtime_error("synthetic Prepared/hold boundary revoked");
            });
    } catch(const std::runtime_error &e) {
        rejected=std::string(e.what()).find("synthetic Prepared/hold boundary revoked")!=std::string::npos;
    }
    Require(rejected && boundaryChecks==2,"Cold catalog helper ignored a revoked caller boundary");
    for(std::size_t i=0;i<services.size();++i)
        Require(services[i]->Commands("catalog")==catalogCommands[i],"Revoked cold boundary submitted a remote catalog command");
    struct stat operation;
    Require(::lstat((controller.auditRoot+"/operations/"+newOperationId).c_str(),&operation)!=0 && errno==ENOENT,
        "Revoked cold boundary created an operation directory");
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN); Ice::CommunicatorPtr communicator;
    try {
        Require((argc==8 || argc==9) && std::string(argv[6])=="--scenario","Pass CoBo/MuTaNT metadata, receiver, service, index, --scenario SCENARIO [NEW_ROOT]"); const std::string scenario=argv[7];
        const bool handoff=scenario.find("handoff-")==0;
        const bool completeHandoff=scenario=="handoff-complete-0" || scenario=="handoff-complete-1";
        const bool failedHandoffCompletion=scenario=="handoff-failed-completion-0" || scenario=="handoff-failed-completion-1";
        const bool completeRetirement=scenario=="handoff-retire-complete-0" || scenario=="handoff-retire-complete-1";
        const bool failedRetirement=scenario=="handoff-retire-failed-0" || scenario=="handoff-retire-failed-1";
        const bool coldRetirement=scenario=="handoff-cold-retire-0" || scenario=="handoff-cold-retire-1";
        const bool coldHandoff=scenario=="handoff-cold-control-0" || scenario=="handoff-cold-control-1";
        const bool plannedHandoff=scenario=="handoff-planned-0" || scenario=="handoff-planned-1";
        const bool retirement=scenario=="retire-sealed-0" || scenario=="retire-sealed-1" || scenario=="retire-audit-fault";
        const bool sealedCleanup=retirement || scenario=="failed-cleanup-sealed-0" || scenario=="failed-cleanup-sealed-1";
        const std::set<std::string> known={"handoff-0","handoff-1","handoff-ack-loss","handoff-receipt-fault","retire-sealed-0","retire-sealed-1","retire-audit-fault","failed-cleanup-sealed-0","failed-cleanup-sealed-1","normal-0","normal-1","truncated","start-ack-loss","finish-ack-loss","partial-start","service-restart","service-restart-paused","data-only","catalog-replace","catalog-prepare-failure","catalog-publish-failure","catalog-finalize-failure","observe-events","plan-mismatch","audit-fault","ownership"};
        Require(known.count(scenario) || plannedHandoff || completeHandoff || failedHandoffCompletion || completeRetirement || failedRetirement || coldRetirement || coldHandoff,"Unknown remote receiver fixture scenario");
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]); Ice::InitializationData init;
        init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4"); communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("RemoteReceiverFixture","tcp -h 127.0.0.1 -p 0");
        const unsigned int encodingMinor=(scenario=="handoff-retire-failed-0" || scenario=="handoff-failed-completion-0" || scenario=="handoff-complete-0" || scenario=="handoff-retire-complete-0" || scenario=="handoff-cold-retire-0" || scenario=="handoff-cold-control-0" || scenario=="handoff-0" || scenario=="handoff-planned-0" || scenario=="normal-0" || scenario=="failed-cleanup-sealed-0" || scenario=="retire-sealed-0") ? 0 : 1;
        Rig r(communicator,adapter,cobo,mutant,encodingMinor); r.options.mutants.clear();
        r.senders.erase(std::remove_if(r.senders.begin(),r.senders.end(),[](const DataSenderOptions &s) { return s.senderId[0]!='C'; }),r.senders.end());
        std::sort(r.senders.begin(),r.senders.end(),[](const DataSenderOptions &a,const DataSenderOptions &b) { return a.senderId<b.senderId; });
        char parent[]="/tmp/getdaq-native-remote-XXXXXX";
        std::string root;
        if(argc==9) { root=argv[8]; IO::CreateDirectory(root); }
        else { Require(::mkdtemp(parent),"Create remote receiver fixture root");root=parent; }
        IO::CreateDirectory(root+"/outputs");
        std::map<std::string,IceUtil::Handle<RawSender>> raw; std::vector<DataSupervisorOptions> plans;
        for (auto &sender:r.senders) {
            raw[sender.senderId]=new RawSender(r.w,sender.senderId); sender.controlProxy=communicator->proxyToString(adapter->addWithUUID(raw[sender.senderId]));
            DataSupervisorOptions plan; plan.receiverExecutable=argv[3]; plan.deploymentId="actual-remote-fixture"; plan.startupTimeoutMs=plan.finishTimeoutMs=3000; plan.statusTimeoutMs=1000;
            if(scenario=="catalog-replace" || scenario=="catalog-prepare-failure" || scenario=="catalog-publish-failure" || scenario=="catalog-finalize-failure") { plan.catalogRoot=root+"/catalog-"+sender.senderId; IO::CreateDirectory(plan.catalogRoot); }
            ReceiverSourcePlan source; source.id=sender.senderId; source.outputRoot=root+"/outputs"; source.bindAddress="127.0.0.1"; source.dataPort=Port(); source.statusPort=Port();
            source.receiverAddress=sender.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort); source.queueMiB=source.maxFrameMiB=1; plan.sources.push_back(source); plans.push_back(plan);
        }
        auto o=Options(r,root); const auto artifact=DecodeSystemExperimentArtifact(o.experimentArtifactBytes); RemoteReceiverOptions remote; remote.controllerId="native-remote-controller";
        remote.commandTimeoutMs=15000; remote.pollIntervalMs=20;
        std::vector<std::unique_ptr<Service>> services; std::vector<std::unique_ptr<DropReplyRelay>> relays;
        for (std::size_t i=0;i<plans.size();++i) {
            const auto id="node-"+std::to_string(i),directory=root+'/'+id; plans[i].auditRoot=directory+"/deployment";
            plans[i].configurationSha256=artifact.configurationSha256; plans[i].artifactSha256=artifact.artifactSha256;
            std::map<std::string,std::string> environment;
            if (failedRetirement && i==1) {
                const auto injector=std::getenv("GETDAQ_TEST_RETIREMENT_INJECTOR"),asan=std::getenv("GETDAQ_TEST_ASAN_RUNTIME");
                Require(injector && *injector,"Failed retirement fixture requires its test injector");
                environment["LD_PRELOAD"]=(asan ? std::string(asan)+':' : std::string())+injector;
                environment["GETDAQ_TEST_RETIREMENT_PLAN"]=root+"/next-plan-1.json";
                environment["GETDAQ_TEST_RETIREMENT_BACKUP"]=root+"/next-plan-1.backup";
                environment["GETDAQ_TEST_RETIREMENT_MARKER"]=root+"/accepted-retirement-marker";
            }
            if (failedHandoffCompletion && i==1) {
                const auto injector=std::getenv("GETDAQ_TEST_HANDOFF_INJECTOR");
                Require(injector && *injector,"Failed handoff fixture requires its test injector");
                environment["LD_PRELOAD"]=injector;
                environment["GETDAQ_TEST_HANDOFF_PROOF"]="handoff-proof.json";
            }
            services.emplace_back(new Service(argv[4],directory,id,Port(),plans[i],environment)); RemoteReceiverEndpoint endpoint;
            endpoint.agentId=id; endpoint.planSha256=GetDAQ::ConfigurationSha256(IO::EncodeOptions(plans[i],GetDAQ::ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)))); endpoint.control=services.back()->direct;
            if (coldRetirement || (i==0 && (scenario=="start-ack-loss" || scenario=="finish-ack-loss" || scenario=="handoff-ack-loss" || completeHandoff || coldHandoff))) {
                const auto port=Port(); relays.emplace_back(new DropReplyRelay(port,services.back()->port,coldRetirement ? "" : scenario=="start-ack-loss" ? "start" : (scenario=="handoff-ack-loss" || completeHandoff || coldHandoff) ? "handoff-control" : "finish")); endpoint.control.port=port;
            }
            if (scenario=="plan-mismatch" && i==0) endpoint.planSha256=std::string(64,'0');
            remote.endpoints.push_back(endpoint);
        }
        adapter->activate();
        std::unique_ptr<RemoteReceiverManager> dataOwner(new RemoteReceiverManager(remote));auto &data=*dataOwner;
        std::unique_ptr<NativeEccCoordinator> eccOwner(new NativeEccCoordinator(o,data));auto &ecc=*eccOwner;
        bool coldCompleted=false;std::string coldBinding;
        std::ofstream index(argv[5]); Require(bool(index),"Create remote ECC evidence index");
        const auto validate=[&] { data.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256); };
        const auto directCycle=[&] {
            validate(); data.VerifyStopped(); IO::CreateDirectory(o.auditRoot+"/operations/manual"); data.StartReceivers(o.auditRoot+"/operations/manual/start",o.senders);
            for (auto &service:services) service->Capture();
        };
        try {
            auto wrong=o.senders; wrong[0].receiverAddress="127.0.0.1:1"; Rejects([&] { data.Validate(EccCommand::Configure,EccState::Prepared,wrong,artifact.configurationSha256,artifact.artifactSha256); },scenario=="plan-mismatch" ? "plan differs" : "target differs");
            if (scenario=="plan-mismatch") {
                Rejects(validate,"plan differs"); Require(!r.Has("C0.sender-connect") && ecc.Operations().empty(),"Wrong remote plan touched hardware");
            } else if (handoff || sealedCleanup || scenario=="data-only" || scenario=="catalog-replace" || scenario=="catalog-prepare-failure" || scenario=="catalog-publish-failure" || scenario=="catalog-finalize-failure" || scenario=="observe-events" || scenario=="audit-fault" || scenario=="ownership") {
                directCycle(); data.VerifyReady();
                if (scenario=="observe-events") {
                    const auto hardwareActionsBefore=r.w.actions.size();
                    const auto eccOperationsBefore=ecc.Operations().size();
                    const auto before=data.Status();
                    const auto receiverObservation=ParseStrictJson(data.ObserveReceiversJson());
                    Require(receiverObservation.type==JsonValue::Array && receiverObservation.items.size()==remote.endpoints.size(),
                        "Remote receiver observation omitted a fixed endpoint");
                    for (std::size_t i=0;i<receiverObservation.items.size();++i) {
                        const auto &node=receiverObservation.items[i];
                        ExactSupervisorFields(node,{"agent_id","plan_sha256","instance_id","available","message","observation"});
                        Require(SupervisorIO::String(node.At("agent_id"))==remote.endpoints[i].agentId &&
                            SupervisorIO::String(node.At("plan_sha256"))==remote.endpoints[i].planSha256 &&
                            SupervisorIO::String(node.At("instance_id"))==before.nodes[i].instanceId &&
                            SupervisorIO::Boolean(node.At("available")) && SupervisorIO::String(node.At("message")).empty() &&
                            node.At("observation").type==JsonValue::Object,
                            "Remote receiver observation lost endpoint identity or lifecycle data");
                    }
                    Require(ecc.Operations().size()==eccOperationsBefore && r.w.actions.size()==hardwareActionsBefore,
                        "Read-only receiver observation fabricated ECC/board actions");
                    auto eventObservation=[&] { return ParseStrictJson(data.ObserveAcquisitionEventsJson("C0")); };
                    const auto initial=eventObservation();
                    const auto initialBytes=ParseStrictJson(data.ObserveAcquisitionEventsJson("C0",true));
                    Require(SupervisorIO::Boolean(initialBytes.At("available")) && SupervisorIO::String(initialBytes.At("accepted_file_bytes"))=="0","Initial raw-file byte query is not zero/available");
                    Require(SupervisorIO::Boolean(initial.At("available")) && SupervisorIO::String(initial.At("source_id"))=="C0" &&
                        SupervisorIO::String(initial.At("session_id"))==before.nodes[0].data.sessionId &&
                        IO::Unsigned(initial.At("pid"))==std::uint64_t(before.nodes[0].data.receivers[0].pid) &&
                        SupervisorIO::String(initial.At("acquisition_events"))=="0","Remote event query lost original source/session/PID or initial value");
                    Require(ecc.Operations().size()==eccOperationsBefore && ecc.Status().state==EccState::Idle &&
                        r.w.actions.size()==hardwareActionsBefore,"Read-only remote event observation fabricated ECC/board actions");
                    auto &selected=raw.at("C0"); selected->connect("TCP",o.senders.front().receiverAddress,Ice::Current()); selected->daqStart(Ice::Current());
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(3); JsonValue current;
                    do { current=eventObservation(); if(SupervisorIO::String(current.At("acquisition_events"))=="2") break; std::this_thread::sleep_for(std::chrono::milliseconds(10)); }
                    while(std::chrono::steady_clock::now()<deadline);
                    Require(SupervisorIO::String(current.At("acquisition_events"))=="2" && SupervisorIO::Boolean(current.At("available")),"Remote event query did not observe two complete frames while receiver was live");
                    const auto hardwareActionsAfterSend=r.w.actions.size();
                    const auto currentBytes=ParseStrictJson(data.ObserveAcquisitionEventsJson("C0",true));
                    Require(SupervisorIO::Boolean(currentBytes.At("available")) &&
                        SupervisorIO::String(currentBytes.At("accepted_file_bytes"))==std::to_string(selected->Expected().size()) &&
                        SupervisorIO::String(currentBytes.At("session_id"))==before.nodes[0].data.sessionId &&
                        IO::Unsigned(currentBytes.At("pid"))==std::uint64_t(before.nodes[0].data.receivers[0].pid),"Raw-file byte observation differs from actual written frame bytes or original session/PID");
                    Require(r.w.actions.size()==hardwareActionsAfterSend && ecc.Operations().size()==eccOperationsBefore,"Read-only raw-file byte query issued a hardware command");
                    for(const auto &sender:o.senders) if(sender.senderId!="C0") {
                        auto &source=raw.at(sender.senderId);source->connect("TCP",sender.receiverAddress,Ice::Current());source->daqStart(Ice::Current());source->disconnect(Ice::Current());
                    }
                    selected->disconnect(Ice::Current());
                    data.FinishReceivers(o.auditRoot+"/operations/manual/finish"); data.VerifyStopped(); CheckFiles(data.Status(),raw,2);
                    IO::WriteNew(root+"/receiver-event-observation.json","{\"passed\":true,\"source\":\"C0\",\"events\":2,\"bound_session\":true,\"bound_pid\":true,\"raw_payload_read_by_control\":false,\"hardware_actions\":0}\n");
                } else if (sealedCleanup) {
                    const auto original=data.Status();raw.at("C0")->truncate=true;
                    for (const auto &sender:o.senders) { auto &s=raw.at(sender.senderId);s->connect("TCP",sender.receiverAddress,Ice::Current());s->daqStart(Ice::Current());s->disconnect(Ice::Current()); }
                    Rejects([&] {data.FinishReceivers(o.auditRoot+"/operations/manual/finish");},"abnormal exit");
                    const auto failed=data.Status();
                    Require(failed.nodes[1].data.state==DataSupervisorStatus::Stopped && failed.nodes[1].data.receivers[0].sealVerified,"Independent peer did not seal after first Finish failure");
                    const auto sealPath=failed.nodes[1].data.receivers[0].outputDirectory+"/.getdaq-receiver-seal.json";
                    const auto sealBytes=IO::ReadFile(sealPath,65536),rawBytes=raw.at("C1")->Expected();
                    r.Clear();data.AbortFailedReceivers(o.auditRoot+"/operations/manual/cleanup","fixture-parent-failure");
                    const auto cleaned=data.Status();
                    Require(cleaned.needsReconciliation && ecc.Status().needsReconciliation,"Partial Finish cleanup cleared reconciliation");
                    Require(services[0]->Commands("abort-failed-session")==1 && services[1]->Commands("abort-failed-session")==0,"Cleanup replayed or sent POST to already sealed peer");
                    for(std::size_t i=0;i<cleaned.nodes.size();++i) {
                        const auto &now=cleaned.nodes[i].data,&before=original.nodes[i].data;
                        Require(now.sessionId==before.sessionId && now.receivers[0].pid==before.receivers[0].pid && now.receivers[0].reaped,"Cleanup adopted another session/PID or left receiver");
                    }
                    Require(cleaned.nodes[0].data.state==DataSupervisorStatus::Aborted && !cleaned.nodes[0].data.receivers[0].sealVerified,"Failed node fabricated seal");
                    Require(cleaned.nodes[1].data.state==DataSupervisorStatus::Stopped && cleaned.nodes[1].data.receivers[0].sealVerified && IO::ReadFile(sealPath,65536)==sealBytes,"Cleanup changed sealed peer");
                    std::string retained;const auto seal=ParseStrictJson(sealBytes);
                    for(const auto &file:seal.At("files").items)retained+=IO::ReadFile(cleaned.nodes[1].data.receivers[0].outputDirectory+'/'+IO::String(file.At("name")),1024*1024);
                    Require(retained==rawBytes && r.w.actions.empty(),"Cleanup changed sealed raw bytes or sent board action");
                    const auto audit=IO::ReadFile(o.auditRoot+"/operations/manual/cleanup/remote.jsonl",1024*1024);
                    Require(audit.find("node_already_sealed")!=std::string::npos && audit.find("no_post_sent")!=std::string::npos,"Missing explicit sealed-node skip record");
                    Rejects([&] {data.AbortFailedReceivers(o.auditRoot+"/operations/manual/retry","fixture-parent-failure");},"already attempted");
                    Require(services[0]->Commands("abort-failed-session")==1 && services[1]->Commands("abort-failed-session")==0,"Retry replayed receiver cleanup");
                    IO::WriteNew(root+"/sealed-cleanup-proof.json","{\"passed\":true,\"sealed_node_cleanup_posts\":0,\"failed_node_cleanup_posts\":1,\"sealed_bytes_preserved\":true,\"original_sessions_pids\":true,\"reconciliation_retained\":true,\"hardware_stop_independently_verified\":false,\"ecc_cleanup_entry_exercised\":false}\n");
                    if(retirement) {
                        Rejects([&] {data.RetireFailedReceivers(o.auditRoot+"/operations/manual/wrong-parent","another-failure");},"original completed abort");
                        if(scenario=="retire-audit-fault") {
                            const auto path=o.auditRoot+"/operations/manual/cleanup/intent.json",bytes=IO::ReadFile(path,1024*1024);
                            {std::ofstream file(path,std::ios::app);file<<" ";}
                            Rejects([&] {data.RetireFailedReceivers(o.auditRoot+"/operations/manual/retire","fixture-parent-failure");},"changed");
                            {std::ofstream file(path,std::ios::trunc);file<<bytes;}
                            Rejects([&] {data.RetireFailedReceivers(o.auditRoot+"/operations/manual/restored","fixture-parent-failure");},"independent hold");
                            Require(data.NeedsReconciliation() && services[0]->Commands("retire-failed-session")==0,"Changed parent audit allowed remote retirement");
                        } else {
                            data.RetireFailedReceivers(o.auditRoot+"/operations/manual/retire","fixture-parent-failure");data.VerifyStopped();
                            const auto retired=data.Status();
                            Require(!data.NeedsReconciliation() && retired.nodes[0].data.state==DataSupervisorStatus::Retired &&
                                retired.nodes[1].data.state==DataSupervisorStatus::Stopped,"Remote retirement lost failed/sealed distinction");
                            Require(services[0]->Commands("retire-failed-session")==1 && services[1]->Commands("retire-failed-session")==0,"Retirement posted to sealed sibling");
                            Require(IO::ReadFile(sealPath,65536)==sealBytes && r.w.actions.empty(),"Retirement changed sealed receipt or touched hardware");
                            for(std::size_t i=0;i<retired.nodes.size();++i)
                                Require(retired.nodes[i].data.sessionId==original.nodes[i].data.sessionId && retired.nodes[i].data.receivers[0].pid==original.nodes[i].data.receivers[0].pid,"Retirement adopted cycle/PID");
                            Rejects([&] {data.RetireFailedReceivers(o.auditRoot+"/operations/manual/repeat-retire","fixture-parent-failure");},"prior attempt");
                            raw.at("C0")->truncate=false;
                            data.StartReceivers(o.auditRoot+"/operations/manual/new-start",o.senders);data.VerifyReady();
                            const auto next=data.Status();
                            for(std::size_t i=0;i<next.nodes.size();++i)
                                Require(next.nodes[i].data.sessionId!=original.nodes[i].data.sessionId,"New cycle reused old session");
                            for (const auto &sender:o.senders) {auto &s=raw.at(sender.senderId);s->connect("TCP",sender.receiverAddress,Ice::Current());s->daqStart(Ice::Current());s->disconnect(Ice::Current());}
                            data.FinishReceivers(o.auditRoot+"/operations/manual/new-finish");data.VerifyStopped();
                            Require(IO::ReadFile(sealPath,65536)==sealBytes,"New cycle rewrote old sibling seal");
                        }
                        IO::WriteNew(root+"/retirement-proof.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(encodingMinor)+
                            ",\"sealed_node_retirement_posts\":0,\"failed_node_retirement_posts\":"+(scenario=="retire-audit-fault" ? "0" : "1")+
                            ",\"parent_ecc_recovery_exercised\":false,\"board_actions\":0}\n");
                    }
                } else if (scenario=="audit-fault") {
                    const auto path=o.auditRoot+"/operations/manual/start/intent.json",original=IO::ReadFile(path,1024*1024);
                    { std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary); file.put('!'); file.flush(); Require(bool(file),"Inject retained audit corruption"); }
                    Rejects([&] { data.VerifyReady(); },"file changed");
                    { std::ofstream file(path,std::ios::binary|std::ios::trunc); file<<original; }
                    Rejects([&] { data.VerifyReady(); },"UNKNOWN"); Require(data.NeedsReconciliation() && ecc.Status().needsReconciliation,"Retained audit restoration cleared provider/parent gate");
                } else if (scenario=="ownership") {
                    auto reversed=remote; std::reverse(reversed.endpoints.begin(),reversed.endpoints.end()); RemoteReceiverManager other(reversed);
                    Require(other.BindingSha256()==data.BindingSha256(),"Remote binding depends on endpoint input order"); other.BindController(ecc.SnapshotSha256(),o.auditRoot);
                    Rejects([&] { other.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256); },"owner scope/epoch differs");
                    Require(other.NeedsReconciliation() && !data.NeedsReconciliation(),"Competing owner affected first receiver manager");
                } else {
                    for (const auto &sender:o.senders) { auto &s=raw.at(sender.senderId); s->connect("TCP",sender.receiverAddress,Ice::Current()); s->daqStart(Ice::Current()); s->disconnect(Ice::Current()); }
                    data.FinishReceivers(o.auditRoot+"/operations/manual/finish"); data.VerifyStopped(); CheckFiles(data.Status(),raw,2);
                    Require(ecc.Operations().empty() && ecc.Status().state==EccState::Idle,"Data-only fixture fabricated an ECC state transition");
                    if (scenario=="catalog-replace" || scenario=="catalog-prepare-failure" || scenario=="catalog-publish-failure" || scenario=="catalog-finalize-failure") {
                        const auto first=data.Status();
                        data.CatalogReceivers(o.auditRoot+"/operations/manual/catalog-initial","Exp",1,false);
                        std::vector<std::string> initialCatalogs;
                        if (scenario=="catalog-prepare-failure" || scenario=="catalog-publish-failure" || scenario=="catalog-finalize-failure")
                            for(const auto &node:data.Status().nodes) initialCatalogs.push_back(node.catalogJson);
                        Require(!data.CanCatalog(),"Already-cataloged Run remained eligible for another ordinary catalog");
                        std::vector<std::string> oldFiles;
                        for(const auto &node:first.nodes) for(const auto &receiver:node.data.receivers) {
                            const auto seal=ParseStrictJson(IO::ReadFile(receiver.outputDirectory+"/.getdaq-receiver-seal.json",65536));
                            for(const auto &file:seal.At("files").items) oldFiles.push_back(IO::String(file.At("name")));
                        }
                        data.StartReceivers(o.auditRoot+"/operations/manual/replace-start",o.senders); data.VerifyReady();
                        for(const auto &sender:o.senders) { auto &s=raw.at(sender.senderId); s->connect("TCP",sender.receiverAddress,Ice::Current()); s->daqStart(Ice::Current()); s->disconnect(Ice::Current()); }
                        data.FinishReceivers(o.auditRoot+"/operations/manual/replace-finish"); data.VerifyStopped();
                        if (scenario=="catalog-prepare-failure") {
                            Require(plans.size()>=2,"Prepare-barrier fixture requires at least two logical data hosts");
                            Require(initialCatalogs.size()==plans.size(),"Initial formal catalog receipts were not retained before the new cycle");
                            const auto firstTarget=IO::String(ParseStrictJson(initialCatalogs.front()).At("directory"));
                            const auto slash=firstTarget.find_last_of('/'); Require(slash!=std::string::npos,"Cannot identify protected catalog parent");
                            const auto blockedParent=firstTarget.substr(0,slash);
                            Require(::chmod(blockedParent.c_str(),0500)==0,"Cannot block one node's replacement-stage parent");
                            bool rejected=false;
                            try { data.CatalogReceivers(o.auditRoot+"/operations/manual/catalog-prepare-failure","Exp",1,true); }
                            catch(const std::exception &) { rejected=true; }
                            Require(::chmod(blockedParent.c_str(),0700)==0,"Cannot restore protected catalog parent permissions");
                            Require(rejected,"A failed node prepare incorrectly allowed the replacement operation to complete");
                            for(std::size_t i=0;i<initialCatalogs.size();++i) {
                                const auto target=IO::String(ParseStrictJson(initialCatalogs[i]).At("directory"));
                                Require(GetDAQ::InspectReceiverCatalog(target)==initialCatalogs[i],"Prepare failure changed an old formal Run catalog");
                            }
                            struct stat publishIntent;
                            Require(::lstat((o.auditRoot+"/operations/manual/catalog-prepare-failure_publish/intent.json").c_str(),&publishIntent)!=0 && errno==ENOENT,
                                "Prepare failure emitted a publish intent");
                            const auto prepareLog=IO::ReadFile(o.auditRoot+"/operations/manual/catalog-prepare-failure_prepare/remote.jsonl",1024*1024);
                            Require(prepareLog.find("\"phase\":\"catalog_prepared\"")!=std::string::npos &&
                                prepareLog.find("\"phase\":\"node_unknown\"")!=std::string::npos,
                                "Prepare barrier did not retain both prepared-node and failed-node evidence");
                            IO::WriteNew(root+"/catalog-prepare-barrier-proof.json","{\"passed\":true,\"all_old_targets_unchanged\":true,\"publish_phase_started\":false,\"real_hardware\":false}\n");
                        } else if (scenario=="catalog-publish-failure") {
                            Require(plans.size()>=2 && initialCatalogs.size()==plans.size(),"Publish-failure fixture requires two retained formal catalogs");
                            const auto operation=o.auditRoot+"/operations/manual/catalog-publish-failure";
                            data.PrepareCatalogReplacement(operation,"Exp",1);
                            for(const auto &service:services) Require(service->Commands("catalog")==2,"All nodes did not receive exactly one old catalog and one prepare");
                            const auto firstTarget=IO::String(ParseStrictJson(initialCatalogs.front()).At("directory"));
                            const auto slash=firstTarget.find_last_of('/');Require(slash!=std::string::npos,"Cannot identify first formal catalog parent");
                            const auto blockedParent=firstTarget.substr(0,slash);
                            Require(::chmod(blockedParent.c_str(),0500)==0,"Cannot inject first-node publish failure");
                            bool rejected=false;try{data.PublishCatalogReplacement(operation,"Exp",1);}catch(const std::exception &){rejected=true;}
                            Require(::chmod(blockedParent.c_str(),0700)==0,"Cannot restore first-node catalog permissions");
                            Require(rejected && data.NeedsReconciliation(),"Uncertain first-node publish did not hold parent reconciliation");
                            for(std::size_t i=0;i<initialCatalogs.size();++i) {
                                const auto target=IO::String(ParseStrictJson(initialCatalogs[i]).At("directory"));
                                Require(GetDAQ::InspectReceiverCatalog(target)==initialCatalogs[i],"A failed publish changed a formal catalog");
                            }
                            Require(services[0]->Commands("catalog")==3 && services[1]->Commands("catalog")==2,
                                "Publish failure was retried or posted to a later data node");
                            const auto publishLog=IO::ReadFile(operation+"_publish/remote.jsonl",1024*1024);
                            Require(publishLog.find("\"phase\":\"node_unknown\"")!=std::string::npos &&
                                publishLog.find("\"phase\":\"node_not_attempted\"")!=std::string::npos &&
                                publishLog.find("\"post_sent\":false")!=std::string::npos,
                                "Publish audit omitted unknown and not-attempted node evidence");
                            const auto hardwareActionsBeforeColdRecovery=r.w.actions.size();
                            eccOwner.reset();dataOwner.reset();
                            RejectColdCatalogWhenBoundaryChanges(remote,o,services,operation+"_publish","cold-catalog-boundary-reject");
                            unsigned int coldBoundaryChecks=0;
                            const auto resumed=RemoteReceiverManager::ResumeColdCatalogReplacement(remote,o,operation+"_publish",
                                "cold-catalog-publish-resume","Exp",1,[&] {
                                    ++coldBoundaryChecks;
                                    Require(r.w.actions.size()==hardwareActionsBeforeColdRecovery,
                                        "Cold catalog continuation unexpectedly touched hardware");
                                });
                            Require(coldBoundaryChecks>4*remote.endpoints.size(),"Cold catalog recovery did not repeatedly recheck its caller boundary");
                            coldCompleted=true;coldBinding=resumed.controllerScopeSha256;
                            Require(!resumed.needsReconciliation,"Verified cold Publish continuation left reconciliation latched");
                            Require(services[0]->Commands("catalog")==5 && services[1]->Commands("catalog")==4,
                                "Explicit resume did not publish/re-finalize one exact replacement on every fixed node");
                            Require(resumed.nodes.size()==initialCatalogs.size(),"Resume changed fixed node coverage");
                            for(std::size_t i=0;i<resumed.nodes.size();++i) {
                                const auto target=IO::String(ParseStrictJson(resumed.nodes[i].catalogJson).At("directory"));
                                Require(GetDAQ::InspectReceiverCatalog(target)==resumed.nodes[i].catalogJson,
                                    "Resumed formal catalog receipt does not match the published node catalog");
                                Require(resumed.nodes[i].catalogJson!=initialCatalogs[i],"Resume did not publish the prepared replacement catalog");
                            }
                            const auto resumeLog=IO::ReadFile(o.auditRoot+"/operations/cold-catalog-publish-resume/catalog-resume_finalize/remote.jsonl",1024*1024);
                            const auto observeLog=IO::ReadFile(o.auditRoot+"/operations/cold-catalog-publish-resume/catalog-resume_observe/remote.jsonl",1024*1024);
                            Require(resumeLog.find("\"phase\":\"complete\"")!=std::string::npos &&
                                observeLog.find("\"phase\":\"resume_node_terminal_failure\"")!=std::string::npos &&
                                observeLog.find("\"phase\":\"resume_node_proven_unattempted\"")!=std::string::npos,
                                "Resume audit did not preserve terminal failure and absent-operation evidence");
                            Require(r.w.actions.size()==hardwareActionsBeforeColdRecovery,"Cold catalog continuation wrote hardware");
                            IO::WriteNew(root+"/catalog-publish-resume-proof.json","{\"passed\":true,\"original_manager_destroyed\":true,\"remote_supervisors_kept_fixed_identity\":true,\"prior_failure_observed_by_original_id\":true,\"unattempted_node_absence_verified\":true,\"same_replacement_resumed\":true,\"all_nodes_catalog_receipts_verified\":true,\"real_hardware\":false}\n");
                        } else if (scenario=="catalog-finalize-failure") {
                            Require(plans.size()>=2 && initialCatalogs.size()==plans.size(),"Finalize-failure fixture requires two retained formal catalogs");
                            const auto operation=o.auditRoot+"/operations/manual/catalog-finalize-failure";
                            data.PrepareCatalogReplacement(operation,"Exp",1);
                            data.PublishCatalogReplacement(operation,"Exp",1);
                            const auto publishIntent=ParseStrictJson(IO::ReadFile(operation+"_publish/intent.json",1024*1024));
                            const auto replacementId=IO::String(publishIntent.At("requests").items.front().At("catalog_replacement_id"));
                            const auto firstTarget=IO::String(data.Status().nodes.front().catalogJson.empty() ?
                                ParseStrictJson(initialCatalogs.front()).At("directory") : ParseStrictJson(data.Status().nodes.front().catalogJson).At("directory"));
                            const auto slash=firstTarget.find_last_of('/');Require(slash!=std::string::npos,"Cannot identify first published catalog parent");
                            const auto leaf=firstTarget.substr(slash+1),parent=firstTarget.substr(0,slash);
                            const auto transactionHash=GetDAQ::ConfigurationSha256("getdaq.catalog-replacement-transaction:1\n"+replacementId).substr(0,24);
                            const auto transaction=parent+"/."+leaf+".replacement-"+transactionHash+".txn";
                            Require(::chmod(transaction.c_str(),0500)==0,"Cannot inject first-node finalize failure");
                            bool rejected=false;try { data.FinalizeCatalogReplacement(operation,"Exp",1); } catch(const std::exception &) { rejected=true; }
                            Require(::chmod(transaction.c_str(),0700)==0,"Cannot restore replacement transaction permissions");
                            Require(rejected && data.NeedsReconciliation(),"Finalize failure did not latch parent reconciliation");
                            Require(services[0]->Commands("catalog")==4 && services[1]->Commands("catalog")==3,
                                "Finalize failure was replayed or posted to a later data node");
                            const auto finalizeLog=IO::ReadFile(operation+"_finalize/remote.jsonl",1024*1024);
                            Require(finalizeLog.find("\"phase\":\"node_unknown\"")!=std::string::npos &&
                                finalizeLog.find("\"phase\":\"node_not_attempted\"")!=std::string::npos,
                                "Finalize failure audit omitted uncertain and not-attempted node evidence");
                            const auto hardwareActionsBeforeColdRecovery=r.w.actions.size();
                            eccOwner.reset();dataOwner.reset();
                            RejectColdCatalogWhenBoundaryChanges(remote,o,services,operation+"_publish","cold-catalog-boundary-reject");
                            unsigned int coldBoundaryChecks=0;
                            const auto resumed=RemoteReceiverManager::ResumeColdCatalogReplacement(remote,o,operation+"_publish",
                                "cold-catalog-finalize-resume","Exp",1,[&] {
                                    ++coldBoundaryChecks;
                                    Require(r.w.actions.size()==hardwareActionsBeforeColdRecovery,
                                        "Cold catalog continuation unexpectedly touched hardware");
                                });
                            Require(coldBoundaryChecks>4*remote.endpoints.size(),"Cold catalog recovery did not repeatedly recheck its caller boundary");
                            coldCompleted=true;coldBinding=resumed.controllerScopeSha256;
                            Require(!resumed.needsReconciliation,"Verified cold Finalize continuation left reconciliation latched");
                            Require(services[0]->Commands("catalog")==5 && services[1]->Commands("catalog")==4,
                                "Finalize resume repeated Publish or failed to finalize every fixed node");
                            for(const auto &node:resumed.nodes) {
                                const auto target=IO::String(ParseStrictJson(node.catalogJson).At("directory"));
                                Require(GetDAQ::InspectReceiverCatalog(target)==node.catalogJson,"Finalize resume changed/invalidated the published catalog receipt");
                            }
                            const auto resumeLog=IO::ReadFile(o.auditRoot+"/operations/cold-catalog-finalize-resume/catalog-resume_finalize/remote.jsonl",1024*1024);
                            const auto observeLog=IO::ReadFile(o.auditRoot+"/operations/cold-catalog-finalize-resume/catalog-resume_observe/remote.jsonl",1024*1024);
                            Require(resumeLog.find("\"phase\":\"complete\"")!=std::string::npos &&
                                observeLog.find("\"phase\":\"resume_node_already_complete\"")!=std::string::npos,
                                "Finalize resume did not bind to the all-success original Publish evidence");
                            Require(r.w.actions.size()==hardwareActionsBeforeColdRecovery,"Cold catalog continuation wrote hardware");
                            IO::WriteNew(root+"/catalog-finalize-resume-proof.json","{\"passed\":true,\"original_manager_destroyed\":true,\"remote_supervisors_kept_fixed_identity\":true,\"all_original_publish_nodes_complete\":true,\"failed_finalize_observed\":true,\"publish_not_repeated\":true,\"all_nodes_finalized\":true,\"real_hardware\":false}\n");
                        } else {
                        data.CatalogReceivers(o.auditRoot+"/operations/manual/catalog-replace","Exp",1,true);
                        const auto prepareIntent=ParseStrictJson(IO::ReadFile(o.auditRoot+"/operations/manual/catalog-replace_prepare/intent.json",1024*1024));
                        const auto publishIntent=ParseStrictJson(IO::ReadFile(o.auditRoot+"/operations/manual/catalog-replace_publish/intent.json",1024*1024));
                        const auto finalizeIntent=ParseStrictJson(IO::ReadFile(o.auditRoot+"/operations/manual/catalog-replace_finalize/intent.json",1024*1024));
                        Require(prepareIntent.At("requests").items.size()==plans.size() && publishIntent.At("requests").items.size()==plans.size() &&
                            finalizeIntent.At("requests").items.size()==plans.size(),"Remote replacement phases omitted a data host");
                        std::vector<std::string> replacementIds;
                        for(const auto &request:prepareIntent.At("requests").items) {
                            Require(IO::String(request.At("command"))=="catalog" && IO::Boolean(request.At("catalog_replace_existing")) &&
                                IO::String(request.At("catalog_phase"))=="prepare",
                                "Remote prepare intent did not bind the explicit Start authorization");
                            replacementIds.push_back(IO::String(request.At("catalog_replacement_id")));
                        }
                        for(std::size_t i=0;i<publishIntent.At("requests").items.size();++i) {
                            const auto &request=publishIntent.At("requests").items[i];
                            Require(IO::String(request.At("command"))=="catalog" && IO::Boolean(request.At("catalog_replace_existing")) &&
                                IO::String(request.At("catalog_phase"))=="publish" &&
                                replacementIds.at(i)==IO::String(request.At("catalog_replacement_id")),
                                "Remote publish intent differs from a prepared node replacement identity");
                        }
                        for(std::size_t i=0;i<finalizeIntent.At("requests").items.size();++i) {
                            const auto &request=finalizeIntent.At("requests").items[i];
                            Require(IO::String(request.At("command"))=="catalog" && IO::Boolean(request.At("catalog_replace_existing")) &&
                                IO::String(request.At("catalog_phase"))=="finalize" &&
                                replacementIds.at(i)==IO::String(request.At("catalog_replacement_id")),
                                "Finalize intent differs from a published node replacement identity");
                        }
                        const auto replaced=data.Status(); Require(!data.NeedsReconciliation() && replaced.nodes.size()==plans.size(),"Replacement left the parent receiver manager uncertain");
                        for(std::size_t i=0;i<replaced.nodes.size();++i) {
                            const auto receipt=ParseStrictJson(replaced.nodes[i].catalogJson);
                            const auto target=IO::String(receipt.At("directory"));
                            Require(IO::String(receipt.At("operation_id")).size()>0 && GetDAQ::InspectReceiverCatalog(target)==replaced.nodes[i].catalogJson,
                                "Remote replacement receipt differs from the published catalog");
                            const auto manifest=ParseStrictJson(IO::ReadFile(target+"/manifest.json",16*1024*1024));
                            Require(IO::Unsigned(manifest.At("schema"))==2,"Remote replacement did not publish schema 2");
                            std::set<std::string> retained;
                            for(const auto &file:manifest.At("sources").items.at(0).At("retained_files").items) retained.insert(IO::String(file.At("name")));
                            Require(!oldFiles.empty(),"Initial remote Run had no old raw files");
                            // Each node owns one CoBo source; retained names must be checked against that node's old seal.
                            const auto oldSeal=ParseStrictJson(IO::ReadFile(first.nodes[i].data.receivers[0].outputDirectory+"/.getdaq-receiver-seal.json",65536));
                            for(const auto &file:oldSeal.At("files").items) {
                                const auto name=IO::String(file.At("name")); struct stat info;
                                Require(retained.count(name) && !::lstat((target+"/source_0/"+name).c_str(),&info) && S_ISREG(info.st_mode),
                                    "Remote replacement discarded a nonmatching old raw");
                            }
                            const auto backup=IO::String(manifest.At("publication_backup"));
                            const auto backupReceipt=ParseStrictJson(IO::ReadFile(backup+"/catalog-complete.json",65536));
                            Require(IO::Boolean(backupReceipt.At("complete")) && IO::String(backupReceipt.At("directory"))==target,
                                "Remote replacement did not preserve the original catalog receipt in its backup");
                        }
                        IO::WriteNew(root+"/catalog-replacement-proof.json","{\"passed\":true,\"nodes\":"+std::to_string(replaced.nodes.size())+",\"new_catalogs_verified\":true,\"all_nodes_finalized\":true,\"old_nonmatching_raw_retained\":true,\"old_catalog_backups_retained\":true,\"real_hardware\":false}\n");
                        }
                    }
                    if (handoff) {
                        const auto old=data.Status();
                        std::map<std::string,std::string> oldFiles;
                        for(const auto &node:old.nodes)for(const auto &receiver:node.data.receivers) {
                            const auto seal=ParseStrictJson(IO::ReadFile(receiver.outputDirectory+"/.getdaq-receiver-seal.json",65536));
                            for(const auto &file:seal.At("files").items) {
                                const auto name=receiver.outputDirectory+'/'+IO::String(file.At("name"));
                                oldFiles[name]=GetDAQ::ConfigurationSha256(IO::ReadFile(name,1024*1024));
                            }
                        }
                        if(coldRetirement || coldHandoff) {
#include "remote_cold_completion_case.h"
                        } else if(failedRetirement) {
#include "remote_failed_plan_completion_case.h"
                        } else if(completeRetirement) {
#include "remote_plan_completion_case.h"
                        } else {
                        auto successorOptions=o;successorOptions.auditRoot=root+"/successor-controller";successorOptions.deploymentId="explicit-successor";
                        RemoteReceiverStatus target;
                        const auto path=o.auditRoot+"/operations/manual/control-handoff";
                        if (plannedHandoff) {
                            const auto inputs=root+"/successor-inputs";IO::CreateDirectory(inputs);
                            const auto credential=inputs+"/credential";IO::WriteNew(credential,IO::RandomSession()+"\n");
                            const auto file=GetDAQFixedEccFixture::WriteConfiguration(inputs,argv[1],successorOptions,remote,Port(),credential);
                            FixedEccConfiguration frozen(file);
                            const auto planFile=inputs+"/successor.json";
                            EccSuccessorPlan::Create(planFile,frozen);
                            auto plan=std::make_shared<EccSuccessorPlan>(planFile,frozen);
                            struct stat missing;Require(::lstat(successorOptions.auditRoot.c_str(),&missing)!=0,"Planning created successor root");
                            target.controllerScopeSha256=plan->ScopeSha256();target.controllerEpoch=plan->Epoch();
                            data.HandoffControl(path,target.controllerScopeSha256,target.controllerEpoch);
                            const auto receipt=data.CompletedControlHandoffJson();IO::WriteNew(root+"/control-handoff-result.json",receipt);
                            plan->ActivateForStartup();
                            const auto before=r.w.actions.size();
                            EccControlJobs jobs(frozen,plan,receipt);
                            const auto status=ParseStrictJson(jobs.StatusJson());const auto &adopted=status.At("data");
                            Require(adopted.At("controller_scope_sha256").text==target.controllerScopeSha256 &&
                                adopted.At("controller_epoch").text==target.controllerEpoch &&
                                adopted.At("needs_reconciliation").text=="false","Planned jobs did not adopt exact target identity");
                            Require(r.w.actions.size()==before,"Planned startup wrote hardware");
                            Require(IO::ReadFile(successorOptions.auditRoot+"/controller-handoff.json",1024*1024)==receipt,"Jobs lost original receipt");
                            IO::WriteNew(root+"/planned-startup-status.json",jobs.StatusJson());
                        } else {
                        RemoteReceiverManager successor(remote);NativeEccCoordinator next(successorOptions,successor);
                        target=successor.Status();
                        if(failedHandoffCompletion) {
                            const auto originalStatus=data.Status();
                            const auto failedId=ConfigurationSha256("getdaq.remote-data-operation:1\n"+originalStatus.controllerScopeSha256+'\n'+originalStatus.controllerEpoch+'\n'+path+"\nhandoff-control\n"+remote.endpoints[1].agentId);
                            const auto failedDirectory=plans[1].auditRoot+"/control/operations/"+failedId;
                            Rejects([&]{data.HandoffControl(path,target.controllerScopeSha256,target.controllerEpoch);},"Remote operation failed");
                            Require(services[0]->Operation(ConfigurationSha256("getdaq.remote-data-operation:1\n"+originalStatus.controllerScopeSha256+'\n'+originalStatus.controllerEpoch+'\n'+path+"\nhandoff-control\n"+remote.endpoints[0].agentId)).At("phase").text=="complete" &&
                                services[1]->Operation(failedId).At("phase").text=="failed" && services[0]->Commands("handoff-control")==1 && services[1]->Commands("handoff-control")==1,
                                "Injected handoff did not retain one complete sibling and one failed original node");
                            const auto originalRequestBytes=IO::ReadFile(failedDirectory+"/request.json",8192);
                            const auto originalRequest=DecodeSupervisorControlRequest(originalRequestBytes);
                            Require(::access((failedDirectory+"/handoff-proof.json").c_str(),F_OK)!=0,"Failed node unexpectedly published a handoff proof");
                            IO::CreateDirectory(o.auditRoot+"/operations/complete");
                            Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/node-repair-needed");},"explicit live failed-handoff completion");
                            Require(services[1]->Commands("complete-control-handoff")==0,"Parent silently repaired an accepted failed node");
                            auto repair=originalRequest;repair.command=SupervisorCommand::CompleteControlHandoff;
                            repair.failedHandoffOperationId=failedId;repair.operationId="operator-handoff-repair";
                            SupervisorHttpClient nodeClient(services[1]->direct);
                            Require(nodeClient.Submit(repair).code==202,"Explicit operator node handoff repair was not accepted");
                            JsonValue repairResult;Wait([&]{repairResult=services[1]->Operation(repair.operationId);const auto phase=IO::String(repairResult.At("phase"));
                                Require(phase!="failed" && phase!="unknown","Explicit node handoff repair failed");return phase=="complete";});
                            Require(services[1]->Operation(failedId).At("phase").text=="failed" &&
                                IO::ReadFile(failedDirectory+"/request.json",8192)==originalRequestBytes && services[1]->Commands("complete-control-handoff")==1,
                                "Explicit node repair rewrote the original failure/request or repeated its command");
                            const auto before=r.w.actions.size();
                            data.CompletePendingHandoff(o.auditRoot+"/operations/complete/explicit-node-repair");
                            Require(r.w.actions.size()==before,"Parent handoff completion wrote hardware");
                            const auto receipt=data.CompletedControlHandoffJson();IO::WriteNew(root+"/control-handoff-result.json",receipt);
                            const auto value=ParseStrictJson(receipt);const auto &nodes=value.At("nodes").items;
                            Require(nodes.size()==2 && IO::String(nodes[1].At("completion_operation_id"))==repair.operationId &&
                                IO::String(nodes[1].At("completion_request"))==EncodeSupervisorControlRequest(repair) &&
                                IO::String(nodes[1].At("proof_sha256"))==IO::String(repairResult.At("handoff").At("proof_sha256")),
                                "Parent receipt omitted the exact explicit node repair proof");
                            RemoteHandoffEvidence observed(path,data.BindingSha256(),originalStatus.controllerScopeSha256);
                            Require(observed.Nodes().size()==2 && observed.Nodes()[0].rejection==0 && observed.Nodes()[1].rejection==0,
                                "Parent evidence failed to record the explicit handoff repair");
                            Require(services[0]->Commands("handoff-control")==1 && services[1]->Commands("handoff-control")==1 &&
                                services[1]->Commands("complete-control-handoff")==1,"Parent reissued a node handoff/repair request");
                            auto foreign=receipt;const auto position=foreign.find(target.controllerEpoch);Require(position!=std::string::npos,"Successor epoch missing");
                            foreign.replace(position,target.controllerEpoch.size(),std::string(64,'0'));
                            Rejects([&]{successor.AdoptControlHandoff(foreign);},"not addressed");
                            successor.AdoptControlHandoff(receipt);
                            successor.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256);successor.VerifyStopped();
                            IO::CreateDirectory(successorOptions.auditRoot+"/operations/fresh");
                            successor.StartReceivers(successorOptions.auditRoot+"/operations/fresh/start",o.senders);
                            Require(successor.Status().nodes[0].data.sessionId!=old.nodes[0].data.sessionId,"Successor reused original completed session");
                            for (const auto &sender:o.senders) {auto &source=raw.at(sender.senderId);source->connect("TCP",sender.receiverAddress,Ice::Current());source->daqStart(Ice::Current());source->disconnect(Ice::Current());}
                            successor.FinishReceivers(successorOptions.auditRoot+"/operations/fresh/finish");successor.VerifyStopped();CheckFiles(successor.Status(),raw,2);
                            IO::WriteNew(root+"/failed-handoff-parent-result.json","{\"passed\":true,\"original_failure_retained\":true,\"operator_node_repair_count\":1,\"parent_repair_posts\":0,\"successor_adopted\":true,\"hardware_writes\":0,\"raw_forwarded\":false}\n");
                        } else if(scenario=="handoff-ack-loss" || completeHandoff) {
                            Rejects([&]{data.HandoffControl(path,target.controllerScopeSha256,target.controllerEpoch);},"Incomplete supervisor HTTP");
                            Rejects([&]{data.CompletedControlHandoffJson();},"No complete");
                            if(!completeHandoff) {
                                successor.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256);
                                Rejects([&]{successor.VerifyStopped();},"unbound previous cycle");
                            } else {
                                RemoteHandoffEvidence uncertain(path,data.BindingSha256(),data.Status().controllerScopeSha256);
                                Require(uncertain.Nodes().size()==2 && uncertain.Nodes()[0].rejection==0 && uncertain.Nodes()[1].rejection==0,"Cold handoff review lost uncertain/accepted delivery");
                                IO::CreateDirectory(o.auditRoot+"/operations/complete");
                                const auto before=r.w.actions.size();
                                relays.front()->hideOperations=true;
                                Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/ambiguous-404");},"delivery was uncertain");
                                for(const auto &service:services)Require(service->Commands("handoff-control")==1,"Ambiguous 404 caused another POST");
                                relays.front()->hideOperations=false;
                                data.CompletePendingHandoff(o.auditRoot+"/operations/complete/data-handoff");
                                RemoteHandoffEvidence observed(path,data.BindingSha256(),data.Status().controllerScopeSha256);
                                Require(observed.Nodes()[0].rejection==0 && observed.Nodes()[1].rejection==0,"Later GET-only completion became replayable");
                                Require(r.w.actions.size()==before,"Handoff completion wrote hardware");
                                Rejects([&]{data.CompletePendingHandoff(o.auditRoot+"/operations/complete/again");},"retained original unfinished");
                            }
                        }
                        if(scenario!="handoff-ack-loss" && !failedHandoffCompletion) {
                            if(!completeHandoff)data.HandoffControl(path,target.controllerScopeSha256,target.controllerEpoch);
                            const auto receipt=data.CompletedControlHandoffJson();
                            IO::WriteNew(root+"/control-handoff-result.json",receipt);
                            auto foreign=receipt;const auto position=foreign.find(target.controllerEpoch);Require(position!=std::string::npos,"Successor epoch missing");
                            foreign.replace(position,target.controllerEpoch.size(),std::string(64,'0'));
                            Rejects([&]{successor.AdoptControlHandoff(foreign);},"not addressed");
                            successor.AdoptControlHandoff(receipt);
                            if(scenario=="handoff-receipt-fault") {
                                const auto file=successorOptions.auditRoot+"/controller-handoff.json";
                                {std::ofstream changed(file,std::ios::app);changed<<' ';}
                                Rejects([&]{successor.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256);},"file changed");
                                {std::ofstream restore(file,std::ios::trunc);restore<<receipt;}
                                Rejects([&]{successor.VerifyStopped();},"UNKNOWN");
                            } else {
                                successor.Validate(EccCommand::Configure,EccState::Prepared,o.senders,artifact.configurationSha256,artifact.artifactSha256);successor.VerifyStopped();
                                IO::CreateDirectory(successorOptions.auditRoot+"/operations/fresh");
                                successor.StartReceivers(successorOptions.auditRoot+"/operations/fresh/start",o.senders);
                                Require(successor.Status().nodes[0].data.sessionId!=old.nodes[0].data.sessionId,"Successor reused original completed session");
                                for (const auto &sender:o.senders) {auto &source=raw.at(sender.senderId);source->connect("TCP",sender.receiverAddress,Ice::Current());source->daqStart(Ice::Current());source->disconnect(Ice::Current());}
                                successor.FinishReceivers(successorOptions.auditRoot+"/operations/fresh/finish");successor.VerifyStopped();CheckFiles(successor.Status(),raw,2);
                            }
                        }
                        }
                        Require(data.NeedsReconciliation(),"Original manager was not permanently fenced");
                        Rejects([&]{data.HandoffControl(path+"-again",target.controllerScopeSha256,target.controllerEpoch);},"prior attempt");
                        Rejects([&]{data.VerifyStopped();},"UNKNOWN");
                        for(const auto &service:services)Require(service->Commands("handoff-control")==1,"Handoff was repeated or a node omitted");
                        for(const auto &entry:oldFiles)Require(GetDAQ::ConfigurationSha256(IO::ReadFile(entry.first,1024*1024))==entry.second,"Old raw changed after handoff");
                        IO::WriteNew(root+"/handoff-proof.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(encodingMinor)+
                            ",\"reply_loss\":"+((scenario=="handoff-ack-loss" || completeHandoff) ? "true" : "false")+",\"original_manager_fenced\":true,\"old_raw_retained\":true,\"raw_forwarded\":false}\n");
                        }
                    }

                }
            } else {
                ecc.Execute(EccCommand::Describe,"describe"); ecc.Execute(EccCommand::Prepare,"prepare");
                IO::Fd occupied;
                if (scenario=="partial-start") { occupied.value=::socket(AF_INET,SOCK_STREAM,0); sockaddr_in a={}; a.sin_family=AF_INET; a.sin_addr.s_addr=htonl(INADDR_LOOPBACK); a.sin_port=htons(plans[1].sources[0].dataPort); Require(occupied.value>=0 && !::bind(occupied.value,reinterpret_cast<sockaddr *>(&a),sizeof(a)) && !::listen(occupied.value,1),"Occupy second remote data port"); }
                if (scenario=="partial-start" || scenario=="start-ack-loss") {
                    Rejects([&] { ecc.Execute(EccCommand::Configure,"configure"); },scenario=="partial-start" ? "Remote operation failed" : "Incomplete supervisor HTTP");
                    Require(ecc.Status().state==EccState::Prepared && ecc.Status().outcomeUnknown && data.NeedsReconciliation(),"Partial/lost start committed Ready");
                    Require(!r.Has("C0.sender-connect") && !r.Has("C1.sender-connect"),"Partial receiver start connected producer data");
                    Require(services[0]->Commands("start")==1 && services[1]->Commands("start")==unsigned(scenario=="partial-start"),"Remote Start was replayed/continued after uncertainty");
                    for (auto &service:services) service->Capture();
                    Wait([&] { return DecodeSupervisorStatus(services[0]->Status().At("lifecycle")).state==DataSupervisorStatus::Ready; });
                    const auto actual=DecodeSupervisorStatus(services[0]->Status().At("lifecycle")); Require(!actual.receivers.empty(),"Actual first remote receiver was killed/not started");
                    if (scenario=="start-ack-loss") {
                        const auto intent=ParseStrictJson(IO::ReadFile(o.auditRoot+"/operations/configure/receivers-start/intent.json",1024*1024));
                        const auto id=IO::String(intent.At("requests").items.at(0).At("operation_id"));
                        Wait([&] { return IO::String(services[0]->Operation(id).At("phase"))=="complete"; });
                        Require(ecc.Status().outcomeUnknown && data.NeedsReconciliation(),"Independent confirmed start result cleared parent UNKNOWN");
                    }
                    Rejects([&] { ecc.Execute(EccCommand::Configure,"repeat"); },"UNKNOWN");
                } else {
                    ecc.Execute(EccCommand::Configure,"configure"); State(ecc,EccState::Ready); const auto session=data.Status(); for (auto &service:services) service->Capture();
                    r.Clear(); ecc.Execute(EccCommand::Start,"start"); State(ecc,EccState::Running); r.Index(index,o.auditRoot+"/operations/start/start","start");
                    if (scenario=="service-restart" || scenario=="service-restart-paused") {
                        if (scenario=="service-restart-paused") { ecc.Execute(EccCommand::Pause,"pause"); State(ecc,EccState::Paused); }
                        services[0]->CrashAndRestart(); r.Clear(); Rejects([&] { ecc.Execute(EccCommand::Stop,"stop"); },"instance changed");
                        Require(ecc.Status().state==(scenario=="service-restart" ? EccState::Running : EccState::Paused) && ecc.Status().outcomeUnknown && data.NeedsReconciliation(),"Service restart allowed positive control continuation");
                        Require(!r.Has("C0.sender-disconnect"),"Remote failure emitted unconfirmed EOF");
                        if (scenario=="service-restart") Require(r.Has("C0.sender-stop") && r.Has("C1.sender-stop"),"Remote failure blocked production stop");
                    } else {
                        r.Clear(); ecc.Execute(EccCommand::Pause,"pause"); State(ecc,EccState::Paused);
                        r.Clear(); ecc.Execute(EccCommand::Resume,"resume"); State(ecc,EccState::Running);
                        r.Index(index,o.auditRoot+"/operations/resume/resume","resume"); ecc.Execute(EccCommand::Stop,"stop"); State(ecc,EccState::Ready);
                        r.Clear(); ecc.Execute(EccCommand::Start,"restart"); r.Index(index,o.auditRoot+"/operations/restart/start","start"); ecc.Execute(EccCommand::Stop,"stop-again");
                        Require(data.Status().nodes[0].data.sessionId==session.nodes[0].data.sessionId && data.Status().nodes[1].data.sessionId==session.nodes[1].data.sessionId,"Stop/Start replaced remote receiver cycles");
                        if (scenario=="truncated") raw.at("C0")->truncate=true;
                        if (scenario=="truncated" || scenario=="finish-ack-loss") {
                            Rejects([&] { ecc.Execute(EccCommand::Breakup,"breakup"); },scenario=="truncated" ? "abnormal exit" : "Incomplete supervisor HTTP");
                            Require(ecc.Status().state==EccState::Ready && ecc.Status().outcomeUnknown && data.NeedsReconciliation(),"Uncertain remote finish committed Prepared");
                            Require(services[0]->Commands("finish")==1 && services[1]->Commands("finish")==1 && data.Status().nodes[1].data.state==DataSupervisorStatus::Stopped,"Independent second EOF/seal not preserved after first node failed");
                            if (scenario=="finish-ack-loss") {
                                const auto intent=ParseStrictJson(IO::ReadFile(o.auditRoot+"/operations/breakup/receivers-finish/intent.json",1024*1024));
                                const auto id=IO::String(intent.At("requests").items.at(0).At("operation_id"));
                                Wait([&] { return IO::String(services[0]->Operation(id).At("phase"))=="complete"; });
                                Require(ecc.Status().outcomeUnknown && data.NeedsReconciliation(),"Independent confirmed seal result cleared parent UNKNOWN");
                            }
                            Rejects([&] { ecc.Execute(EccCommand::Breakup,"repeat"); },"UNKNOWN");
                        } else {
                            ecc.Execute(EccCommand::Breakup,"breakup"); State(ecc,EccState::Prepared); CheckFiles(data.Status(),raw,6);
                            ecc.Execute(EccCommand::Configure,"configure-second"); State(ecc,EccState::Ready);
                            Require(data.Status().nodes[0].data.sessionId!=session.nodes[0].data.sessionId && data.Status().nodes[1].data.sessionId!=session.nodes[1].data.sessionId,"Second Configure reused distributed sessions");
                            ecc.Execute(EccCommand::Start,"start-second"); ecc.Execute(EccCommand::Stop,"stop-second"); ecc.Execute(EccCommand::Breakup,"breakup-second"); CheckFiles(data.Status(),raw,2);
                            ecc.Execute(EccCommand::Reset,"unprepare"); State(ecc,EccState::Described); ecc.Execute(EccCommand::Reset,"forget"); State(ecc,EccState::Idle);
                        }
                    }
                }
            }
            for (auto &service:services) service->Capture();
            std::ofstream evidence(std::string(argv[5])+".root.json"); evidence<<"{\"scenario\":"<<EncodeJsonString(scenario)<<",\"root\":"<<EncodeJsonString(root)
                <<",\"real_control_services\":2,\"real_receivers\":true,\"physical_hosts\":1,\"real_hardware\":false,\"ecc_unknown\":"<<(!eccOwner || ecc.Status().outcomeUnknown ? "true" : "false")
                <<",\"provider_needs_reconciliation\":"<<(!dataOwner || data.NeedsReconciliation() ? "true" : "false")<<",\"binding_sha256\":"<<EncodeJsonString(coldCompleted ? coldBinding : data.BindingSha256())<<"}\n";
            evidence.flush(); Require(bool(evidence),"Save remote receiver evidence index");
        } catch (...) { for (auto &sender:raw) sender.second->FixtureClose(); for (auto &service:services) try { service->Capture(); } catch (...) {} throw; }
        for (auto &sender:raw) sender.second->FixtureClose();
        for (const auto &relay:relays) Require(relay->failure.empty() && relay->dropped==(coldRetirement ? 0 : 1),"Fixture relay fault injection did not execute as planned");
        relays.clear(); services.clear();
        adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy(); communicator->destroy();
        std::cout<<"PASS native ECC remote receiver "<<scenario<<"; 2 actual service processes, direct raw TCP; synthetic localhost, no hardware/throughput claim\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1; }
}
