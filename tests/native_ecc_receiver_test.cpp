#include "receiverprocessmanager.h"
#include "native_receiver_fixture.h"
#include "configurationsha256.h"
#include "supervisorio.h"
#include "system_acquisition_fixture.h"
#include <arpa/inet.h>
#include <chrono>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
namespace {
using namespace GetDAQReceiverFixture;
namespace IO=GetDAQ::SupervisorIO;
void CheckFiles(ReceiverProcessManager &data,const std::map<std::string,IceUtil::Handle<RawSender>> &senders,const DataSupervisorOptions &plan,unsigned int expectedFrames) {
    const auto status=data.Status(); Require(status.state==DataSupervisorStatus::Stopped && !status.needsReconciliation,"Actual process seals did not commit");
    for (const auto &c:status.receivers) {
        Require(c.reaped && c.exitCode==0 && c.sealVerified && c.frames==expectedFrames,"Managed final process/frame status differs");
        const auto seal=ParseStrictJson(IO::ReadFile(c.outputDirectory+"/.getdaq-receiver-seal.json",65536));
        Require(IO::String(seal.At("configuration_sha256"))==plan.configurationSha256 && IO::String(seal.At("artifact_sha256"))==plan.artifactSha256 &&
                IO::String(seal.At("sender_id"))==c.id && IO::String(seal.At("session_id"))==status.sessionId && IO::Unsigned(seal.At("pid"))==unsigned(c.pid) &&
                IO::Boolean(seal.At("sender_eof_confirmed"))==!c.sourceUnusedConfirmed,"Seal differs from independently retained parent plan/PID/source/session");
        if(c.sourceUnusedConfirmed) Require(expectedFrames==0 && IO::Boolean(seal.At("source_unused_confirmed")) && IO::Unsigned(seal.At("connections"))==0,"Local unused source fabricated EOF/frames");
        std::map<unsigned long long,std::string> ordered;
        for(const auto &file:seal.At("files").items) {const auto name=IO::String(file.At("name"));const auto underscore=name.rfind('_');Require(underscore!=std::string::npos && ordered.emplace(std::stoull(name.substr(underscore+1,name.size()-underscore-6)),name).second,"Local file sequence differs");}
        std::string actual;for(const auto &file:ordered) actual+=IO::ReadFile(c.outputDirectory+'/'+file.second,1024*1024);
        Require(actual==senders.at(c.id)->Expected(),"Native ECC cycle raw files differ from independent fixture bytes");
    }
}
void Cleanup(ReceiverProcessManager &data,std::map<std::string,IceUtil::Handle<RawSender>> &senders) {
    for (auto &s:senders) s.second->FixtureClose();
    for (const auto &c:data.Status().receivers) if (c.pid>0 && !c.reaped) {
        siginfo_t info={}; if (::waitid(P_PID,id_t(c.pid),&info,WEXITED|WNOHANG|WNOWAIT)) continue;
        if (!info.si_pid) ::kill(c.pid,SIGTERM);
        int exitCode;
        while (::waitpid(c.pid,&exitCode,0)<0 && errno==EINTR) {}
    }
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN); Ice::CommunicatorPtr communicator;
    try {
        Require(argc==7,"Pass CoBo/MuTaNT fixtures, receiver, evidence index, --scenario and scenario"); const std::string scenario=argv[6];
        Require(std::string(argv[5])=="--scenario" && (scenario=="normal-0" || scenario=="normal-1" || scenario=="fdt-0" || scenario=="fdt-empty-0" || scenario=="truncated" || scenario=="disconnect-ack-loss"),"Unknown actual receiver scenario");
        const bool fdt=scenario=="fdt-0" || scenario=="fdt-empty-0",unused=scenario=="fdt-empty-0";
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4"); communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("NativeReceiverFixture","tcp -h 127.0.0.1 -p 0");
        Rig r(communicator,adapter,cobo,mutant,scenario=="normal-0" || fdt ? 0 : 1); r.options.mutants.clear();
        r.senders.erase(std::remove_if(r.senders.begin(),r.senders.end(),[](const DataSenderOptions &s) { return s.senderId[0]!='C'; }),r.senders.end());
        char parent[]="/tmp/getdaq-native-receivers-XXXXXX"; Require(::mkdtemp(parent),"Create actual receiver evidence parent"); const std::string root=parent; IO::CreateDirectory(root+"/outputs");
        IO::WriteNew(std::string(argv[4])+".fixture-root.json","{\"directory\":"+JsonString(root)+"}\n"); // 数据启动前发布本夹具私有根，失败也可有据清理。
        DataSupervisorOptions plan; plan.auditRoot=root+"/supervisor"; plan.deploymentId="actual-receiver-fixture"; plan.receiverExecutable=argv[3];
        plan.startupTimeoutMs=3000; plan.finishTimeoutMs=3000; plan.statusTimeoutMs=1000;
        std::set<unsigned int> ports; const auto port=[&] { unsigned int p; do { p=Port(); } while (!ports.insert(p).second); return p; };
        std::map<std::string,IceUtil::Handle<RawSender>> raw;
        for (auto &s:r.senders) {
            raw[s.senderId]=new RawSender(r.w,s.senderId); s.controlProxy=communicator->proxyToString(adapter->addWithUUID(raw[s.senderId]));
            if(fdt) {s.protocol="FDT";raw[s.senderId]->expectedProtocol="FDT";}
            ReceiverSourcePlan source; source.id=s.senderId;if(fdt) source.inputProtocol="FDT"; source.bindAddress="127.0.0.1"; source.dataPort=port(); source.statusPort=port();
            source.receiverAddress=s.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort); source.outputRoot=root+"/outputs"; source.queueMiB=source.maxFrameMiB=1; plan.sources.push_back(source);
        }
        auto o=Options(r,root); const auto artifact=DecodeSystemExperimentArtifact(o.experimentArtifactBytes);
        plan.configurationSha256=artifact.configurationSha256; plan.artifactSha256=artifact.artifactSha256;
        adapter->activate(); ReceiverProcessManager data(plan); NativeEccCoordinator ecc(o,data); std::ofstream index(argv[4]); Require(bool(index),"Create native/actual receiver evidence index");
        try {
            auto wrongSenders=o.senders; wrongSenders[0].receiverAddress="127.0.0.1:1";
            Rejects([&] { data.Validate(EccCommand::Configure,EccState::Prepared,wrongSenders,plan.configurationSha256,plan.artifactSha256); },"target differs");
            wrongSenders=o.senders;wrongSenders[0].protocol="ZBUF";
            Rejects([&] {data.Validate(EccCommand::Configure,EccState::Prepared,wrongSenders,plan.configurationSha256,plan.artifactSha256);},"target differs");
            Rejects([&] { data.Validate(EccCommand::Configure,EccState::Prepared,o.senders,std::string(64,'c'),plan.artifactSha256); },"artifact differs");
            Require(data.Status().receivers.empty() && ecc.Operations().empty(),"Invalid receiver binding started hardware/processes");
            Require(ParseStrictJson(IO::ReadFile(o.auditRoot+"/snapshot.json",1024*1024)).At("data_lifecycle_binding_sha256").text==data.BindingSha256(),"Parent snapshot omitted actual data plan binding");
            ecc.Execute(EccCommand::Describe,"describe"); ecc.Execute(EccCommand::Prepare,"prepare"); ecc.Execute(EccCommand::Configure,"configure"); State(ecc,EccState::Ready);
            const auto session=data.Status(); Require(session.receivers.size()==2,"Configure did not start actual receivers");
            if(unused) {
                for(const auto &entry:raw) Require(!entry.second->Connected(),"Local FDT Connect fabricated data connection");
                ecc.Execute(EccCommand::Breakup,"breakup-unused");State(ecc,EccState::Prepared);CheckFiles(data,raw,plan,0);
                ecc.Execute(EccCommand::Reset,"unprepare");State(ecc,EccState::Described);ecc.Execute(EccCommand::Reset,"forget");State(ecc,EccState::Idle);
            } else {
            r.Clear(); ecc.Execute(EccCommand::Start,"start"); State(ecc,EccState::Running); r.Index(index,o.auditRoot+"/operations/start/start","start");
            r.Clear(); ecc.Execute(EccCommand::Pause,"pause"); State(ecc,EccState::Paused);
            for (const auto &c:session.receivers) {
                const auto i=std::find_if(plan.sources.begin(),plan.sources.end(),[&](const ReceiverSourcePlan &s) { return s.id==c.id; });
                const auto status=ParseStrictJson(IO::HttpStatus(i->statusPort,1000));
                Require(IO::Boolean(status.At("connected"))==!fdt && IO::Unsigned(status.At("connections"))==1 && IO::Boolean(status.At("sender_eof_confirmed"))==fdt,"Pause connection/EOF protocol boundary differs");
            }
            r.Clear(); ecc.Execute(EccCommand::Resume,"resume"); State(ecc,EccState::Running); r.Index(index,o.auditRoot+"/operations/resume/resume","resume");
            r.Clear(); ecc.Execute(EccCommand::Stop,"stop"); State(ecc,EccState::Ready);
            r.Before("C1.ctrl.pipeCtrl.intEnable=0","C0.sender-stop"); Require(!r.Has("C0.sender-disconnect"),"Stop emitted EOF");
            r.Clear(); ecc.Execute(EccCommand::Start,"restart"); State(ecc,EccState::Running); Require(data.Status().sessionId==session.sessionId,"Stop/Start replaced receiver cycle");
            r.Index(index,o.auditRoot+"/operations/restart/start","start"); ecc.Execute(EccCommand::Stop,"stop-again");
            if (scenario=="truncated") raw.at("C0")->truncate=true;
            if (scenario=="disconnect-ack-loss") raw.at("C0")->failDisconnect=true;
            if (scenario=="truncated" || scenario=="disconnect-ack-loss") {
                Rejects([&] { ecc.Execute(EccCommand::Breakup,"breakup"); },scenario=="truncated" ? "abnormal exit" : "UNKNOWN");
                Require(ecc.Status().state==EccState::Ready && ecc.Status().outcomeUnknown,"Failed data/control outcome committed Prepared");
                if (scenario=="truncated") {
                    const auto failed=data.Status(); Require(failed.state==DataSupervisorStatus::Failed,"Truncated stream did not fail data lifecycle");
                    for (const auto &c:failed.receivers) Require(c.sealVerified==(c.id=="C1"),"Truncated stream did not preserve independent seal failure/success");
                }
                else {
                    Require(data.Status().state==DataSupervisorStatus::Ready && r.Count("C0.sender-disconnect")==1 && !r.Has("C1.sender-disconnect"),"Lost ACK replayed disconnect or automatically finished data lifecycle");
                    // 独立检查C0确有EOF/完整文件；这个事实不能清除父控制UNKNOWN。
                    const auto c=std::find_if(session.receivers.begin(),session.receivers.end(),[](const ManagedReceiverStatus &s) { return s.id=="C0"; });
                    const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
                    for (;;) {
                        siginfo_t info={}; Require(!::waitid(P_PID,id_t(c->pid),&info,WEXITED|WNOHANG|WNOWAIT),"Observe independently sealed receiver");
                        if (info.si_pid==c->pid) { Require(info.si_code==CLD_EXITED && info.si_status==0,"Actual EOF after lost control ACK did not exit normally"); break; }
                        Require(std::chrono::steady_clock::now()<deadline,"Independent EOF seal timeout"); std::this_thread::sleep_for(std::chrono::milliseconds(10));
                    }
                    const auto seal=ParseStrictJson(IO::ReadFile(c->outputDirectory+"/.getdaq-receiver-seal.json",65536));
                    Require(IO::Boolean(seal.At("sender_eof_confirmed")) && IO::String(seal.At("session_id"))==session.sessionId &&
                            IO::String(seal.At("configuration_sha256"))==plan.configurationSha256 && IO::String(seal.At("artifact_sha256"))==plan.artifactSha256,"Lost ACK actual seal identity/EOF differs");
                    std::string actual; for (const auto &f:seal.At("files").items) actual+=IO::ReadFile(c->outputDirectory+'/'+IO::String(f.At("name")),1024*1024);
                    Require(actual==raw.at("C0")->Expected() && ecc.Status().outcomeUnknown && !data.Status().receivers[0].sealVerified,
                            "Independent intact seal cleared controller UNKNOWN or fabricated lifecycle completion");
                }
                Rejects([&] { ecc.Execute(EccCommand::Breakup,"breakup-repeat"); },"UNKNOWN");
            } else {
                ecc.Execute(EccCommand::Breakup,"breakup"); State(ecc,EccState::Prepared); CheckFiles(data,raw,plan,6);
                ecc.Execute(EccCommand::Configure,"configure-second"); State(ecc,EccState::Ready); Require(data.Status().sessionId!=session.sessionId,"Next Configure reused receiver identity");
                ecc.Execute(EccCommand::Start,"start-second"); ecc.Execute(EccCommand::Stop,"stop-second"); ecc.Execute(EccCommand::Breakup,"breakup-second"); CheckFiles(data,raw,plan,2);
                ecc.Execute(EccCommand::Reset,"unprepare"); State(ecc,EccState::Described); ecc.Execute(EccCommand::Reset,"forget"); State(ecc,EccState::Idle);
            }
            }
            const auto status=ecc.Status();
            std::ofstream evidence(std::string(argv[4])+".root.json");
            evidence<<"{\"scenario\":"<<EncodeJsonString(scenario)<<",\"root\":"<<EncodeJsonString(root)
                <<",\"real_receiver_executed\":true,\"data_lifecycle_test_double\":false,\"real_hardware\":false,\"ecc_unknown\":"<<(status.outcomeUnknown ? "true" : "false")<<"}\n";
            evidence.flush(); Require(bool(evidence),"Save actual receiver root index");
        } catch (...) { Cleanup(data,raw); throw; }
        Cleanup(data,raw); adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy(); communicator->destroy();
        std::cout<<"PASS native ECC + actual receiver "<<scenario<<": persistent parent/plan/process identity, actual TCP EOF/seal; synthetic hardware and frames, no throughput/physical proof\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1; }
}
