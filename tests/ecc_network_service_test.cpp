#include "hardwarestateprotocol.h"
#include "eccsuccessorplan.h"
#include "fixed_ecc_fixture.h"
#include "native_remote_service_fixture.h"
#include "receivercatalog.h"
#include "receiver_observation_barrier.h"
#ifdef GETDAQ_TEST_ICE_SOURCE
#include "source_observation_barrier.h"
#endif
#include <algorithm>
#include <iostream>
#include <fstream>
#include <future>
#include <dirent.h>
#include <iomanip>
#include <sstream>
namespace {
using namespace GetDAQFixedEccFixture;
using namespace GetDAQRemoteServiceFixture;
class HangingObservationEndpoint {
    IO::Fd fListener;
    std::atomic<bool> fStop{false};
    std::thread fWorker;
public:
    std::atomic<bool> accepted{false};
    explicit HangingObservationEndpoint(unsigned int port) : fListener(::socket(AF_INET,SOCK_STREAM,0)) {
        const int reuse=1; sockaddr_in address={}; address.sin_family=AF_INET; address.sin_addr.s_addr=htonl(INADDR_LOOPBACK); address.sin_port=htons(port);
        Require(fListener.value>=0 && !::setsockopt(fListener.value,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)) &&
            !::bind(fListener.value,reinterpret_cast<sockaddr *>(&address),sizeof(address)) && !::listen(fListener.value,1),"Own hanging observation listener");
        fWorker=std::thread([this] {
            while (!fStop) {
                pollfd ready={fListener.value,POLLIN,0}; if (::poll(&ready,1,20)<=0) continue;
                IO::Fd socket(::accept(fListener.value,nullptr,nullptr)); if (socket.value<0) continue;
                accepted=true; while (!fStop) std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
        });
    }
    ~HangingObservationEndpoint() { fStop=true; if(fWorker.joinable()) fWorker.join(); }
};
class EccProcess {
    std::string executable,configuration,root;
    std::vector<std::string> startupArguments;
    unsigned int generation=0;
public:
    int pid=-1;
    SupervisorHttpOptions direct;
    EccProcess(const std::string &exe,const std::string &file,const std::string &directory,const SupervisorHttpOptions &endpoint,
               const std::vector<std::string> &arguments={})
        : executable(exe),configuration(file),root(directory),startupArguments(arguments),direct(endpoint) { Start(); }
    ~EccProcess() { try { Stop(); } catch (...) { if (pid>0) { ::kill(pid,SIGKILL); ::waitpid(pid,nullptr,0); } } }
    void Start() {
        IO::Fd log(::open((root+"/ecc-service-"+std::to_string(generation++)+".log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600)); Require(log.value>=0,"ECC process log");
        posix_spawn_file_actions_t actions; Require(!::posix_spawn_file_actions_init(&actions),"ECC spawn actions");
        ::posix_spawn_file_actions_adddup2(&actions,log.value,STDOUT_FILENO); ::posix_spawn_file_actions_adddup2(&actions,log.value,STDERR_FILENO);
        std::vector<std::string> words={executable,"--config",configuration};words.insert(words.end(),startupArguments.begin(),startupArguments.end());
        std::vector<char *> argv; for (auto &word:words) argv.push_back(&word[0]); argv.push_back(nullptr);
        pid_t child=0; const auto result=::posix_spawn(&child,executable.c_str(),&actions,nullptr,argv.data(),environ); ::posix_spawn_file_actions_destroy(&actions);
        Require(!result && child>0,"Spawn actual ECC network process"); pid=child;
        try { Wait([&] { int status=0; const auto exited=::waitpid(pid,&status,WNOHANG); if (exited==pid) pid=-1; Require(!exited,"ECC service startup exited");
                try { return NativeControlHttpClient(direct).Identity().code==200; } catch (...) { return false; } }); }
        catch (...) { if (pid>0) { ::kill(pid,SIGKILL); ::waitpid(pid,nullptr,0); pid=-1; } throw; }
    }
    void Stop() { if (pid>0) { Require(!::kill(pid,SIGTERM),"ECC graceful signal"); int status=0; Require(::waitpid(pid,&status,0)==pid,"Reap ECC process"); pid=-1; Require(WIFEXITED(status) && !WEXITSTATUS(status),"ECC normal exit failed"); } }
    void ShutdownAfterHandoff(const JsonValue &identity,const std::string &handoffOperationId) {
        const auto request=NativeControlHttpClient(direct).ShutdownEccService(IO::String(identity.At("instance_id")),
            IO::String(identity.At("snapshot_sha256")),handoffOperationId);
        if(request.code!=200)throw std::runtime_error("ECC refused clean shutdown after completed handoff: "+request.body);
        int status=0;const pid_t child=pid;
        Wait([&] {const auto result=::waitpid(child,&status,WNOHANG);if(result==child){pid=-1;return true;}
            if(result<0 && errno!=EINTR)throw std::runtime_error("Cannot wait for clean ECC service shutdown");
            return false;},10000);
        Require(pid==-1 && WIFEXITED(status) && !WEXITSTATUS(status),"ECC shutdown endpoint did not exit normally");
    }
    void CrashAndRestart() { Require(pid>0 && !::kill(pid,SIGKILL),"ECC test crash"); Require(::waitpid(pid,nullptr,0)==pid,"Reap crashed ECC"); pid=-1; Start(); }
};
void HandoffTool(const std::string &executable,const std::vector<std::string> &arguments,const std::string &logFile,bool success=true) {
    IO::Fd log(::open(logFile.c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600));Require(log.value>=0,"Create handoff tool log");
    posix_spawn_file_actions_t actions;Require(!::posix_spawn_file_actions_init(&actions),"Handoff tool spawn actions");
    ::posix_spawn_file_actions_adddup2(&actions,log.value,STDOUT_FILENO);::posix_spawn_file_actions_adddup2(&actions,log.value,STDERR_FILENO);
    std::vector<std::string> words={executable};words.insert(words.end(),arguments.begin(),arguments.end());
    std::vector<char *> argv;for (auto &word:words)argv.push_back(&word[0]);argv.push_back(nullptr);
    pid_t child=0;const auto result=::posix_spawn(&child,executable.c_str(),&actions,nullptr,argv.data(),environ);::posix_spawn_file_actions_destroy(&actions);
    Require(!result && child>0,"Spawn actual handoff tool");int status=0;
    Require(::waitpid(child,&status,0)==child && WIFEXITED(status) && (WEXITSTATUS(status)==0)==success,
        ("Handoff tool result differs: "+IO::ReadFile(logFile,65536)).c_str());
}
JsonValue Read(const SupervisorHttpResponse &response) {
    Require(response.code==200,("ECC metadata response "+std::to_string(response.code)+": "+response.body).c_str());
    JsonLimits limits; limits.stringBytes=1024*1024; return ParseStrictJson(response.body,limits);
}
EccControlRequest Request(const JsonValue &identity,const std::string &command,const std::string &id) {
    EccControlRequest r; r.operationId=id; r.command=command; r.instanceId=IO::String(identity.At("instance_id")); r.snapshotSha256=IO::String(identity.At("snapshot_sha256"));
    r.clientId="test-console"; r.clientEpoch=std::string(64,'a'); return r;
}
JsonValue Terminal(NativeControlHttpClient &client,const std::string &id) {
    JsonValue op; Wait([&] { op=Read(client.Operation(id)); const auto phase=IO::String(op.At("phase")); return phase=="complete" || phase=="failed" || phase=="unknown"; },180000); return op;
}
void State(NativeControlHttpClient &client,EccState state) {
    const auto status=Read(client.Status()); const auto &c=status.At("controller");
    Require(IO::Unsigned(c.At("state"))==unsigned(state) && !IO::Boolean(c.At("outcome_unknown")) && !IO::Boolean(c.At("needs_reconciliation")) &&
        !IO::Boolean(status.At("control_needs_reconciliation")) && !IO::Boolean(status.At("operation_active")),"Network ECC state did not commit");
}
std::string ReplacementBackupPath(const std::string &catalogRoot,const std::string &experiment,unsigned int run,const std::string &deploymentId) {
    std::ostringstream number;number<<"run_"<<std::setw(4)<<std::setfill('0')<<run;
    const auto parent=catalogRoot+'/'+experiment+'/'+number.str();DIR *directory=::opendir(parent.c_str());
    Require(directory!=nullptr,"Open catalog Run parent while locating prepared replacement");
    std::string backup;struct dirent *entry=nullptr;
    while((entry=::readdir(directory))!=nullptr) {
        const std::string name=entry->d_name;
        if(name.compare(0,deploymentId.size(),deploymentId)!=0 || name.find(".pending-")==std::string::npos)continue;
        const auto manifest=ParseStrictJson(IO::ReadFile(parent+'/'+name+"/manifest.json",16*1024*1024));
        if(!IO::Boolean(manifest.At("staged_for_replacement")))continue;
        Require(backup.empty(),"Multiple prepared replacement candidates in the original Run parent");
        backup=IO::String(manifest.At("publication_backup"));
    }
    ::closedir(directory);
    Require(!backup.empty(),"Prepared replacement candidate did not identify its exact backup path");
    return backup;
}
void CheckFiles(const JsonValue &status,const std::map<std::string,IceUtil::Handle<RawSender>> &raw,unsigned int frames,bool parentReconciliation=false,const std::function<std::string(const std::string &)> &iceExpected={}) {
    const auto &data=status.At("data"); Require(IO::Boolean(data.At("needs_reconciliation"))==parentReconciliation && data.At("nodes").items.size()==2,"ECC cached data snapshot differs");
    for (const auto &node:data.At("nodes").items) {
        const auto lifecycle=DecodeSupervisorStatus(node.At("lifecycle")); Require(lifecycle.state==DataSupervisorStatus::Stopped && !lifecycle.needsReconciliation,"Network receiver not stopped");
        Require(lifecycle.receivers.size()==1,"Network fixture receiver set differs");
        for (const auto &child:lifecycle.receivers) {
            Require(child.reaped && child.exitCode==0 && child.sealVerified && child.frames==frames,"Network final receiver proof differs");
            const auto seal=ParseStrictJson(IO::ReadFile(child.outputDirectory+(iceExpected ? "/.getdaq-ice-receiver-seal.json" : "/.getdaq-receiver-seal.json"),65536));
            Require(IO::String(seal.At("session_id"))==lifecycle.sessionId && IO::String(seal.At("sender_id"))==child.id && IO::Boolean(seal.At("sender_eof_confirmed"))==(!iceExpected && !child.sourceUnusedConfirmed) && child.sourceEndConfirmed==bool(iceExpected),"Network seal identity/EOF differs");
            if(child.sourceUnusedConfirmed) Require(frames==0 && IO::Unsigned(seal.At("connections"))==0 && IO::Boolean(seal.At("source_unused_confirmed")),"Empty FDT cycle fabricated EOF or data");
            else if(raw.at(child.id)->expectedProtocol=="FDT") Require(IO::Unsigned(seal.At("connections"))==(frames==6 ? 3 : 1),"FDT segment count differs from actual Start/Stop boundaries");
            std::map<unsigned long long,std::string> ordered;
            for(const auto &file:seal.At("files").items) {
                const auto name=IO::String(file.At("name"));const auto underscore=name.rfind('_');
                Require(underscore!=std::string::npos && ordered.emplace(std::stoull(name.substr(underscore+1,name.size()-underscore-6)),name).second,"Network file sequence differs");
            }
            std::string bytes;for(const auto &file:ordered) bytes+=IO::ReadFile(child.outputDirectory+'/'+file.second,1024*1024);
            Require(bytes==(iceExpected ? iceExpected(child.id) : raw.at(child.id)->Expected()),"Network graw bytes differ from independent raw sender");
        }
    }
}
std::string AcceptedOperationId(const Service &service,const std::string &command) {
    std::ifstream journal(service.plan.auditRoot+"/control/operations.jsonl");std::string line,operationId;
    while(std::getline(journal,line)) {
        const auto event=ParseStrictJson(line);
        if(IO::String(event.At("event"))!="accepted")continue;
        const auto request=DecodeSupervisorControlRequest(IO::String(event.At("request")));
        if(SupervisorCommandName(request.command)!=command)continue;
        const std::string duplicateMessage="Fixture accepted more than one original "+command+" request";
        Require(operationId.empty(),duplicateMessage.c_str());operationId=request.operationId;
    }
    const std::string missingMessage="Fixture did not accept the original "+command+" request";
    Require(!operationId.empty(),missingMessage.c_str());return operationId;
}
}
#include "cold_recovery_service_fixture.h"
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN); Ice::CommunicatorPtr communicator;std::string testRoot,blockedCatalogPath;
    try {
        Require((argc==9 || argc==10) && std::string(argv[7])=="--scenario","Pass CoBo/MuTaNT descriptions, receiver, data-service, ecc-server, NEW_ROOT, --scenario NAME [QT_PROBE]");
        const std::string scenario=argv[8],root=argv[6];testRoot=root;const bool maintenanceWindow=scenario=="cobo-maintenance-window-0" || scenario=="cobo-maintenance-window-1"; const bool completionWindow=scenario=="plan-transition-completion-window-0" || scenario=="plan-transition-completion-window-1"; const bool failedNodeWindow=scenario=="plan-transition-failed-node-window-0" || scenario=="plan-transition-failed-node-window-1";const bool planWindow=completionWindow || failedNodeWindow || scenario=="plan-transition-window-1"; const bool partialCompletion=scenario=="plan-transition-complete-0" || scenario=="plan-transition-complete-1";const bool failedNodeCompletion=scenario=="plan-transition-failed-node-0" || scenario=="plan-transition-failed-node-1";const bool failedNodeScenario=failedNodeCompletion || failedNodeWindow; const bool coldSame=scenario=="cold-handoff-service-0" || scenario=="cold-handoff-service-1"; const bool coldCompletion=coldSame || scenario=="plan-transition-cold-complete-0" || scenario=="plan-transition-cold-complete-1"; const bool partialPlans=(coldCompletion && !coldSame) || completionWindow || partialCompletion || failedNodeScenario || scenario=="plan-transition-partial-0"; const bool changedPlans=scenario=="plan-transition-0" || scenario=="plan-transition-1" || partialPlans || planWindow; const bool catalogFailure=scenario=="qt-catalog-failure";const bool catalogRecovery=scenario=="qt-catalog-recovery";const bool hardwareObservation=scenario=="hardware-observation-0" || scenario=="hardware-observation-1";
        const bool descriptionRecovery=scenario.compare(0,21,"description-recovery-")==0;
        const bool descriptionWindow=scenario=="description-recovery-window-0" || scenario=="description-recovery-window-1";
        const bool preparationAlarmWindow=scenario=="preparation-alarm-recovery-window-0" || scenario=="preparation-alarm-recovery-window-1";
        const bool preparationWindow=preparationAlarmWindow || scenario=="preparation-recovery-window-0" || scenario=="preparation-recovery-window-1";
        const bool acquisitionRecovery=scenario.find("acquisition-recovery-network-")==0;
        const bool acquisitionRecoveryAlarm=scenario.find("acquisition-recovery-network-alarm-")==0;
        const bool acquisitionAlarmAck=scenario.find("acquisition-recovery-network-alarm-ack-")==0;
        const bool acquisitionUnprepare=acquisitionRecovery || scenario.find("acquisition-unprepare-network-")==0;
        const bool receiverRetirement=acquisitionUnprepare || scenario.find("receiver-retirement-network-")==0;
        const bool failureBreakup=receiverRetirement || scenario.find("failure-breakup-network-")==0;
        const bool qtDescriptionRecovery=scenario=="description-recovery-qt-0" || scenario=="description-recovery-qt-1";
        const bool descriptionSuccess=descriptionRecovery && scenario.find("success")!=std::string::npos;
        const bool inputFault=scenario.compare(0,12,"input-fault-")==0;
        const bool qtInputFault=scenario=="input-fault-qt-0" || scenario=="input-fault-qt-1";
        const bool preparationCleanup=scenario.find("prepare-cleanup-")==0;
        const bool preparationRecovery=scenario.find("prepare-cleanup-recovery-")==0;
        const bool qtPreparationRecovery=preparationRecovery && scenario.find("-qt-")!=std::string::npos;
        const bool preparationFailure=preparationCleanup || scenario=="prepare-failure-0" || scenario=="prepare-failure-1" || scenario=="prepare-stop-loss";
        const bool preRun=scenario=="pre-run-configured-0" || scenario=="pre-run-configured-1" || scenario=="pre-run-ready-0" || scenario=="pre-run-ready-1";
        const bool preRunConfigured=scenario=="pre-run-configured-0" || scenario=="pre-run-configured-1";
        const bool preparationAlarmAck=scenario=="alarm-preparation-recovery-0" || scenario=="alarm-preparation-recovery-1";
        const bool alarmUnknownPrepare=preparationAlarmAck || scenario=="alarm-unknown-prepare-0" || scenario=="alarm-unknown-prepare-1";
        const bool alarmUnknownStart=scenario=="alarm-unknown-start";
        const bool zbuf=scenario=="zbuf-0" || scenario=="zbuf-1";
        const bool fdt=scenario=="fdt-0" || scenario=="fdt-1" || scenario=="fdt-empty-0";
        const bool observeSources=scenario=="ice-observe-0" || scenario=="ice-observe-1";
        const bool qtSourceRecovery=scenario=="qt-source-recovery-0" || scenario=="qt-source-recovery-1";
        const bool iceSourceRecovery=scenario=="ice-source-recovery-0" || scenario=="ice-source-recovery-1";
        const bool iceReceiverFailure=scenario.compare(0,13,"ice-receiver-")==0;
        const bool receiverStopLoss=scenario=="receiver-stop-loss" || scenario=="receiver-cleanup-stop-loss" || scenario=="ice-receiver-stop-loss";
        const bool receiverCleanup=scenario=="receiver-cleanup-0" || scenario=="receiver-cleanup-1" || scenario=="receiver-cleanup-stop-loss" || scenario=="receiver-cleanup-audit";
        const bool fdtEmpty=scenario=="fdt-empty-0",ice=qtSourceRecovery || iceSourceRecovery || iceReceiverFailure || scenario=="ice-0" || scenario=="ice-1" || scenario=="ice-empty-0" || observeSources,iceEmpty=scenario=="ice-empty-0";
        const bool alarmUnknown=alarmUnknownPrepare || alarmUnknownStart;
        const bool receiverCase=!receiverRetirement && (iceReceiverFailure || scenario.compare(0,9,"receiver-")==0);
        const bool receiverStale=scenario=="receiver-stale-stop-0" || scenario=="receiver-stale-stop-1";
        const bool qtAlarmAcquisitionRecovery=scenario=="qt-alarm-acquisition-recovery-0" || scenario=="qt-alarm-acquisition-recovery-1";
        const bool qtAcquisitionRecovery=qtSourceRecovery || qtAlarmAcquisitionRecovery || scenario=="qt-acquisition-recovery-0" || scenario=="qt-acquisition-recovery-1";
        const bool qtAcquisitionUnprepare=qtAcquisitionRecovery || scenario=="qt-acquisition-unprepare-0" || scenario=="qt-acquisition-unprepare-1";
        const bool qtReceiverRetirement=qtAcquisitionUnprepare || scenario=="qt-acquisition-unprepare-0" || scenario=="qt-receiver-retirement-0" || scenario=="qt-receiver-retirement-1";
        const bool qtFailureBreakup=qtReceiverRetirement || scenario=="qt-acquisition-unprepare-0" || scenario=="qt-receiver-retirement-0" || scenario=="qt-failure-breakup-0" || scenario=="qt-failure-breakup-1";
        const bool qtWindowCleanup=qtFailureBreakup || scenario=="qt-window-cleanup-0" || scenario=="qt-window-cleanup-1";
        const bool qtReceiverFailure=qtWindowCleanup || scenario=="qt-receiver-failure-0" || scenario=="qt-receiver-failure-1";
        const bool receiverEnabled=!qtSourceRecovery && (failureBreakup || qtReceiverFailure || (receiverCase && scenario!="receiver-disabled"));
        const bool qtStoppedAlarmRelease=scenario=="qt-stopped-alarm-release-0" || scenario=="qt-stopped-alarm-release-1";
        const bool qtAlarm=qtStoppedAlarmRelease || scenario=="qt-alarm-0" || scenario=="qt-alarm-1";
        const bool alarmRelease=scenario.find("alarm-release-")==0;
        const bool alarmReleaseReady=scenario.find("alarm-release-ready-")==0;
        const bool alarmReleasePowerLoss=scenario.find("alarm-release-power-loss-")==0;
        const bool alarmReady=alarmReleaseReady || scenario=="alarm-ready";
        const bool alarmCase=alarmRelease || scenario=="alarm-running-0" || scenario=="alarm-running-1" || scenario=="alarm-paused" || scenario=="alarm-stop-loss" ||
            scenario=="alarm-ready" || scenario=="alarm-disabled" || scenario=="alarm-restart" || scenario=="alarm-during-stop";
        const bool alarmEnabled=(preparationAlarmWindow || qtAlarmAcquisitionRecovery || acquisitionRecoveryAlarm || alarmCase || alarmUnknown || qtAlarm) && scenario!="alarm-disabled";
        const bool catalog=scenario=="qt-catalog-0" || scenario=="qt-catalog-1" || catalogFailure || catalogRecovery;
        const bool qt=qtReceiverFailure || scenario=="qt-gui-0" || scenario=="qt-gui-1" || catalog || qtAlarm;
        Require(std::set<std::string>{"plan-transition-completion-window-0","plan-transition-completion-window-1","plan-transition-failed-node-window-0","plan-transition-failed-node-window-1","plan-transition-complete-0","plan-transition-complete-1","plan-transition-failed-node-0","plan-transition-failed-node-1","cobo-maintenance-window-0","cobo-maintenance-window-1","qt-source-recovery-0","qt-source-recovery-1","ice-source-recovery-0","ice-source-recovery-1","qt-stopped-alarm-release-0","qt-stopped-alarm-release-1","alarm-release-running-0","alarm-release-running-1","alarm-release-ready-0","alarm-release-ready-1","alarm-release-power-loss-0","alarm-release-power-loss-1","preparation-alarm-recovery-window-0","preparation-alarm-recovery-window-1","alarm-preparation-recovery-0","alarm-preparation-recovery-1","qt-alarm-acquisition-recovery-0","qt-alarm-acquisition-recovery-1","acquisition-recovery-network-alarm-ack-0","acquisition-recovery-network-alarm-ack-1","acquisition-recovery-network-alarm-0","acquisition-recovery-network-alarm-1","qt-acquisition-recovery-0","qt-acquisition-recovery-1","acquisition-recovery-network-0","acquisition-recovery-network-1","acquisition-unprepare-network-0","acquisition-unprepare-network-1","acquisition-unprepare-network-power-loss","qt-acquisition-unprepare-0","qt-acquisition-unprepare-1","receiver-retirement-network-0","receiver-retirement-network-1","qt-receiver-retirement-0","qt-receiver-retirement-1","qt-failure-breakup-0","qt-failure-breakup-1","failure-breakup-network-0","failure-breakup-network-1","failure-breakup-network-stop-loss","failure-breakup-network-disconnect-loss","failure-breakup-network-unknown","preparation-recovery-window-0","preparation-recovery-window-1","prepare-cleanup-recovery-0","prepare-cleanup-recovery-1","prepare-cleanup-recovery-power","prepare-cleanup-recovery-audit","prepare-cleanup-recovery-qt-0","prepare-cleanup-recovery-qt-1","prepare-cleanup-0","prepare-cleanup-1","prepare-cleanup-power-loss","description-recovery-window-0","description-recovery-window-1","description-recovery-success-0","description-recovery-success-1","description-recovery-denial-0","description-recovery-denial-1","description-recovery-audit-0","description-recovery-audit-1","description-recovery-qt-0","description-recovery-qt-1","input-fault-qt-0","input-fault-qt-1","input-fault-running-0","input-fault-running-1","input-fault-prepared-0","input-fault-prepared-1","input-fault-audit-0","input-fault-audit-1","hardware-observation-0","hardware-observation-1","broken-cleanup-0","broken-cleanup-1","broken-cleanup-readback","broken-cleanup-readback-again","qt-window-cleanup-0","qt-window-cleanup-1","receiver-cleanup-0","receiver-cleanup-1","receiver-cleanup-stop-loss","receiver-cleanup-audit","receiver-stale-stop-0","receiver-stale-stop-1","ice-receiver-running-0","ice-receiver-running-1","ice-receiver-stop-loss","ice-receiver-restart","qt-receiver-failure-0","qt-receiver-failure-1","receiver-running-0","receiver-running-1","receiver-paused","receiver-ready","receiver-stop-loss","receiver-disabled","receiver-normal-exit","receiver-offline","receiver-restart","handoff-service-0","handoff-service-1","cold-handoff-service-0","cold-handoff-service-1","plan-transition-0","plan-transition-1","plan-transition-partial-0","plan-transition-cold-complete-0","plan-transition-cold-complete-1","plan-transition-window-1","normal-0","normal-1","zbuf-0","zbuf-1","fdt-0","fdt-1","fdt-empty-0","ice-0","ice-1","ice-empty-0","ice-observe-0","ice-observe-1","start-ack-loss","truncated","ecc-restart","ecc-interrupted","request-fault","qt-gui-0","qt-gui-1","qt-catalog-0","qt-catalog-1","qt-catalog-failure","qt-catalog-recovery","prepare-failure-0","prepare-failure-1","prepare-stop-loss","pre-run-configured-0","pre-run-configured-1","pre-run-ready-0","pre-run-ready-1","alarm-running-0","alarm-running-1","alarm-paused","alarm-stop-loss","alarm-ready","alarm-disabled","alarm-restart","alarm-during-stop","alarm-unknown-prepare-0","alarm-unknown-prepare-1","alarm-unknown-start","qt-alarm-0","qt-alarm-1"}.count(scenario),"Unknown ECC network scenario");
        Require(((maintenanceWindow || planWindow || qt || qtInputFault || qtDescriptionRecovery || descriptionWindow || preparationWindow || qtPreparationRecovery) ? argc==10 : argc==9),"ECC network scenario has the wrong Qt probe argument count");
        IO::CreateDirectory(root); IO::CreateDirectory(root+"/outputs");
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4"); communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("EccNetworkFixture","tcp -h 127.0.0.1 -p 0");
        Rig rig(communicator,adapter,LoadHardwareDescription(argv[1]),LoadHardwareDescription(argv[2]),((maintenanceWindow && scenario.back()=='0') || (alarmRelease && scenario.back()=='0') || ((inputFault || descriptionRecovery || preparationCleanup || preparationWindow || failureBreakup) && scenario.back()=='0')) || scenario=="hardware-observation-0" || scenario=="broken-cleanup-0" || scenario=="qt-window-cleanup-0" || scenario=="qt-alarm-acquisition-recovery-0" || scenario=="qt-acquisition-recovery-0" || scenario=="qt-acquisition-unprepare-0" || scenario=="qt-receiver-retirement-0" || scenario=="qt-failure-breakup-0" || scenario=="receiver-stale-stop-0" || scenario=="ice-receiver-running-0" || scenario=="qt-receiver-failure-0" || scenario=="receiver-running-0" || scenario=="receiver-cleanup-0" || ((partialPlans || coldSame) && scenario.back()=='0') || scenario=="plan-transition-0" || scenario=="handoff-service-0" || scenario=="normal-0" || scenario=="zbuf-0" || scenario=="fdt-0" || fdtEmpty || scenario=="ice-0" || iceEmpty || scenario=="ice-observe-0" || scenario=="qt-gui-0" || scenario=="qt-catalog-0" || scenario=="prepare-failure-0" || scenario=="pre-run-configured-0" || scenario=="pre-run-ready-0" || scenario=="alarm-running-0" || scenario=="alarm-unknown-prepare-0" || scenario=="alarm-preparation-recovery-0" || scenario=="qt-alarm-0" || scenario=="qt-stopped-alarm-release-0" || scenario=="ice-source-recovery-0" || scenario=="qt-source-recovery-0" ? 0 : 1); rig.options.mutants.clear();
        if(maintenanceWindow)rig.w.ledDelayMs=600;
        rig.senders.erase(std::remove_if(rig.senders.begin(),rig.senders.end(),[](const DataSenderOptions &s) { return s.senderId[0]!='C'; }),rig.senders.end());
        std::sort(rig.senders.begin(),rig.senders.end(),[](const DataSenderOptions &a,const DataSenderOptions &b) { return a.senderId<b.senderId; });
        std::map<std::string,IceUtil::Handle<RawSender>> raw;std::vector<std::function<void()>> iceCleanup;std::vector<DataSupervisorOptions> plans;
#ifdef GETDAQ_TEST_ICE_SOURCE
        std::map<std::string,std::unique_ptr<IceProducer>> iceProducers;std::map<std::string,IceUtil::Handle<ObservationBarrierSource>> iceSources;
#else
        Require(!ice,"ICE source scenarios require optional source build");
#endif
        for (auto &sender:rig.senders) {
            raw[sender.senderId]=new RawSender(rig.w,sender.senderId); sender.controlProxy=communicator->proxyToString(adapter->addWithUUID(raw.at(sender.senderId)));
            if(zbuf || fdt) {sender.protocol=fdt ? "FDT" : "ZBUF";raw.at(sender.senderId)->expectedProtocol=sender.protocol;}
            DataSupervisorOptions plan; plan.receiverExecutable=argv[3]; plan.deploymentId="ecc-network-fixture"; plan.startupTimeoutMs=plan.finishTimeoutMs=3000; plan.statusTimeoutMs=1000;
            ReceiverSourcePlan source; source.id=sender.senderId; source.outputRoot=root+"/outputs"; source.bindAddress="127.0.0.1"; source.dataPort=Port(); source.statusPort=Port();
            if(zbuf || fdt) source.inputProtocol=sender.protocol;
            if(ice) {sender.protocol=source.inputProtocol="ICE";plan.iceReceiverExecutable=argv[3];plan.iceReceiverSha256=ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024));}
            source.receiverAddress=sender.receiverAddress="127.0.0.1:"+std::to_string(source.dataPort); source.queueMiB=source.maxFrameMiB=1;
            #ifdef GETDAQ_TEST_ICE_SOURCE
            if(ice) {
                iceProducers[sender.senderId].reset(new IceProducer(rig.w,sender.senderId));SourceControlOptions sourceOptions;sourceOptions.sourceId=sender.senderId;
                sourceOptions.receiver.port=source.dataPort;sourceOptions.receiver.encodingMinor=unsigned(rig.options.encodingMinor);sourceOptions.receiver.timeoutMs=1000;sourceOptions.receiver.chunkBytes=317;
                iceSources[sender.senderId]=new ObservationBarrierSource(*iceProducers.at(sender.senderId),sourceOptions);sender.controlProxy=communicator->proxyToString(adapter->addWithUUID(iceSources.at(sender.senderId)));
                const auto id=sender.senderId;iceCleanup.push_back([&,id] {iceProducers.at(id)->Stop();});
            }
            #endif
            plan.sources.push_back(source); plans.push_back(plan);
        }
        std::function<std::string(const std::string &)> iceExpected;
#ifdef GETDAQ_TEST_ICE_SOURCE
        if(ice) iceExpected=[&](const std::string &id) {return iceProducers.at(id)->Expected();};
#endif
        auto native=Options(rig,root); const auto artifact=DecodeSystemExperimentArtifact(native.experimentArtifactBytes);
        RemoteReceiverOptions remote; remote.controllerId="native-remote-controller"; remote.commandTimeoutMs=15000; remote.pollIntervalMs=20;
        std::vector<std::unique_ptr<Service>> services;
        std::unique_ptr<DropReplyRelay> dataRelay,coldDataRelay; std::atomic<bool> alarmInjected{false},catalogRecoveryFaultInjected{false};
        std::unique_ptr<ReceiverObservationBarrier> receiverBarrier;
        for (std::size_t i=0;i<plans.size();++i) {
            const auto id="node-"+std::to_string(i),directory=root+'/'+id; plans[i].auditRoot=directory+"/deployment"; plans[i].configurationSha256=artifact.configurationSha256; plans[i].artifactSha256=artifact.artifactSha256;
            if (catalog) { plans[i].catalogRoot=root+"/catalog-"+id; IO::CreateDirectory(plans[i].catalogRoot); }
            if (catalogFailure && i==1) {
                IO::CreateDirectory(plans[i].catalogRoot+"/NativeIntegration"); IO::CreateDirectory(plans[i].catalogRoot+"/NativeIntegration/run_0000");
                IO::CreateDirectory(plans[i].catalogRoot+"/NativeIntegration/run_0000/"+plans[i].deploymentId);
                IO::WriteNew(plans[i].catalogRoot+"/NativeIntegration/run_0000/"+plans[i].deploymentId+"/existing-marker.txt","Preserve this existing dataset\n");
            }
            std::map<std::string,std::string> childEnvironment;
            if(failedNodeScenario && i==1) {
                const char *injector=::getenv("GETDAQ_TEST_RETIREMENT_INJECTOR");Require(injector && *injector,"Failed-node ECC scenario requires the private retirement injector");
                const char *asan=::getenv("GETDAQ_TEST_ASAN_RUNTIME");std::string preload=asan && *asan ? std::string(asan)+":" : std::string();preload+=injector;
                childEnvironment["LD_PRELOAD"]=preload;
                childEnvironment["GETDAQ_TEST_RETIREMENT_PLAN"]=root+"/successor-inputs/reviewed-plan-1.json";
                childEnvironment["GETDAQ_TEST_RETIREMENT_BACKUP"]=root+"/successor-inputs/reviewed-plan-1.json.backup";
                childEnvironment["GETDAQ_TEST_RETIREMENT_MARKER"]=root+"/retirement-fault-marker";
                if(failedNodeWindow) {
                    childEnvironment["GETDAQ_TEST_RETIREMENT_ARM"]=root+"/retirement-fault-armed";
                    childEnvironment["GETDAQ_TEST_RETIREMENT_MARKER_WAIT_MS"]="3000";
                }
            }
            services.emplace_back(new Service(argv[4],directory,id,Port(),plans[i],childEnvironment)); RemoteReceiverEndpoint e; e.agentId=id; e.control=services.back()->direct;
            if(catalogRecovery && i==1) {
                const auto relayPort=Port();dataRelay.reset(new DropReplyRelay(relayPort,e.control.port,"",[&](const std::string &request,const std::string &) {
                    if(request.compare(0,5,"POST ")==0 && !catalogRecoveryFaultInjected.load()) {
                        const auto separator=request.find("\r\n\r\n");Require(separator!=std::string::npos,"Malformed catalog recovery fixture request");
                        const auto body=ParseStrictJson(request.substr(separator+4));
                        if(IO::String(body.At("command"))=="catalog" && body.members.count("catalog_phase") && IO::String(body.At("catalog_phase"))=="prepare" && !catalogRecoveryFaultInjected.exchange(true)) {
                            blockedCatalogPath=ReplacementBackupPath(plans[0].catalogRoot,"NativeIntegration",0,plans[0].deploymentId);
                            IO::WriteNew(blockedCatalogPath,"Synthetic occupied replacement backup; remove before explicit resume.\n");
                        }
                    }
                }));e.control.port=relayPort;
            }
            if(planWindow && i==0) {
                const auto relayPort=Port();dataRelay.reset(new DropReplyRelay(relayPort,e.control.port,"",[&](const std::string &request,const std::string &) {
                    if(request.compare(0,5,"POST ")==0 && IO::String(ParseStrictJson(request.substr(request.find("\r\n\r\n")+4)).At("command"))=="retire-plan" && !alarmInjected.exchange(true)) {
                        IO::WriteNew(root+"/gui-retirement-held","Original node completed; parent reply held for GUI lease check\n");
                        const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(60);struct stat st;
                        while(::lstat((root+"/gui-retirement-release").c_str(),&st)!=0) {
                            Require(std::chrono::steady_clock::now()<deadline,"GUI did not release retirement test barrier");std::this_thread::sleep_for(std::chrono::milliseconds(5));
                        }
                    }
                }));e.control.port=relayPort;
            }
            if(receiverStale && i==0) {const auto relayPort=Port();receiverBarrier.reset(new ReceiverObservationBarrier(relayPort,e.control.port));e.control.port=relayPort;}
            if (preRunConfigured && i==0) {
                const auto relayPort=Port();
                dataRelay.reset(new DropReplyRelay(relayPort,e.control.port,"",[&](const std::string &request,const std::string &) {
                    if (request.compare(0,5,"POST ")==0 && IO::String(ParseStrictJson(request.substr(request.find("\r\n\r\n")+4)).At("command"))=="verify-ready" && !alarmInjected.exchange(true))
                        rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                }));
                e.control.port=relayPort;
            }
            if(coldSame && i==0) {const auto relayPort=Port();coldDataRelay.reset(new DropReplyRelay(relayPort,e.control.port,"handoff-control"));e.control.port=relayPort;}
            e.planSha256=GetDAQ::ConfigurationSha256(IO::EncodeOptions(plans[i],GetDAQ::ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)))); remote.endpoints.push_back(e);
        }
        const auto credential=root+"/ecc-credential"; IO::WriteNew(credential,IO::RandomSession()+"\n"); const auto port=Port(); auto config=WriteConfiguration(root,argv[1],native,remote,port,credential,alarmEnabled,receiverEnabled);
        const bool reviewedConfiguration=scenario=="qt-gui-0" || scenario=="qt-gui-1";
        if (reviewedConfiguration) config=ReviewedConfiguration(root,config,native.experimentArtifactBytes);
        adapter->activate(); SupervisorHttpOptions endpoint; endpoint.address="127.0.0.1"; endpoint.port=port; endpoint.credentialFile=credential; endpoint.timeoutMs=1500;
        std::unique_ptr<EccProcess> process(new EccProcess(argv[5],config,root,endpoint)); std::unique_ptr<DropReplyRelay> relay;
        if (scenario=="start-ack-loss") { const auto relayPort=Port(); relay.reset(new DropReplyRelay(relayPort,port,"start")); endpoint.port=relayPort; }
        NativeControlHttpClient client(endpoint),direct(process->direct); const auto identity=Read(client.Identity());
        if(scenario=="handoff-service-0" || scenario=="handoff-service-1") {
            const auto premature=direct.ShutdownEccService(IO::String(identity.At("instance_id")),IO::String(identity.At("snapshot_sha256")),"premature-shutdown");
            Require(premature.code==409 && direct.Identity().code==200,"ECC shutdown was not refused before a completed handoff");
        }
        IO::WriteNew(root+"/identity.json",direct.Identity().body); std::ofstream index(root+"/acquisition-index.jsonl"); Require(bool(index),"Network fixture index");
        const auto freshObservation=direct.Receivers(); const auto freshProcesses=DecodeEccControlReceiverObservation(freshObservation.body);
        Require(freshObservation.code==200 && freshProcesses.nodes.size()==2,"Fresh ECC receiver observation unavailable");
        for (const auto &node:freshProcesses.nodes) Require(node.available && !node.busy && node.receivers.empty(),"Fresh ECC observation invented receivers");
        IO::WriteNew(root+"/fresh-receiver-observation.json",freshObservation.body);
        if (scenario=="handoff-service-0" || scenario=="handoff-service-1" || changedPlans || coldSame) {
            auto premature=Request(identity,"handoff-data-control","premature-handoff");
            premature.successorScopeSha256=std::string(64,'d');premature.successorEpoch=std::string(64,'e');
            premature.successorPlanSha256=std::string(64,'f');premature.successorDataBindingSha256=identity.At("data_binding_sha256").text;
            const auto before=direct.Status().body;
            Require(direct.PostOperation(EncodeEccControlRequest(premature)).code==412,"Fresh ECC accepted unsealed handoff");
            Require(direct.Status().body==before,"Rejected handoff changed original state/owner");
        }
        const auto submit=[&](const std::string &command,const std::string &id) { const auto response=client.PostOperation(EncodeEccControlRequest(Request(identity,command,id))); Require(response.code==202,"ECC network command not accepted"); return response; };
        const auto execute=[&](const std::string &command,const std::string &id,EccState state) {
            submit(command,id); const auto op=Terminal(client,id); IO::WriteNew(root+"/operation-"+id+".json",client.Operation(id).body);
            Require(IO::String(op.At("phase"))=="complete",("ECC network workflow failed: "+IO::String(op.At("message"))).c_str()); State(client,state);
            if (command=="configure") {
                const auto response=direct.ConfigureReadbacks();Require(response.code==200,"Configure readback cache unavailable");
                const auto evidence=ParseStrictJson(response.body);const auto &captured=evidence.At("readbacks");
                Require(evidence.At("instance_id").text==identity.At("instance_id").text && evidence.At("snapshot_sha256").text==identity.At("snapshot_sha256").text &&
                    evidence.At("configuration_sha256").text==identity.At("configuration_sha256").text && evidence.At("artifact_sha256").text==identity.At("artifact_sha256").text,
                    "Configure evidence identity/parameter binding differs");
                Require(captured.At("phase").text=="captured" && captured.At("configure_operation_id").text==id &&
                    captured.At("historical").text=="true" && captured.At("actual_loaded_configuration_verified").text=="false" &&
                    !captured.At("entries").items.empty(),"Configure cache hides scope or has no hardware checks");
                IO::WriteNew(root+"/configure-readbacks-"+id+".json",response.body);
            }
        };
        try {
#ifdef GETDAQ_TEST_ICE_SOURCE
            if(iceSourceRecovery) {
                execute("describe","describe",EccState::Described);execute("prepare","prepare",EccState::Prepared);execute("configure","configure",EccState::Ready);execute("start","start",EccState::Running);
                Wait([&]{for(const auto &entry:iceProducers)if(entry.second->read[0]!=entry.second->write[0] || entry.second->read[1]!=entry.second->write[1])return false;return true;},5000);
                IO::WriteNew(root+"/source-recovery-before-fault.json",direct.Status().body);
                iceSources.begin()->second->ReportFailure("Synthetic asynchronous ICE source device fault");
                Require(!DecodeEccControlSystemStatus(direct.Status().body).controller.outcomeUnknown,"Readonly source failure changed parent before explicit stop");
                submit("stop-production-after-failure","original-source-stop");Require(IO::String(Terminal(client,"original-source-stop").At("phase"))=="complete","Original source stop failed");
                Require(DecodeEccControlSystemStatus(direct.Status().body).controller.outcomeUnknown,"Source stop cleared UNKNOWN");
                submit("retire-failed-sources","original-source-retirement");Require(IO::String(Terminal(client,"original-source-retirement").At("phase"))=="complete","Original failed-source retirement failed");
                IO::WriteNew(root+"/source-recovery-retired-sources.json",direct.Sources().body);
                submit("breakup-after-failure","original-source-breakup");Require(IO::String(Terminal(client,"original-source-breakup").At("phase"))=="complete","Retired source connection cleanup failed");
                submit("abort-failed-receivers","original-source-receiver-abort");Require(IO::String(Terminal(client,"original-source-receiver-abort").At("phase"))=="complete","Original source receiver Abort failed");
                submit("retire-failed-receivers","original-source-receiver-retirement");Require(IO::String(Terminal(client,"original-source-receiver-retirement").At("phase"))=="complete","Original source receiver retirement failed");
                submit("unprepare-acquisition-after-failure","original-source-unprepare");Require(IO::String(Terminal(client,"original-source-unprepare").At("phase"))=="complete","Original source power-off cleanup failed");
                const auto retired=DecodeEccControlSystemStatus(direct.Status().body);IO::WriteNew(root+"/source-recovery-retired-data.json",direct.Status().body);
                Require(retired.controller.outcomeUnknown && retired.controlNeedsReconciliation && !retired.dataNeedsReconciliation,"Original source cleanup cleared parent early");
                for(const auto &node:retired.nodes)Require(node.state=="retired" && node.receivers[0].reaped && !node.receivers[0].sealVerified && !node.receivers[0].sourceEndConfirmed,"Source cleanup fabricated data seal/End");
                auto request=Request(identity,"reconcile-acquisition","ordinary-source-recovery");request.failedOperationId="start";
                Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Ordinary source recovery intent missing");
                Require(IO::String(Terminal(client,request.operationId).At("phase"))=="failed","Ordinary acquisition recovery cleared source fault");
                request.command="reconcile-acquisition-with-sources";request.operationId="verified-source-recovery";
                rig.Clear();const auto writes=rig.w.Writes();Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Explicit source recovery not accepted");
                Require(IO::String(Terminal(client,request.operationId).At("phase"))=="complete","Explicit source recovery failed");
                Require(rig.w.Writes()==writes,"Source recovery wrote hardware");
                IO::WriteNew(root+"/operation-verified-source-recovery.json",client.Operation(request.operationId).body);
                const auto recovered=DecodeEccControlSystemStatus(direct.Status().body);
                Require(recovered.controller.state==2 && !recovered.controller.outcomeUnknown && !recovered.controlNeedsReconciliation && !recovered.dataNeedsReconciliation,"Source recovery did not establish known Described");
                execute("prepare","fresh-prepare",EccState::Prepared);execute("configure","fresh-configure",EccState::Ready);
                Require(DecodeEccControlSystemStatus(direct.Status().body).nodes[0].sessionId!=retired.nodes[0].sessionId,"Source recovery reused original failed data session");
                execute("start","fresh-start",EccState::Running);execute("stop","fresh-stop",EccState::Ready);execute("breakup","fresh-breakup",EccState::Prepared);
                CheckFiles(Read(direct.Status()),raw,2,false,iceExpected);
                for(const auto &service:services)Require(service->Commands("start")==2 && service->Commands("finish")==1 && service->Commands("abort-failed-session")==1 && service->Commands("retire-failed-session")==1,"Source recovery replayed data lifecycle");
                execute("reset","fresh-unprepare",EccState::Described);execute("reset","fresh-forget",EccState::Idle);
                rig.Clear();process->CrashAndRestart();const auto cold=Read(direct.Identity());request=Request(cold,"reconcile-acquisition-with-sources","cold-source-recovery");request.failedOperationId="start";
                Require(direct.PostOperation(EncodeEccControlRequest(request)).code==202,"Cold source recovery intent missing");
                Require(IO::String(Terminal(direct,request.operationId).At("phase"))=="failed" && rig.Actions().empty(),"Cold service adopted old source recovery objects");
                IO::WriteNew(root+"/cold-source-recovery-denied.json",direct.Operation(request.operationId).body);
                IO::WriteNew(root+"/ice-source-recovery-network-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                    ",\"old_source_unknown_retained\":true,\"old_data_unsealed\":true,\"recovery_hardware_writes\":0,\"fresh_cycle_bytes_and_seals_verified\":true,\"cold_replay_held\":true}\n");
            } else
#endif
            if(descriptionWindow || preparationWindow) {
                if(descriptionWindow)rig.devices[0].at("ctrl")->failStatusClear=true;
                bool preparationAlarmSent=false;
                if(preparationWindow)rig.w.c[1].onAction=[&](const std::string &action){rig.w.Record("C1."+action);if(!preparationAlarmSent && rig.w.alarms[0]->subscribes==1){preparationAlarmSent=true;rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());}};
                std::string nodes="[";
                for (std::size_t i=0;i<plans.size();++i) {
                    if (nodes.back()!='[') nodes+=',';
                    nodes+="{\"id\":"+std::to_string(i)+",\"role\":\"cobo\",\"host\":\"127.0.0.1\",\"native_sender_id\":"+EncodeJsonString(plans[i].sources[0].id)+",\"native_agent_id\":"+EncodeJsonString(remote.endpoints[i].agentId)+"}";
                }
                nodes+=']';
                const auto gui="{\"experiment\":\"NativeIntegration\",\"run_number\":0,\"getdaq\":{\"simulation\":false,\"native_configuration_status\":true,\"native_alarm_status\":"+((qtAlarm || qtAlarmAcquisitionRecovery || preparationAlarmWindow) ? std::string("true") : std::string("false"))+
                    ",\"native_receiver_failure_status\":"+(qtReceiverFailure && !qtSourceRecovery ? std::string("true") : std::string("false"))+",\"native_source_status\":"+(qtSourceRecovery ? std::string("true") : std::string("false"))+",\"native_catalog\":"+(catalog ? std::string("true") : std::string("false"))+",\"config_dir\":"+EncodeJsonString(root+"/unused-legacy-config")+",\"output_dir\":"+EncodeJsonString(root+"/gui-runs")+
                    ",\"poll_ms\":60,\"timeout_ms\":2000,\"step_timeout_ms\":240000,\"native_ecc\":{\"address\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"client_epoch\":"+EncodeJsonString(std::string(64,'a'))+
                    ",\"snapshot_sha256\":"+EncodeJsonString(IO::String(identity.At("snapshot_sha256")))+",\"input_binding_sha256\":"+EncodeJsonString(IO::String(identity.At("input_binding_sha256")))+",\"timeout_ms\":2000,\"status_max_age_ms\":10000,\"record_dir\":"+EncodeJsonString(root+"/gui-client-records")+"},\"nodes\":"+nodes+"}}\n";
                const auto file=root+"/gui.json";IO::WriteNew(file,gui);
                std::vector<std::string> words={argv[9],file};std::vector<char *> arguments;for(auto &word:words)arguments.push_back(&word[0]);arguments.push_back(nullptr);pid_t probe=0;
                Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),environ),"Start real description recovery window");
                bool released=preparationWindow;int status=0;const auto started=std::chrono::steady_clock::now();
                while(::waitpid(probe,&status,WNOHANG)==0) {
                    if(!released && ::access((root+"/description-reconciled-ready").c_str(),F_OK)==0) {
                        rig.devices[0].at("ctrl")->failStatusClear=false;IO::WriteNew(root+"/continue-prepare","ready");released=true;
                    }
                    if(std::chrono::steady_clock::now()-started>std::chrono::seconds(230)){::kill(probe,SIGTERM);::waitpid(probe,&status,0);throw std::runtime_error("Window recovery deadline");}
                    std::this_thread::sleep_for(std::chrono::milliseconds(20));
                }
                Require(WIFEXITED(status) && WEXITSTATUS(status)==0 && released,"Real MainWindow recovery/Prepare failed");
                State(client,EccState::Prepared);
                if(preparationWindow) {
                    const auto proof=ParseStrictJson(IO::ReadFile(root+"/window-preparation-proof.json",65536));
                    Require(IO::Boolean(proof.At("passed")) && IO::Boolean(proof.At("original_failure_recorded_by_gui")) &&
                        IO::Unsigned(proof.At("confirmed_cleanup_signals"))==1 && IO::Unsigned(proof.At("confirmed_recovery_signals"))==1 &&
                        IO::Unsigned(proof.At("cancel_cleanup_signals"))==0 && IO::Unsigned(proof.At("cancel_recovery_signals"))==0,"Preparation window proof differs");
                    std::map<std::string,unsigned int> accepted;std::map<std::string,EccControlOperation> observed;
                    std::ifstream events(root+"/controller/network/operations.jsonl");std::string row;
                    while(std::getline(events,row)){const auto event=ParseStrictJson(row);if(IO::String(event.At("event"))=="accepted") {
                        const auto request=DecodeEccControlRequest(IO::String(event.At("request")));++accepted[request.command];
                        observed.emplace(request.operationId,DecodeEccControlOperation(client.Operation(request.operationId).body));
                    }}
                    auto expected=std::map<std::string,unsigned int>{{"describe",1},{"prepare",2},{"stop-production-after-failure",1},{"unprepare-after-failure",1},{preparationAlarmWindow ? "reconcile-preparation-with-alarm" : "reconcile-preparation",1}};
                    Require(accepted==expected,"Window canceled or duplicated accepted requests");
                    const auto original=IO::String(proof.At("original_failed_operation_id"));
                    Require(preparationAlarmSent && observed.at(original).request.command=="prepare" && observed.at(original).phase=="failed" && observed.at(original).message.find("alarm interlock")!=std::string::npos,"Window original Prepare failure not bound to injected alarm");
                    for(const auto &entry:observed) {
                        const auto &op=entry.second;
                        if(op.request.command=="stop-production-after-failure" || op.request.command=="unprepare-after-failure") {
                            const auto held=DecodeEccControlControllerStatus(op.resultJson);
                            Require(op.phase=="complete" && held.outcomeUnknown && held.operationId==original && held.state==2,"Window negative completion cleared/rebound parent");
                        }
                        if(op.request.command=="reconcile-preparation" || op.request.command=="reconcile-preparation-with-alarm") {
                            Require(op.phase=="complete" && op.request.failedOperationId==original,"Window recovery did not bind GUI original Prepare");
                            if(preparationAlarmWindow){const auto stop=observed.find(op.request.alarmOperationId);Require(stop!=observed.end() && stop->second.request.command=="stop-production-after-failure","Window acknowledgement did not bind original automatic stop");}
                        }
                    }
                    for(const auto &service:services){const auto life=DecodeSupervisorStatus(service->Status().At("lifecycle"));Require(life.state==DataSupervisorStatus::Fresh && life.receivers.empty() && !service->Commands("start"),"Preparation window started receiver");}
                    IO::WriteNew(root+"/preparation-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"no_receivers_started\":true,\"accepted_cleanup_requests\":1,\"accepted_recovery_requests\":1}");
                } else {
                const auto proof=ParseStrictJson(IO::ReadFile(root+"/window-description-proof.json",65536));Require(IO::Boolean(proof.At("passed")) && IO::Unsigned(proof.At("confirmed_requests"))==1 && IO::Unsigned(proof.At("cancel_requests"))==0,"Window recovery proof differs");
                }

                if(descriptionWindow)IO::WriteNew(root+"/description-recovery-result.json","{\"passed\":true,\"real_main_window_and_runcontrol\":true,\"real_hardware\":false}");
            } else if(descriptionRecovery) {

                rig.devices[0].at("ctrl")->failStatusClear=true;
                submit("describe","failed-describe");const auto failed=Terminal(client,"failed-describe");
                Require(IO::String(failed.At("phase"))=="failed","Describe fault did not finish failed");
                rig.devices[0].at("ctrl")->failStatusClear=false;
                const auto before=rig.w.Writes();const auto held=Read(client.Status());
                Require(IO::Boolean(held.At("control_needs_reconciliation")) && IO::Boolean(held.At("controller").At("outcome_unknown")),"Failed Describe gate absent");
                const auto recover=[&](const std::string &id,const std::string &original) {
                    auto request=Request(identity,"reconcile-description",id);request.failedOperationId=original;
                    const auto response=client.PostOperation(EncodeEccControlRequest(request));Require(response.code==202,"Recovery request not recorded");return Terminal(client,id);
                };
                if(scenario.find("audit")!=std::string::npos) {
                    std::ofstream changed(native.auditRoot+"/operations/failed-describe/operation.jsonl",std::ios::app);changed<<" ";changed.close();
                    const auto denied=recover("corrupt-audit-recovery","failed-describe");Require(IO::String(denied.At("phase"))=="failed","Corrupt original audit recovered");
                    const auto status=Read(client.Status());Require(IO::Boolean(status.At("controller").At("outcome_unknown")) && rig.w.Writes()==before,"Audit refusal wrote hardware or cleared UNKNOWN");
                } else {
                    if(scenario.find("denial")!=std::string::npos) {
                        const auto wrong=recover("wrong-original-recovery","unrelated");Require(IO::String(wrong.At("phase"))=="failed","Wrong original recovered");
                        {std::lock_guard<std::mutex> lock(rig.w.c[1].mutex);rig.w.c[1].Set("mutantConfig","initDone",1);}
                        const auto denied=recover("not-ready-recovery","failed-describe");Require(IO::String(denied.At("phase"))=="failed","Ready node recovered");
                        Require(rig.w.Writes()==before && IO::Boolean(Read(client.Status()).At("controller").At("outcome_unknown")),"Denied recovery repaired hardware or cleared gate");
                        {std::lock_guard<std::mutex> lock(rig.w.c[1].mutex);rig.w.c[1].Set("mutantConfig","initDone",0);}
                    }
                    std::string id="verified-description";
                    if(qtDescriptionRecovery) {
                        IO::WriteNew(root+"/client-options.json","{\"port\":"+std::to_string(port)+",\"credential\":"+EncodeJsonString(credential)+",\"service_id\":"+EncodeJsonString(IO::String(identity.At("service_id")))+",\"client_id\":"+EncodeJsonString(IO::String(identity.At("client_id")))+",\"client_epoch\":"+EncodeJsonString(std::string(64,'a'))+",\"snapshot_sha256\":"+EncodeJsonString(IO::String(identity.At("snapshot_sha256")))+"}");
                        std::vector<std::string> words={argv[9],root+"/client-options.json",root+"/qt-description-result.json"};std::vector<char *> arguments;
                        for(auto &word:words)arguments.push_back(&word[0]);arguments.push_back(nullptr);pid_t probe=0;
                        Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),environ),"Start Qt description recovery client");int status=0;
                        Require(::waitpid(probe,&status,0)==probe && WIFEXITED(status) && WEXITSTATUS(status)==0,"Qt description recovery client failed");id="qt-description-recovery";
                    } else {const auto completed=recover(id,"failed-describe");Require(IO::String(completed.At("phase"))=="complete","Bound recovery did not complete");}
                    const auto status=Read(client.Status());Require(IO::Unsigned(status.At("controller").At("state"))==2 && !IO::Boolean(status.At("controller").At("outcome_unknown")) && !IO::Boolean(status.At("control_needs_reconciliation")) && rig.w.Writes()==before,"Recovery failed to establish Described without writes");
                    auto duplicate=Request(identity,"reconcile-description",id);duplicate.failedOperationId="failed-describe";
                    Require(client.PostOperation(EncodeEccControlRequest(duplicate)).code==202 && rig.w.Writes()==before,"Same recovery ID replayed hardware");
                    IO::WriteNew(root+"/reconciled-status.json",client.Status().body);
                    if(descriptionSuccess) {
                        execute("prepare","prepare",EccState::Prepared);execute("configure","configure",EccState::Ready);execute("start","start",EccState::Running);
                        execute("pause","pause",EccState::Paused);execute("resume","resume",EccState::Running);execute("stop","stop",EccState::Ready);
                        execute("breakup","breakup",EccState::Prepared);execute("reset","unprepare",EccState::Described);execute("reset","forget",EccState::Idle);CheckFiles(Read(client.Status()),raw,4);
                    }
                    const auto beforeRestart=rig.w.Writes();process->CrashAndRestart();
                    const auto restarted=Read(client.Status());Require(IO::Boolean(restarted.At("control_needs_reconciliation")),"Cold service lost gate");
                    auto cold=Request(Read(client.Identity()),"reconcile-description","cold-recovery");cold.failedOperationId="failed-describe";
                    Require(client.PostOperation(EncodeEccControlRequest(cold)).code==202,"Cold recovery not tracked");const auto refused=Terminal(client,"cold-recovery");
                    Require(IO::String(refused.At("phase"))=="failed" && rig.w.Writes()==beforeRestart,"Cold instance adopted original description");
                }
                IO::WriteNew(root+"/description-recovery-result.json","{\"passed\":true,\"read_only_recovery\":true,\"real_hardware\":false}");
            } else if(inputFault) {

                const bool prepared=scenario.find("prepared")!=std::string::npos,auditFault=scenario.find("audit")!=std::string::npos;
                execute("describe","input-describe",EccState::Described);execute("prepare","input-prepare",EccState::Prepared);
                if(!prepared){execute("configure","input-configure",EccState::Ready);execute("start","input-start",EccState::Running);}
                const auto original=DecodeEccControlSystemStatus(direct.Status().body);const auto originalBytes=IO::ReadFile(config,1024*1024);
                const auto bytesBefore=rig.w.Writes();rig.Clear();
                {std::ofstream changed(config,std::ios::binary|std::ios::app);changed<<' ';changed.close();Require(bool(changed),"Change external fixed input");}
                if(qtInputFault) {
                    const auto options="{\"port\":"+std::to_string(port)+",\"credential\":"+EncodeJsonString(credential)+",\"service_id\":"+EncodeJsonString(identity.At("service_id").text)+",\"client_id\":"+EncodeJsonString(identity.At("client_id").text)+",\"client_epoch\":"+EncodeJsonString(original.clientEpoch)+",\"snapshot_sha256\":"+EncodeJsonString(original.snapshotSha256)+",\"store\":"+EncodeJsonString(root+"/qt-input-store")+"}\n";
                    IO::WriteNew(root+"/qt-input-options.json",options);std::vector<std::string> words={argv[9],root+"/qt-input-options.json",root+"/qt-input-result.json"};std::vector<char *> arguments;for(auto &word:words)arguments.push_back(&word[0]);arguments.push_back(nullptr);pid_t probe=0;
                    Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),environ),"Start actual Qt input-fault client");int status=0;Require(::waitpid(probe,&status,0)==probe && WIFEXITED(status) && WEXITSTATUS(status)==0,"Actual Qt input-fault client failed");
                } else {
                Require(client.PostOperation(EncodeEccControlRequest(Request(identity,prepared ? "configure" : "stop","input-positive"))).code==202 && IO::String(Terminal(client,"input-positive").At("phase"))=="failed","Changed external inputs did not durably fail ordinary command");
                }
                auto held=DecodeEccControlSystemStatus(direct.Status().body);Require(held.controlNeedsReconciliation && held.controller.outcomeUnknown && held.controller.operationId==original.controller.operationId,"External input failure did not retain original UNKNOWN scope");
                if(!qtInputFault)Require(rig.w.Writes()==bytesBefore && rig.Actions().empty(),"Input detection issued hardware actions");
                if(auditFault) {
                    {std::ofstream damage(root+"/controller/network/operations.jsonl",std::ios::app);damage<<' ';}
                    Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"stop-production-after-failure","audit-negative"))).code==409,"Fault stop bypassed corrupt network journal");
                    Require(rig.w.Writes()==bytesBefore && rig.Actions().empty(),"Corrupt audit allowed producer actions");
                    IO::WriteNew(root+"/input-fault-result.json","{\"passed\":true,\"audit_fault_refused\":true,\"hardware_unchanged\":true}\n");
                } else {
                    if(!qtInputFault)submit("stop-production-after-failure","input-negative");const auto negativeId=qtInputFault ? "qt-input-negative" : "input-negative";const auto stopped=Terminal(client,negativeId);Require(IO::String(stopped.At("phase"))=="complete","External-input retained-object stop failed");
                    held=DecodeEccControlSystemStatus(direct.Status().body);Require(held.controller.state==original.controller.state && held.controller.operationId==original.controller.operationId && held.controller.outcomeUnknown && held.controlNeedsReconciliation,"Input-fault stop cleared/changed parent state");
                    for(unsigned int i=0;i<2;++i){std::lock_guard<std::mutex> guard(rig.w.c[i].mutex);Require(rig.w.c[i].Get("pipeCtrl","firmwareEnable")==0 && rig.w.c[i].Get("pipeCtrl","intEnable")==0,"Producer bits not stopped");}
                    for(std::size_t i=0;i<original.nodes.size();++i)Require(held.nodes[i].receivers.size()==original.nodes[i].receivers.size(),"Input stop invented receiver completion");
                    rig.Clear();Require(submit("stop-production-after-failure",negativeId).code==202,"Original negative request lost idempotence");Require(rig.Actions().empty(),"Same ID repeated hardware stop");
                    {std::ofstream restore(config,std::ios::binary|std::ios::trunc);restore<<originalBytes;restore.close();Require(bool(restore),"Restore only own external input bytes");}
                    Require(client.PostOperation(EncodeEccControlRequest(Request(identity,prepared ? "configure" : "start","input-after-restore"))).code==202 && IO::String(Terminal(client,"input-after-restore").At("phase"))=="failed","Restoring source cleared fixed-input hold");
                    const auto journal=IO::ReadFile(root+"/controller/network/operations.jsonl",64*1024*1024);Require(journal.find("input-fault-held")!=std::string::npos,"Missing durable original input fault hold");
                    process->CrashAndRestart();const auto next=Read(direct.Identity());held=DecodeEccControlSystemStatus(direct.Status().body);Require(held.controlNeedsReconciliation && held.controller.outcomeUnknown && rig.Actions().empty(),"Reopen replayed stop or cleared external input hold");
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(next,"start","input-cold-positive"))).code==202 && IO::String(Terminal(direct,"input-cold-positive").At("phase"))=="failed","Cold input fault allowed positive takeover");
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(next,"stop-production-after-failure","input-cold-negative"))).code==202,"Cold request not represented for observation");
                    Require(IO::String(Terminal(direct,"input-cold-negative").At("phase"))=="failed" && rig.Actions().empty(),"Cold stop adopted old hardware object");
                    IO::WriteNew(root+"/input-fault-result.json","{\"passed\":true,\"prepared\":"+std::string(prepared ? "true" : "false")+",\"negative_stop_complete\":true,\"original_state_and_operation_retained\":true,\"unknown_preserved\":true,\"restore_did_not_clear_hold\":true,\"reopen_no_replay_or_takeover\":true}\n");
                }
            }
            else if(hardwareObservation) {
                const auto before=direct.Status().body;const auto writes=rig.w.Writes();const auto response=direct.HardwareState();Require(response.code==200,"Hardware state HTTP endpoint failed");const auto observation=DecodeHardwareStateObservation(response.body);Require(observation.instanceId==identity.At("instance_id").text && observation.snapshotSha256==identity.At("snapshot_sha256").text && observation.consistent && !observation.busy,"Hardware observation HTTP binding differs");for(const auto &node:observation.nodes)Require(node.available && node.role=="cobo" && node.values.size()==4,"Fresh CoBo hardware state missing");Require(rig.w.Writes()==writes && direct.Status().body==before,"Hardware observation changed ECC or hardware");IO::WriteNew(root+"/hardware-state.json",response.body);
                auto noAuth=process->direct;noAuth.credentialFile=root+"/wrong-hardware-credential";IO::WriteNew(noAuth.credentialFile,IO::RandomSession()+"\n");Require(NativeControlHttpClient(noAuth).HardwareState().code==401,"Hardware observation accepted wrong credential");Require(rig.w.Writes()==writes,"Unauthenticated query changed hardware");
                const auto name=rig.nodes[0]->fName;rig.nodes[0]->fName="wrong-node";const auto mismatchResponse=direct.HardwareState();const auto mismatch=DecodeHardwareStateObservation(mismatchResponse.body);rig.nodes[0]->fName=name;Require(!mismatch.nodes.front().available && mismatch.nodes.front().values.empty(),"Metadata mismatch retained hardware sample");Require(direct.Status().body==before,"Metadata mismatch cleared/changed control gate");IO::WriteNew(root+"/hardware-state-mismatch.json",mismatchResponse.body);
                const auto originalConfiguration=IO::ReadFile(config,1024*1024);
                std::atomic<unsigned int> observedReads{0};
                rig.devices[0].at("ctrl")->beforeReadField=[&]{++observedReads;};
                {std::ofstream changed(config,std::ios::binary|std::ios::trunc);changed<<originalConfiguration<<' ';changed.close();Require(bool(changed),"Change fixture configuration source");}
                const auto changedInput=direct.HardwareState();
                Require(changedInput.code==503 && observedReads==0,"Changed fixed configuration still queried/displayed hardware bits");
                {std::ofstream restore(config,std::ios::binary|std::ios::trunc);restore<<originalConfiguration;restore.close();Require(bool(restore),"Restore fixture configuration source");}
                const auto restoredInput=direct.HardwareState();
                Require(restoredInput.code==503 && observedReads==0,"Restoring source bytes cleared fixed-input observation gate");
                rig.devices[0].at("ctrl")->beforeReadField={};
                Require(rig.w.Writes()==writes,"Fixed-input observation rejection changed hardware");
                IO::WriteNew(root+"/hardware-state-input-fault.json","{\"changed_source_status\":503,\"restored_source_status\":503,\"fixed_bit_reads\":0,\"hardware_writes\":0}\n");
            }
            else if (scenario=="broken-cleanup-0" || scenario=="broken-cleanup-1" || scenario=="broken-cleanup-readback" || scenario=="broken-cleanup-readback-again") {
                execute("describe","describe",EccState::Described);execute("prepare","prepare",EccState::Prepared);
                execute("configure","configure",EccState::Ready);execute("start","start",EccState::Running);execute("stop","stop",EccState::Ready);
                const auto original=DecodeEccControlSystemStatus(direct.Status().body);
                raw.at("C0")->truncate=true;submit("breakup","partial-finish");
                Require(IO::String(Terminal(direct,"partial-finish").At("phase"))=="failed","Truncated receiver Finish should fail after hardware Breakup");
                const auto failed=DecodeEccControlSystemStatus(direct.Status().body);
                Require(failed.controller.outcomeUnknown && failed.nodes[1].state=="stopped" && failed.nodes[1].receivers[0].sealVerified,"Partial Finish did not preserve peer seal/UNKNOWN");
                const auto sealPath=failed.nodes[1].receivers[0].outputDirectory+"/.getdaq-receiver-seal.json",sealBytes=IO::ReadFile(sealPath,65536);
                rig.Clear();
                const bool readbackFault=scenario=="broken-cleanup-readback" || scenario=="broken-cleanup-readback-again";
                if(scenario=="broken-cleanup-readback-again") {
                    submit("stop-production-after-failure","earlier-broken-stop");
                    Require(IO::String(Terminal(direct,"earlier-broken-stop").At("phase"))=="complete","Initial Broken stop must confirm before stale-proof test");
                }
                if(readbackFault) {std::lock_guard<std::mutex> lock(rig.w.c[1].mutex);rig.w.c[1].Set("pipeCtrl","firmwareEnable",1);}
                submit("stop-production-after-failure","verify-broken-stop");
                Require(IO::String(Terminal(direct,"verify-broken-stop").At("phase"))==(readbackFault ? "failed" : "complete"),"Broken stop readback outcome differs");
                if(readbackFault) {
                    {std::lock_guard<std::mutex> lock(rig.w.c[1].mutex);rig.w.c[1].Set("pipeCtrl","firmwareEnable",0);}
                    submit("stop-production-after-failure","verify-after-restore");
                    Require(IO::String(Terminal(direct,"verify-after-restore").At("phase"))=="failed","Restored register cleared failed verification gate");
                }
                submit("abort-failed-receivers","cleanup-after-breakup");
                Require(IO::String(Terminal(direct,"cleanup-after-breakup").At("phase"))==(readbackFault ? "failed" : "complete"),"ECC cleanup after Breakup differs");
                Require(services[0]->Commands("abort-failed-session")==unsigned(!readbackFault) && services[1]->Commands("abort-failed-session")==0,"Cleaned sealed node or unconfirmed stop");
                const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                Require(after.controller.outcomeUnknown && after.controlNeedsReconciliation && after.dataNeedsReconciliation && after.controller.operationId=="partial-finish","Cleanup cleared original ECC fault");
                for(std::size_t i=0;i<after.nodes.size();++i)Require(after.nodes[i].sessionId==original.nodes[i].sessionId && after.nodes[i].receivers[0].pid==original.nodes[i].receivers[0].pid,"ECC adopted a new cycle");
                Require(after.nodes[1].receivers[0].sealVerified && IO::ReadFile(sealPath,65536)==sealBytes && !after.nodes[0].receivers[0].sealVerified,"ECC cleanup changed/fabricated seal");
                const auto seal=ParseStrictJson(sealBytes);std::string bytes;for(const auto &file:seal.At("files").items)bytes+=IO::ReadFile(after.nodes[1].receivers[0].outputDirectory+'/'+IO::String(file.At("name")),1024*1024);
                Require(bytes==raw.at("C1")->Expected() && rig.Actions().empty(),"Cleanup changed raw or replayed board/sender action");
                IO::WriteNew(root+"/broken-cleanup-proof.json",std::string("{\"passed\":true,\"ecc_cleanup_entry_exercised\":true,\"readback_fault\":")+(readbackFault ? "true" : "false")+",\"sealed_node_cleanup_posts\":0,\"sealed_bytes_preserved\":true,\"hardware_actions_replayed\":false,\"unknown_preserved\":true}\n");
            } else if (receiverStale) {
                execute("describe","describe",EccState::Described);execute("prepare","prepare",EccState::Prepared);
                execute("configure","configure",EccState::Ready);execute("start","start",EccState::Running);
                const auto before=DecodeEccControlSystemStatus(direct.Status().body);for(auto &service:services)service->Capture();
                rig.Clear();receiverBarrier->Arm();Require(!::kill(before.nodes[0].receivers[0].pid,SIGKILL),"Kill only owned receiver for stale observation test");
                receiverBarrier->WaitEntered();const auto held=receiverBarrier->Body();
                IO::WriteNew(root+"/held-receiver-observation.json",held);
                const auto entered=std::chrono::steady_clock::now();
                try {
                    const auto began=std::chrono::steady_clock::now();Read(direct.Status());
                    Require(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(500),"Held receiver observation blocked ECC status");
                    submit("stop","manual-stop");
                    const auto manual=Terminal(direct,"manual-stop");
                    Require(IO::String(manual.At("phase"))=="failed","Dead receiver must leave manual Stop unconfirmed");
                    const auto changed=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(changed.controller.operationId=="manual-stop" && changed.controller.outcomeUnknown,"Manual Stop did not change original controller operation");
                    submit("stop-production-after-failure","manual-negative");
                    Require(IO::String(Terminal(direct,"manual-negative").At("phase"))=="complete","Manual retained-object negative stop failed");
                    receiverBarrier->TargetOffline();services[0]->Stop(); // No future identity/observation can supply the same failed child.
                    receiverBarrier->Release();receiverBarrier->WaitSent();
                    Require(std::chrono::steady_clock::now()-entered<std::chrono::milliseconds(1400),"Manual commands did not finish inside original observation budget");
                } catch(...) {receiverBarrier->Release();throw;}
                // Evidence must be released while its original HTTP client is still pending.
                // The changed controller operation, rather than timeout, invalidates it.
                JsonValue policy;Wait([&] {policy=Read(direct.ReceiverFailure());return IO::String(policy.At("phase"))=="watching" || IO::String(policy.At("phase"))=="observation-unavailable";},3000);
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
                policy=Read(direct.ReceiverFailure());Require(!IO::Boolean(policy.At("latched")) && IO::String(policy.At("operation_id")).empty(),"Stale failed-child response created automatic stop");
                const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                Require(after.controller.outcomeUnknown && after.controlNeedsReconciliation && after.controller.operationId=="manual-stop" &&
                    after.nodes[0].sessionId==before.nodes[0].sessionId && after.nodes[0].receivers[0].pid==before.nodes[0].receivers[0].pid,"Stale response changed retained state/lifecycle");
                unsigned int heldEvents=0,negativeRequests=0;JsonLimits limits;limits.stringBytes=1024*1024;
                std::ifstream journal(root+"/controller/network/operations.jsonl");std::string row;
                while(std::getline(journal,row)) {const auto e=ParseStrictJson(row,limits);
                    if(IO::String(e.At("event"))=="receiver-held")++heldEvents;
                    if(IO::String(e.At("event"))=="accepted" && DecodeEccControlRequest(IO::String(e.At("request"))).command=="stop-production-after-failure")++negativeRequests;
                }
                Require(heldEvents==0 && negativeRequests==1,"Stale response persisted/replayed receiver intent");
                for(const auto &id:{"C0","C1"})Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 &&
                    rig.Count(std::string(id)+".sender-stop")==1 && !rig.Has(std::string(id)+".sender-disconnect"),"Concurrent stale observation repeated or changed manual stop");
                IO::WriteNew(root+"/receiver-failure-policy.json",direct.ReceiverFailure().body);
                IO::WriteNew(root+"/stale-receiver-proof.json","{\"passed\":true,\"genuine_failed_observation_held\":true,\"original_http_client_still_pending\":true,\"controller_operation_changed\":true,\"stale_hold_events\":0,\"only_manual_negative_request\":true,\"status_responsive\":true}\n");
            } else if (receiverCase) {
                execute("describe","describe",EccState::Described); execute("prepare","prepare",EccState::Prepared);
                execute("configure","configure",EccState::Ready);
                if (scenario!="receiver-ready") execute("start","start",EccState::Running);
                if (scenario=="receiver-paused") execute("pause","pause",EccState::Paused);
                const auto beforeBytes=direct.Status().body; const auto before=DecodeEccControlSystemStatus(beforeBytes);
#ifdef GETDAQ_TEST_ICE_SOURCE
                if (iceReceiverFailure) {
                    Wait([&] {
                        for (const auto &entry:iceProducers) if (entry.second->read[0]!=entry.second->write[0] || entry.second->read[1]!=entry.second->write[1]) return false;
                        return true;
                    },5000);
                    for (const auto &entry:iceProducers) Require(entry.second->readout->Status().workerRunning,"ICE producer worker was not active before receiver failure");
                    IO::WriteNew(root+"/before-receiver-failure-sources.json",direct.Sources().body);
                }
#endif
                for (auto &service:services) service->Capture();
                IO::WriteNew(root+"/before-receiver-failure.json",beforeBytes); rig.Clear();
                if (receiverStopLoss) rig.devices[0].at("ctrl")->failFirmwareStop=true;
                const bool normalExit=scenario=="receiver-normal-exit",offline=scenario=="receiver-offline";
                if (normalExit) {
                    raw.at("C0")->FixtureClose();
                    Wait([&] {
                        const auto o=DecodeEccControlReceiverObservation(direct.Receivers().body);
                        return o.nodes[0].available && !o.nodes[0].receivers.empty() && o.nodes[0].receivers[0].state=="exited" && o.nodes[0].receivers[0].exitCode==0;
                    },5000);
                } else if (offline) services[0]->Stop();
                else Require(!::kill(before.nodes[0].receivers[0].pid,SIGKILL),"Kill only fixture-owned receiver");
                JsonValue policy;
                const bool actionable=receiverEnabled && !normalExit && !offline;
                if (actionable) {
                    Wait([&] {policy=Read(direct.ReceiverFailure());const auto phase=IO::String(policy.At("phase"));return phase=="complete" || phase=="failed" || phase=="unknown";},30000);
                    const auto automatic=IO::String(policy.At("operation_id"));
                    Require(automatic.compare(0,18,"internal-receiver-")==0,"Receiver failure lacks reserved stop intent");
                    Require(IO::String(Terminal(direct,automatic).At("phase"))==(receiverStopLoss ? "failed" : "complete"),"Receiver negative stop outcome differs");
                    IO::WriteNew(root+"/operation-receiver-stop.json",direct.Operation(automatic).body);
                    Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"start","after-receiver-failure"))).code==409,"Receiver hold allowed production restart");
                    const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(after.controller.state==before.controller.state && after.controller.operationId==before.controller.operationId &&
                        after.controller.outcomeUnknown && after.controlNeedsReconciliation,"Receiver fault cleared original state/operation or gate");
                    for (std::size_t i=0;i<after.nodes.size();++i) Require(after.nodes[i].instanceId==before.nodes[i].instanceId &&
                        after.nodes[i].sessionId==before.nodes[i].sessionId && after.nodes[i].receivers[0].pid==before.nodes[i].receivers[0].pid &&
                        !after.nodes[i].receivers[0].reaped && !after.nodes[i].receivers[0].sealVerified,"Receiver fault adopted/reaped/sealed original lifecycle");
                    for (const auto &id:{"C0","C1"}) {
                        const auto count=rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0");
                        Require(count==unsigned(scenario!="receiver-paused"),"Receiver fault repeated/skipped production stop");
                        Require((ice ? rig.Count(std::string(id)+".sender-stop")==1 : !rig.Has(std::string(id)+".sender-stop")) && !rig.Has(std::string(id)+".sender-disconnect"),"Receiver negative stop skipped/replayed ICE producer or flushed/disconnected ordinary sender");
                    }
                    rig.Clear(); std::this_thread::sleep_for(std::chrono::milliseconds(350));
#ifdef GETDAQ_TEST_ICE_SOURCE
                    if (iceReceiverFailure) {
                        const auto sourceBytes=direct.Sources().body;
                        const auto source=DecodeEccControlSourceObservation(sourceBytes);
                        const auto original=DecodeEccControlSourceObservation(IO::ReadFile(root+"/before-receiver-failure-sources.json",256*1024));
                        Require(source.sources.size()==2 && source.controllerOperationId==before.controller.operationId,"Receiver failure changed source set/controller operation");
                        for (std::size_t i=0;i<source.sources.size();++i) {
                            const auto &row=source.sources[i],&old=original.sources[i];
                            Require(row.phase=="available" && row.state=="unknown" && row.localState==9 && row.failureStopIssued && row.failureStopAcknowledged &&
                                !row.sourceEndAcknowledged && !row.finishIssued && !row.drainAcknowledged && row.id==old.id && row.instanceId==old.instanceId &&
                                row.receiverSession==old.receiverSession && row.issuedBytes==old.issuedBytes && row.locallySentBytes==old.locallySentBytes,
                                "ICE receiver fault adopted source, drained/new data, fabricated End or lost failure-stop acknowledgement");
                        }
                        for (const auto &entry:iceProducers) {
                            const auto readout=entry.second->readout->Status();
                            Require(!readout.workerRunning && readout.deviceStopAcknowledged,"ICE receiver fault left worker/device running");
                        }
                        for (const auto &node:after.nodes) for (const auto &child:node.receivers)
                            Require(!child.sourceEndConfirmed && !child.sourceUnusedConfirmed && !child.sealVerified,"ICE receiver fault fabricated data end or seal");
                        IO::WriteNew(root+"/after-receiver-failure-sources.json",sourceBytes);
                        IO::WriteNew(root+"/ice-receiver-failure-proof.json","{\"passed\":true,\"retained_source_instances_sessions\":true,\"source_workers_stopped\":true,\"device_stop_acknowledged\":true,\"drain_requested\":false,\"end_requested\":false,\"receiver_cleanup_requested\":false}\n");
                    }
#endif
                    Require(rig.Actions().empty(),"Receiver fault poll replayed stop");
                    unsigned int holds=0,stops=0;std::ifstream journal(root+"/controller/network/operations.jsonl");std::string row;
                    JsonLimits limits;limits.stringBytes=1024*1024;
                    while (std::getline(journal,row)) {const auto e=ParseStrictJson(row,limits);const auto name=IO::String(e.At("event"));
                        if (name=="receiver-held")++holds;
                        if (name=="accepted" && DecodeEccControlRequest(IO::String(e.At("request"))).command=="stop-production-after-failure")++stops;
                    }
                    Require(holds==1 && stops==1,"Receiver fault accepted repeated intent/stop");
                    if (scenario=="receiver-restart" || scenario=="ice-receiver-restart") {
                        process->Stop();process->Start();const auto reopened=Read(direct.ReceiverFailure());
                        Require(IO::String(reopened.At("phase"))=="reopened-held" && IO::String(reopened.At("operation_id"))==automatic &&
                            IO::String(reopened.At("evidence").At("observation"))==IO::String(policy.At("evidence").At("observation")),"Reopen lost original receiver evidence");
                        std::this_thread::sleep_for(std::chrono::milliseconds(250));Require(rig.Actions().empty(),"Reopen replayed receiver stop");
                    }
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(700)); policy=Read(direct.ReceiverFailure());
                    Require(!IO::Boolean(policy.At("latched")) && IO::String(policy.At("operation_id")).empty(),"Disabled/normal/unavailable receiver inferred failure");
                    Require(rig.Actions().empty(),"Disabled/normal/unavailable receiver sent hardware stop");
                }
                IO::WriteNew(root+"/receiver-failure-policy.json",direct.ReceiverFailure().body);
                for (const auto &service:services) Require(service->Commands("finish")==0 && service->Commands("start")==1,"Receiver failure changed data lifecycle");
                if(receiverCleanup) {
                    const auto cleanup=Request(identity,"abort-failed-receivers","explicit-receiver-cleanup");
                    const bool auditChanged=scenario=="receiver-cleanup-audit";
                    std::string auditPath,originalAudit;
                    if(auditChanged) {
                        auditPath=root+"/controller/operations/"+IO::String(policy.At("operation_id"))+"/operation.jsonl";originalAudit=IO::ReadFile(auditPath,1024*1024);
                        std::ofstream changed(auditPath,std::ios::binary|std::ios::trunc);changed<<originalAudit<<' ';changed.close();
                    }
                    Require(client.PostOperation(EncodeEccControlRequest(cleanup)).code==202,"ECC cleanup not accepted");
                    Require(IO::String(Terminal(direct,cleanup.operationId).At("phase"))==((receiverStopLoss || auditChanged) ? "failed" : "complete"),"ECC receiver cleanup outcome differs");
                    if(receiverStopLoss || auditChanged) {
                        for(const auto &service:services) Require(service->Commands("abort-failed-session")==0,"Unconfirmed/corrupt production stop allowed receiver signals");
                        if(auditChanged) {
                            std::ofstream restore(auditPath,std::ios::binary|std::ios::trunc);restore<<originalAudit;restore.close();
                            const auto retry=Request(identity,"abort-failed-receivers","after-proof-restoration");
                            Require(client.PostOperation(EncodeEccControlRequest(retry)).code==202,"Restoration denial was not observable");
                            Require(IO::String(Terminal(direct,retry.operationId).At("phase"))=="failed","Restoring proof cleared latched storage fault");
                            for(const auto &service:services) Require(service->Commands("abort-failed-session")==0,"Restored proof reenabled cleanup");
                        }
                    } else {
                    const auto cleaned=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(cleaned.controller.outcomeUnknown && cleaned.controlNeedsReconciliation && cleaned.dataNeedsReconciliation,"Cleanup cleared parent/data fault");
                    for(std::size_t i=0;i<cleaned.nodes.size();++i) {
                        Require(cleaned.nodes[i].instanceId==before.nodes[i].instanceId && cleaned.nodes[i].sessionId==before.nodes[i].sessionId,"Cleanup adopted another data instance/session");
                        for(const auto &receiver:cleaned.nodes[i].receivers) Require(receiver.reaped && !receiver.sealVerified,"Cleanup invented complete data or left an owned process");
                    }
                    Require(rig.Actions().empty(),"Receiver cleanup sent extra hardware commands");
                    Require(client.PostOperation(EncodeEccControlRequest(cleanup)).code==202,"Same cleanup ID not observable");
                    IO::WriteNew(root+"/receiver-cleanup-status.json",direct.Status().body);
                    IO::WriteNew(root+"/receiver-cleanup-operation.json",direct.Operation(cleanup.operationId).body);
                    }
                }
            } else if (alarmUnknown) {
                execute("describe","describe",EccState::Described);
                bool sent=false;
                if (alarmUnknownPrepare) {
                    rig.w.c[1].onAction=[&](const std::string &s) {
                        const bool first=!sent; if (first) { sent=true; rig.Clear(); }
                        rig.w.Record("C1."+s);
                        if (first) rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                    };
                    submit("prepare","prepare");
                } else {
                    execute("prepare","prepare",EccState::Prepared); execute("configure","configure",EccState::Ready);
                    raw.at("C0")->failStart=true; submit("start","start");
                }
                const std::string failedId=alarmUnknownPrepare ? "prepare" : "start";
                Require(IO::String(Terminal(client,failedId).At("phase"))=="failed","UNKNOWN alarm fixture did not fail original operation");
                IO::WriteNew(root+"/operation-original-failure.json",client.Operation(failedId).body);
                if (!alarmUnknownPrepare) { rig.Clear(); rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); }
                JsonValue alarm; Wait([&] { alarm=Read(direct.Alarms()); const auto phase=IO::String(alarm.At("phase")); return phase=="complete" || phase=="failed" || phase=="unknown"; },30000);
                const auto automatic=IO::String(alarm.At("operation_id")); Require(!automatic.empty(),"UNKNOWN alarm has no unique automatic operation");
                const auto stop=Terminal(direct,automatic);
                if (alarmUnknownPrepare) rig.w.c[1].onAction=[&](const std::string &s) { rig.w.Record("C1."+s); };
                Require(IO::String(stop.At("phase"))=="complete" && IO::String(stop.At("request").At("command"))=="stop-production-after-failure","UNKNOWN alarm did not use retained-object negative stop");
                IO::WriteNew(root+"/alarm-status.json",direct.Alarms().body); IO::WriteNew(root+"/operation-automatic-stop.json",direct.Operation(automatic).body);
                const auto current=Read(direct.Status());
                Require(IO::Boolean(current.At("controller").At("outcome_unknown")) && IO::String(current.At("controller").At("operation_id"))==failedId &&
                    IO::Unsigned(current.At("controller").At("state"))==unsigned(alarmUnknownPrepare ? EccState::Described : EccState::Ready) && IO::Boolean(current.At("control_needs_reconciliation")),"Automatic negative stop cleared original UNKNOWN");
                for (const auto &id:{"C0","C1"}) {
                    Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count(std::string(id)+".ctrl.pipeCtrl.intEnable=0")==1 &&
                        !rig.Has(std::string(id)+".sender-stop") && !rig.Has(std::string(id)+".sender-disconnect"),"UNKNOWN alarm stop repeated/skipped producer or touched sender");
                }
                for (const auto &service:services) Require(service->Commands("start")==unsigned(!alarmUnknownPrepare) && service->Commands("finish")==0,"UNKNOWN alarm changed receiver lifecycle");
                rig.Clear(); rig.w.alarms[0]->sendAsadAlarm("CoBo0",8,Ice::Current()); std::this_thread::sleep_for(std::chrono::milliseconds(150));
                Require(rig.Actions().empty(),"Repeated UNKNOWN alarm replayed negative stop");
                Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"reset","unverified-reset"))).code==409,"Automatic negative stop cleared positive command gate");
                if(alarmUnknownPrepare) {
                    submit("unprepare-after-failure","alarm-preparation-cleanup");
                    Require(IO::String(Terminal(client,"alarm-preparation-cleanup").At("phase"))=="complete","Original alarmed preparation cleanup failed");
                    const auto writes=rig.w.Writes();
                    auto request=Request(identity,"reconcile-preparation","alarm-preparation-recovery");request.failedOperationId=failedId;
                    Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Alarmed preparation recovery intent not recorded");
                    const auto denied=Terminal(client,request.operationId);
                    Require(IO::String(denied.At("phase"))=="failed" && IO::String(denied.At("message")).find("independent alarm/input/storage hold")!=std::string::npos &&
                        rig.w.Writes()==writes && DecodeEccControlSystemStatus(direct.Status().body).controller.outcomeUnknown,
                        "Generic preparation recovery cleared independent alarm after cleanup");
                    IO::WriteNew(root+"/alarm-preparation-recovery-denied.json",client.Operation(request.operationId).body);
                    if(preparationAlarmAck) {
                        request.command="reconcile-preparation-with-alarm";request.operationId="wrong-preparation-alarm";request.alarmOperationId="internal-alarm-unrelated";
                        Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Wrong alarm binding intent not recorded");
                        Require(IO::String(Terminal(client,request.operationId).At("phase"))=="failed" && rig.w.Writes()==writes,"Wrong Prepare alarm binding performed recovery");
                        request.operationId="verified-preparation-alarm";request.alarmOperationId=automatic;
                        Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Explicit preparation alarm recovery not accepted");
                        Require(IO::String(Terminal(client,request.operationId).At("phase"))=="complete","Explicit preparation alarm recovery failed");
                        IO::WriteNew(root+"/operation-verified-preparation-alarm.json",client.Operation(request.operationId).body);
                        const auto recovered=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(recovered.controller.state==2 && !recovered.controller.outcomeUnknown && !recovered.controlNeedsReconciliation &&
                            !recovered.dataNeedsReconciliation && recovered.controller.operationId==request.operationId && rig.w.Writes()==writes,
                            "Preparation alarm recovery did not establish Described without writes");
                        const auto rearmed=DecodeEccControlAlarmStatus(direct.Alarms().body);
                        Require(rearmed.enabled && !rearmed.latched && rearmed.phase=="watching" && rearmed.operationId.empty(),"Original Prepare alarm not rearmed");
                        IO::WriteNew(root+"/after-preparation-alarm-ack.json",direct.Alarms().body);
                        execute("prepare","fresh-prepare",EccState::Prepared);
                        for(const auto &service:services)Require(service->Commands("start")==0 && service->Commands("finish")==0,"Preparation recovery changed receiver lifecycle");
                        rig.Clear();process->CrashAndRestart();const auto reopened=Read(direct.Identity());
                        const auto coldAlarm=DecodeEccControlAlarmStatus(direct.Alarms().body);
                        Require(coldAlarm.latched && coldAlarm.phase=="startup-reconciliation" && coldAlarm.operationId.empty(),"Cold replay did not retain startup alarm gate");
                        auto cold=Request(reopened,"reconcile-preparation-with-alarm","cold-preparation-alarm");cold.failedOperationId=failedId;cold.alarmOperationId=automatic;
                        Require(direct.PostOperation(EncodeEccControlRequest(cold)).code==202,"Cold Prepare acknowledgement not recorded");
                        Require(IO::String(Terminal(direct,cold.operationId).At("phase"))=="failed" && rig.Actions().empty(),"Cold Prepare acknowledgement adopted old objects");
                        IO::WriteNew(root+"/cold-preparation-alarm-denied.json",direct.Operation(cold.operationId).body);
                        IO::WriteNew(root+"/preparation-alarm-recovery-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                            ",\"original_alarm_id\":"+EncodeJsonString(automatic)+",\"recovery_hardware_writes\":0,\"fresh_prepare_verified\":true,\"cold_replay_held\":true,\"receiver_starts\":0}\n");
                    }

                }

            } else if(failureBreakup) {
                execute("describe","describe",EccState::Described);execute("prepare","prepare",EccState::Prepared);
                execute("configure","configure",EccState::Ready);execute("start","start",EccState::Running);
                std::string original="start";
                const bool unknown=scenario=="failure-breakup-network-unknown";
                if(unknown) {
                    raw.at("C0")->failStop=true;submit("pause","failed-pause");Require(IO::String(Terminal(client,"failed-pause").At("phase"))=="failed","Sender ACK loss did not fail Pause");
                    raw.at("C0")->failStop=false;original="failed-pause";submit("stop-production-after-failure","failure-production-stop");
                    Require(IO::String(Terminal(client,"failure-production-stop").At("phase"))=="complete","Retained stop after unknown Pause failed");
                } else {
                    const auto live=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(live.nodes.size()==2 && live.nodes[0].receivers.size()==1 && live.nodes[0].receivers[0].pid>0,"No original receiver to fail");
                    Require(!::kill(live.nodes[0].receivers[0].pid,SIGKILL),"Inject owned receiver failure");
                    Wait([&]{const auto held=Read(direct.ReceiverFailure());return IO::String(held.At("phase"))=="complete";},180000);
                }
                if(acquisitionRecoveryAlarm) {
                    rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                    Wait([&]{return IO::String(Read(direct.Alarms()).At("phase"))=="complete";},180000);
                    IO::WriteNew(root+"/independent-alarm-held.json",direct.Alarms().body);
                    Require(IO::Boolean(Read(direct.Alarms()).At("latched")),"Independent hardware alarm was not held");
                }
                const auto before=DecodeEccControlSystemStatus(direct.Status().body);Require(before.controller.outcomeUnknown && before.controller.operationId==original,"Failure cleanup parent not held");
                rig.Clear();const bool stopLoss=scenario=="failure-breakup-network-stop-loss",disconnectLoss=scenario=="failure-breakup-network-disconnect-loss";
                raw.at("C0")->failStop=stopLoss;raw.at("C0")->failDisconnect=disconnectLoss;
                submit("breakup-after-failure","failure-connections");const auto cleaned=Terminal(client,"failure-connections");
                Require(IO::String(cleaned.At("phase"))==((unknown || stopLoss || disconnectLoss) ? "failed" : "complete"),"Failure connection cleanup phase differs");
                const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                Require(after.controller.state==before.controller.state && after.controller.operationId==original && after.controller.outcomeUnknown && after.controlNeedsReconciliation,"Failure cleanup cleared/rebound parent");
                for(std::size_t i=0;i<services.size();++i) {
                    Require(services[i]->Commands("start")==1 && services[i]->Commands("finish")==0 && services[i]->Commands("abort-failed-session")==0,"Connection cleanup changed receiver lifecycle");
                    Require(after.nodes[i].sessionId==before.nodes[i].sessionId && after.nodes[i].receivers[0].pid==before.nodes[i].receivers[0].pid,"Connection cleanup adopted/replaced receivers");
                }
                if(unknown)Require(rig.Actions().empty(),"Unknown sender preflight performed actions");
                else {
                    for(const auto &id:{"C0","C1"})Require(rig.Count(std::string(id)+".sender-stop")==1,"Failure cleanup skipped sender stop");
                    Require(rig.Count("C1.sender-disconnect")==1 && rig.Count("C0.sender-disconnect")==unsigned(!stopLoss),"Failure cleanup failed independent close/closed unknown stop");
                    if(!stopLoss && !disconnectLoss)Require(!raw.at("C0")->Connected() && !raw.at("C1")->Connected(),"Successful cleanup retained raw sockets");
                }
                if(receiverRetirement) {
                    rig.Clear();submit("retire-failed-receivers","without-receiver-abort");
                    Require(IO::String(Terminal(client,"without-receiver-abort").At("phase"))=="failed","Retirement accepted missing original Abort proof");
                    Require(services[0]->Commands("retire-failed-session")==0 && services[1]->Commands("retire-failed-session")==0 && rig.Actions().empty(),"Missing proof touched receivers/hardware");
                    submit("abort-failed-receivers","original-receiver-abort");Require(IO::String(Terminal(client,"original-receiver-abort").At("phase"))=="complete","Original Abort failed before retirement");
                    submit("retire-failed-receivers","original-receiver-retirement");Require(IO::String(Terminal(client,"original-receiver-retirement").At("phase"))=="complete","Bound data retirement failed");
                    const auto retired=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(retired.controller.outcomeUnknown && retired.controlNeedsReconciliation && !retired.dataNeedsReconciliation && retired.controller.operationId==original,"Data retirement cleared parent or retained wrong data hold");
                    for(std::size_t i=0;i<services.size();++i) {
                        Require(services[i]->Commands("retire-failed-session")==1 && services[i]->Commands("abort-failed-session")==1 && services[i]->Commands("start")==1,"Retirement replayed or restarted receivers");
                        const auto &node=retired.nodes[i];Require(node.state=="retired" && node.sessionId==before.nodes[i].sessionId && node.receivers[0].pid==before.nodes[i].receivers[0].pid && node.receivers[0].reaped && !node.receivers[0].sealVerified,"Retirement changed original identity or fabricated seal");
                    }
                    Require(rig.Actions().empty(),"Retirement performed board/sender actions");
                    if(acquisitionUnprepare) {
                        const bool loss=scenario=="acquisition-unprepare-network-power-loss";
                        rig.devices[0].at("ctrl")->failPowerOff=loss;rig.Clear();
                        submit("unprepare-acquisition-after-failure","original-acquisition-unprepare");
                        Require(IO::String(Terminal(client,"original-acquisition-unprepare").At("phase"))==(loss ? "failed" : "complete"),"Acquisition Unprepare terminal differs");
                        const auto poweredOff=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(poweredOff.controller.outcomeUnknown && poweredOff.controlNeedsReconciliation && !poweredOff.dataNeedsReconciliation && poweredOff.controller.operationId==original,"Power-off cleared parent or changed retired data");
                        for(unsigned int i=0;i<2;++i)Require(rig.w.c[i].powered==0,"Power-off skipped independent board");
                        for(const auto &service:services)Require(service->Commands("start")==1 && service->Commands("finish")==0 && service->Commands("abort-failed-session")==1 && service->Commands("retire-failed-session")==1,"Power-off modified data lifecycle");
                        if(loss)Require(rig.w.alarms[0]->unsubscribes==0,"Unknown power-off closed alarm subscription");
                        else Require(rig.w.alarms[0]->unsubscribes==1,"Power-off did not close original subscription");
                        rig.devices[0].at("ctrl")->failPowerOff=false;rig.Clear();submit("unprepare-acquisition-after-failure","repeat-acquisition-unprepare");
                        Require(IO::String(Terminal(client,"repeat-acquisition-unprepare").At("phase"))=="failed" && rig.Actions().empty(),"Another ID replayed power-off");
                        IO::WriteNew(root+"/acquisition-unprepare-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"parent_unknown_preserved\":true,\"data_retired_preserved\":true,\"all_power_off_observed\":true,\"power_off_ack_loss\":"+(loss ? "true" : "false")+"}\n");
                    }

                    if(acquisitionRecoveryAlarm) {
                        const auto writes=rig.w.Writes();
                        for(const auto &id:{"denied-recovery","denied-recovery-again"}) {
                            auto request=Request(identity,"reconcile-acquisition",id);request.failedOperationId=original;
                            Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Independent alarm recovery intent not recorded");
                            const auto denied=Terminal(client,id);
                            Require(IO::String(denied.At("phase"))=="failed" && IO::String(denied.At("message")).find("independent alarm/input/storage hold")!=std::string::npos,
                                "Acquisition recovery cleared or ignored independent hardware alarm");
                            IO::WriteNew(root+"/operation-"+id+".json",client.Operation(id).body);
                        }
                        const auto held=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(held.controller.outcomeUnknown && held.controlNeedsReconciliation && !held.dataNeedsReconciliation &&
                            held.controller.operationId==original && rig.w.Writes()==writes &&
                            IO::Boolean(Read(direct.Alarms()).At("latched")) && IO::Boolean(Read(direct.ReceiverFailure()).At("latched")),
                            "Independent alarm denial changed original binding, policy hold or hardware");
                        for(const auto &service:services)Require(service->Commands("start")==1 && service->Commands("finish")==0 &&
                            service->Commands("retire-failed-session")==1,"Denied recovery replayed receiver lifecycle");
                        IO::WriteNew(root+"/independent-alarm-final.json",direct.Alarms().body);
                        IO::WriteNew(root+"/acquisition-recovery-alarm-denial-result.json","{\"passed\":true,\"encoding_minor\":"+
                            std::to_string(rig.options.encodingMinor)+",\"independent_alarm_preserved\":true,\"original_unknown_preserved\":true,\"original_data_retired\":true,\"recovery_hardware_writes\":0,\"denied_recovery_attempts\":2}\n");
                        if(acquisitionAlarmAck) {
                            const auto alarmId=IO::String(Read(direct.Alarms()).At("operation_id"));
                            auto request=Request(identity,"reconcile-acquisition-with-alarm","wrong-alarm-ack");request.failedOperationId=original;request.alarmOperationId="internal-alarm-unrelated";
                            Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Wrong alarm binding intent not recorded");
                            Require(IO::String(Terminal(client,"wrong-alarm-ack").At("phase"))=="failed","Wrong original alarm ID was acknowledged");
                            request.operationId="verified-alarm-acquisition";request.alarmOperationId=alarmId;
                            Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Bound alarm acknowledgement not accepted");
                            Require(IO::String(Terminal(client,request.operationId).At("phase"))=="complete","Bound alarm acknowledgement/recovery failed");
                            IO::WriteNew(root+"/operation-verified-alarm-acquisition.json",client.Operation(request.operationId).body);
                            const auto recovered=DecodeEccControlSystemStatus(direct.Status().body);
                            Require(recovered.controller.state==2 && !recovered.controller.outcomeUnknown && !recovered.controlNeedsReconciliation &&
                                !recovered.dataNeedsReconciliation && recovered.controller.operationId==request.operationId && rig.w.Writes()==writes,
                                "Alarm acknowledgement did not return bound Described without writes");
                            Require(!IO::Boolean(Read(direct.Alarms()).At("latched")) && IO::String(Read(direct.Alarms()).At("phase"))=="watching" &&
                                !IO::Boolean(Read(direct.ReceiverFailure()).At("latched")),"Acknowledged original policies were not rearmed");
                            IO::WriteNew(root+"/after-ack-alarm-policy.json",direct.Alarms().body);
                            execute("prepare","fresh-prepare",EccState::Prepared);execute("configure","fresh-configure",EccState::Ready);execute("start","fresh-start",EccState::Running);
                            const auto next=DecodeEccControlSystemStatus(direct.Status().body);Require(next.nodes[0].sessionId!=before.nodes[0].sessionId,"Alarm recovery reused original data session");
                            rig.Clear();rig.w.alarms[0]->sendAsadAlarm("CoBo0",2,Ice::Current());
                            Wait([&]{return IO::String(Read(direct.Alarms()).At("phase"))=="complete";},180000);
                            const auto again=DecodeEccControlSystemStatus(direct.Status().body);
                            Require(again.controller.state==4 && again.controlNeedsReconciliation && !again.controller.outcomeUnknown &&
                                IO::String(Read(direct.Alarms()).At("operation_id"))!=alarmId,"New alarm did not stop the new Run with a distinct hold");
                            for(const auto &service:services)Require(service->Commands("start")==2 && service->Commands("finish")==0,"Alarm acknowledgement replayed/sealed data");
                            IO::WriteNew(root+"/second-alarm-held.json",direct.Alarms().body);
                            rig.Clear();process->CrashAndRestart();
                            const auto reopened=Read(direct.Identity());
                            Require(IO::String(reopened.At("instance_id"))!=IO::String(identity.At("instance_id")) &&
                                IO::Boolean(Read(direct.Status()).At("control_needs_reconciliation")),"Cold replay lost original records or cleared startup gate");
                            auto cold=Request(reopened,"reconcile-acquisition-with-alarm","cold-alarm-ack");cold.failedOperationId=original;cold.alarmOperationId=alarmId;
                            Require(direct.PostOperation(EncodeEccControlRequest(cold)).code==202,"Cold acknowledgement intent not recorded");
                            Require(IO::String(Terminal(direct,cold.operationId).At("phase"))=="failed","Cold service adopted old acknowledgement");
                            Require(direct.PostOperation(EncodeEccControlRequest(Request(reopened,"start","cold-start"))).code==409 && rig.Actions().empty(),
                                "Cold replay performed hardware actions or resumed production");
                            IO::WriteNew(root+"/cold-alarm-recovery-denial.json",direct.Operation(cold.operationId).body);
                            const auto coldReceiver=DecodeEccControlReceiverFailureStatus(direct.ReceiverFailure().body);
                            const auto coldAlarm=DecodeEccControlAlarmStatus(direct.Alarms().body);
                            Require(coldReceiver.latched && coldReceiver.phase=="startup-reconciliation" && coldReceiver.operationId.empty() &&
                                coldAlarm.latched && coldAlarm.phase=="reopened-held","Cold policy cache cannot be decoded as a retained startup hold");
                            IO::WriteNew(root+"/cold-receiver-failure.json",direct.ReceiverFailure().body);
                            IO::WriteNew(root+"/cold-alarm-policy.json",direct.Alarms().body);


                            IO::WriteNew(root+"/alarm-ack-recovery-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                                ",\"recovery_hardware_writes\":0,\"original_alarm_id\":"+EncodeJsonString(alarmId)+",\"both_policies_rearmed\":true,\"new_alarm_stopped_new_run\":true,\"cold_replay_held\":true,\"old_run_complete\":false}\n");
                        }
                    } else if(acquisitionRecovery) {
                        const auto writes=rig.w.Writes();
                        auto recovery=Request(identity,"reconcile-acquisition","verified-acquisition");recovery.failedOperationId=original;
                        Require(client.PostOperation(EncodeEccControlRequest(recovery)).code==202,"Acquisition recovery request not accepted");
                        const auto completed=Terminal(client,"verified-acquisition");
                        IO::WriteNew(root+"/operation-verified-acquisition.json",client.Operation("verified-acquisition").body);
                        Require(IO::String(completed.At("phase"))=="complete","Acquisition recovery failed");
                        const auto recovered=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(recovered.controller.state==int(EccState::Described) && !recovered.controller.outcomeUnknown &&
                            !recovered.controlNeedsReconciliation && !recovered.dataNeedsReconciliation &&
                            recovered.controller.operationId=="verified-acquisition" && rig.w.Writes()==writes,
                            "Recovery failed to establish bound Described without writes");
                        Require(IO::String(Read(direct.ReceiverFailure()).At("phase"))=="watching","Receiver failure policy was not rearmed");
                        for(const auto &service:services)Require(service->Commands("start")==1 && service->Commands("finish")==0,
                            "Recovery restarted receivers or sealed interrupted Run");
                        execute("prepare","fresh-prepare",EccState::Prepared);
                        execute("configure","fresh-configure",EccState::Ready);
                        execute("start","fresh-start",EccState::Running);
                        const auto next=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(next.nodes[0].sessionId!=before.nodes[0].sessionId && next.nodes[0].receivers[0].pid!=before.nodes[0].receivers[0].pid,
                            "Fresh Run reused retired session or original receiver");
                        rig.Clear();Require(!::kill(next.nodes[0].receivers[0].pid,SIGKILL),"Inject second owned receiver failure");
                        Wait([&]{return IO::String(Read(direct.ReceiverFailure()).At("phase"))=="complete";},180000);
                        const auto again=DecodeEccControlSystemStatus(direct.Status().body);
                        Require(again.controller.outcomeUnknown && again.controller.operationId=="fresh-start" && again.controlNeedsReconciliation,
                            "Rearmed receiver policy did not hold the new Run");
                        for(const auto &id:{"C0","C1"})Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1,
                            "Rearmed receiver policy skipped or repeated new production stop");
                        for(const auto &service:services)Require(service->Commands("start")==2 && service->Commands("finish")==0,
                            "Second failure replayed lifecycle or fabricated normal completion");
                        IO::WriteNew(root+"/second-receiver-failure.json",direct.ReceiverFailure().body);
                        IO::WriteNew(root+"/acquisition-recovery-result.json","{\"passed\":true,\"encoding_minor\":"+
                            std::to_string(rig.options.encodingMinor)+",\"recovery_hardware_writes\":0,\"receiver_policy_rearmed\":true,\"fresh_prepare_verified\":true,\"second_failure_stopped_new_run\":true,\"original_run_complete\":false}\n");
                    }
                    IO::WriteNew(root+"/receiver-retirement-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"parent_unknown_preserved\":true,\"data_retired\":true,\"receivers_restarted\":false,\"hardware_actions\":0}\n");
                }
                raw.at("C0")->failStop=raw.at("C0")->failDisconnect=false;rig.Clear();
                if(!acquisitionRecovery)submit("breakup-after-failure","another-failure-connections");if(!acquisitionRecovery)Require(IO::String(Terminal(client,"another-failure-connections").At("phase"))=="failed" && rig.Actions().empty(),"New cleanup ID replayed steps");
                IO::WriteNew(root+"/failure-breakup-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"unknown_preserved\":true,\"receiver_lifecycle_unchanged\":true,\"sender_cleanup_confirmed\":"+std::string(unknown || stopLoss || disconnectLoss ? "false" : "true")+"}");
            } else if (alarmCase) {
                execute("describe","describe",EccState::Described); execute("prepare","prepare",EccState::Prepared); execute("configure","configure",EccState::Ready);
                if (!alarmReady) execute("start","start",EccState::Running);
                if (scenario=="alarm-paused") execute("pause","pause",EccState::Paused);
                IO::WriteNew(root+"/before-alarm-status.json",direct.Status().body);
                rig.Clear(); if (scenario=="alarm-stop-loss") rig.devices[0].at("ctrl")->failFirmwareStop=true;
                if (scenario=="alarm-during-stop") {
                    bool sent=false;
                    rig.w.c[0].onAction=[&](const std::string &s) {
                        rig.w.Record("C0."+s);
                        if (!sent && s=="ctrl.pipeCtrl.firmwareEnable=0") { sent=true; rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); }
                    };
                    submit("stop","manual-stop");
                    Require(IO::String(Terminal(client,"manual-stop").At("phase"))=="complete","Manual Stop during alarm did not complete");
                    IO::WriteNew(root+"/operation-manual-stop.json",client.Operation("manual-stop").body);
                    Require(sent,"Alarm did not occur inside manual Stop");
                    rig.w.c[0].onAction=[&](const std::string &s) { rig.w.Record("C0."+s); };
                } else rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                const auto began=std::chrono::steady_clock::now(); Read(direct.Alarms()); Read(direct.Status());
                Require(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(500),"Alarm cache/status blocked on hardware");
                JsonValue alarm;
                if (alarmEnabled) Wait([&] {
                    alarm=Read(direct.Alarms()); const auto phase=IO::String(alarm.At("phase"));
                    return phase=="complete" || phase=="failed" || phase=="unknown" || phase=="held-no-run";
                },30000);
                else { alarm=Read(direct.Alarms()); Require(!IO::Boolean(alarm.At("enabled")) && !IO::Boolean(alarm.At("latched")),"Disabled alarm policy triggered hold"); }
                IO::WriteNew(root+"/alarm-status.json",direct.Alarms().body);
                const auto automatic=IO::String(alarm.At("operation_id"));
                const bool noRun=alarmReady || scenario=="alarm-during-stop";
                if (alarmEnabled) {
                    Require(IO::Boolean(alarm.At("latched")) && IO::Boolean(alarm.At("observation").At("stop_requested")),"Observed alarm/hold missing");
                    if (noRun) Require(automatic.empty() && IO::String(alarm.At("phase"))=="held-no-run","Known Ready alarm sent/repeated Stop");
                    else {
                        Require(automatic.compare(0,15,"internal-alarm-")==0,"Automatic stop has no reserved original ID");
                        const auto operation=Terminal(direct,automatic); const auto expected=scenario=="alarm-stop-loss" ? "failed" : "complete";
                        Require(IO::String(operation.At("phase"))==expected,"Automatic alarm stop result differs");
                        IO::WriteNew(root+"/operation-automatic-stop.json",direct.Operation(automatic).body);
                    }
                    const auto status=Read(direct.Status());
                    Require(IO::Boolean(status.At("control_needs_reconciliation")) && IO::Unsigned(status.At("controller").At("state"))==unsigned(scenario=="alarm-stop-loss" ? EccState::Running : EccState::Ready) &&
                        IO::Boolean(status.At("controller").At("outcome_unknown"))==(scenario=="alarm-stop-loss"),"Alarm hold/parent state differs");
                    Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"start","alarm-restart-run"))).code==409,"Alarm hold allowed production restart");
                    if (scenario!="alarm-paused" && !alarmReady) for (const auto &id:{"C0","C1"})
                        Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count(std::string(id)+".ctrl.pipeCtrl.intEnable=0")==1,"Automatic stop repeated/skipped independent producer");
                    if (scenario=="alarm-stop-loss") Require(!rig.Has("C0.sender-stop") && !rig.Has("C1.sender-stop"),"Unconfirmed producer stop flushed sender");
                } else { State(client,EccState::Running); Require(rig.Actions().empty(),"Disabled alarm policy sent hardware writes"); }
                const auto stopped=DecodeEccControlSystemStatus(direct.Status().body),before=DecodeEccControlSystemStatus(IO::ReadFile(root+"/before-alarm-status.json",1024*1024));
                for (std::size_t i=0;i<stopped.nodes.size();++i) {
                    const auto &a=stopped.nodes[i],&b=before.nodes[i];
                    Require(a.state=="ready" && a.sessionId==b.sessionId && a.receivers.size()==1 && a.receivers[0].pid==b.receivers[0].pid && !a.receivers[0].reaped && !a.receivers[0].sealVerified,
                        "Alarm stop disconnected/stopped/replaced receiver");
                    Require(services[i]->Commands("start")==1 && services[i]->Commands("finish")==0,"Alarm policy changed receiver lifecycle");
                }
                for (const auto &entry:raw) Require(entry.second->Connected() && entry.second->Expected().size()==std::size_t(alarmReady ? 0 : 2*4096),"Alarm stop changed raw connection/expected synthetic payload");
                if (scenario=="alarm-disabled") execute("stop","manual-stop",EccState::Ready);
                rig.Clear(); rig.devices[0].at("ctrl")->failFirmwareStop=false;
                rig.w.alarms[0]->sendAsadAlarm("CoBo0",8,Ice::Current()); rig.w.alarms[1]->sendAsadAlarm("CoBo1",1,Ice::Current());
                std::this_thread::sleep_for(std::chrono::milliseconds(150));
                Require(rig.Actions().empty(),"Repeated/second-board alarm replayed producer Stop");
                if (scenario=="alarm-restart") {
                    process->CrashAndRestart(); const auto reopened=Read(direct.Alarms());
                    Require(IO::String(reopened.At("operation_id"))==automatic && IO::String(reopened.At("phase"))=="reopened-held" &&
                        IO::Boolean(reopened.At("observation").At("stop_requested")),"Reopen lost original alarm hold/evidence");
                    Require(IO::String(Read(direct.Operation(automatic)).At("phase"))=="complete" && rig.Actions().empty(),"Reopen replayed automatic stop");
                    const auto next=Read(direct.Identity()); Require(direct.PostOperation(EncodeEccControlRequest(Request(next,"start","after-alarm-reopen"))).code==409,"Reopen cleared alarm/startup gate");
                }
                std::map<std::string,unsigned int> accepted; unsigned int holds=0; std::ifstream log(root+"/controller/network/operations.jsonl"); std::string row;
                while (std::getline(log,row)) { const auto event=ParseStrictJson(row); const auto name=IO::String(event.At("event"));
                    if (name=="alarm-held") ++holds;
                    if (name=="accepted") ++accepted[DecodeEccControlRequest(IO::String(event.At("request"))).command];
                }
                Require(holds==unsigned(alarmEnabled) && accepted["stop"]==unsigned(!alarmReady),"Alarm stop/hold accepted more than once");
                if(alarmRelease) {
                    const auto original=DecodeEccControlSystemStatus(direct.Status().body).controller.operationId;
                    auto request=Request(identity,"release-stopped-alarm","wrong-stopped-alarm");request.failedOperationId=original;request.alarmOperationId="internal-alarm-unrelated";
                    const auto writes=rig.w.Writes();
                    Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Wrong stopped alarm request not recorded");
                    Require(IO::String(Terminal(client,request.operationId).At("phase"))=="failed" && rig.w.Writes()==writes,"Wrong stopped alarm touched hardware");
                    request.operationId="release-stopped-alarm";request.alarmOperationId=automatic;
                    if(alarmReleasePowerLoss)rig.devices[0].at("ctrl")->failPowerOff=true;
                    Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Stopped alarm release not accepted");
                    const auto released=Terminal(client,request.operationId);
                    IO::WriteNew(root+"/operation-stopped-alarm-release.json",client.Operation(request.operationId).body);
                    const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                    if(alarmReleasePowerLoss) {
                        Require(IO::String(released.At("phase"))=="failed" && after.controller.outcomeUnknown && after.controlNeedsReconciliation &&
                            DecodeEccControlAlarmStatus(direct.Alarms().body).latched,"Failed power-off cleared original alarm gate");
                        rig.devices[0].at("ctrl")->failPowerOff=false;rig.Clear();request.operationId="repeat-stopped-alarm";
                        Require(client.PostOperation(EncodeEccControlRequest(request)).code==202,"Repeated release intent missing");
                        Require(IO::String(Terminal(client,request.operationId).At("phase"))=="failed" && rig.Actions().empty(),"Another ID replayed partial alarm release");
                    } else {
                        Require(IO::String(released.At("phase"))=="complete" && after.controller.state==2 && !after.controller.outcomeUnknown &&
                            !after.controlNeedsReconciliation && !after.dataNeedsReconciliation && after.controller.operationId==request.operationId,
                            "Stopped alarm release did not establish known Described");
                        const auto policy=DecodeEccControlAlarmStatus(direct.Alarms().body);
                        Require(!policy.latched && policy.phase=="watching" && policy.operationId.empty(),"Stopped alarm policy not rearmed");
                    }
                    for(std::size_t i=0;i<services.size();++i) {
                        const auto &node=after.nodes[i];
                        Require(services[i]->Commands("start")==1 && services[i]->Commands("finish")==1 && !services[i]->Commands("abort-failed-session") &&
                            node.sessionId==before.nodes[i].sessionId && node.receivers[0].pid==before.nodes[i].receivers[0].pid && node.receivers[0].reaped && node.receivers[0].sealVerified,
                            "Alarm release failed normal drain/seal or changed original receiver identity");
                    }
                    IO::WriteNew(root+"/after-stopped-alarm-release.json",direct.Status().body);
                    IO::WriteNew(root+"/after-stopped-alarm-policy.json",direct.Alarms().body);
                    if(!alarmReleasePowerLoss) {
                        for(unsigned int i=0;i<2;++i)Require(rig.w.c[i].powered==0 && rig.w.alarms[i]->unsubscribes==1,"Alarm release did not power off/close owned subscription");
                        execute("prepare","fresh-prepare",EccState::Prepared);execute("configure","fresh-configure",EccState::Ready);execute("start","fresh-start",EccState::Running);
                        rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                        Wait([&]{return IO::String(Read(direct.Alarms()).At("phase"))=="complete";},30000);
                        const auto nextAlarm=DecodeEccControlAlarmStatus(direct.Alarms().body);
                        Require(nextAlarm.latched && !nextAlarm.operationId.empty() && nextAlarm.operationId!=automatic &&
                            DecodeEccControlSystemStatus(direct.Status().body).controller.state==4,"New alarm did not stop fresh Run");
                        IO::WriteNew(root+"/second-stopped-alarm-policy.json",direct.Alarms().body);
                        rig.Clear();process->CrashAndRestart();const auto reopened=Read(direct.Identity());
                        auto cold=Request(reopened,"release-stopped-alarm","cold-stopped-alarm");cold.failedOperationId=nextAlarm.operationId;cold.alarmOperationId=nextAlarm.operationId;
                        Require(direct.PostOperation(EncodeEccControlRequest(cold)).code==202,"Cold release intent missing");
                        Require(IO::String(Terminal(direct,cold.operationId).At("phase"))=="failed" && rig.Actions().empty(),"Cold release adopted stopped objects");
                        IO::WriteNew(root+"/cold-stopped-alarm-denied.json",direct.Operation(cold.operationId).body);
                    }
                    IO::WriteNew(root+"/stopped-alarm-release-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                        ",\"original_stopped_operation_id\":"+EncodeJsonString(original)+",\"original_alarm_id\":"+EncodeJsonString(automatic)+
                        ",\"power_off_ack_loss\":"+(alarmReleasePowerLoss ? "true" : "false")+",\"normal_receiver_finish_verified\":true,\"original_run_complete\":false}\n");
                }

            } else if (preRun) {
                execute("describe","describe",EccState::Described); execute("prepare","prepare",EccState::Prepared);
                bool auditMoved=false;
                if (!preRunConfigured) raw.at("C1")->onConnected=[&] {
                    const auto file=root+"/controller/operations/configure/operation.jsonl";
                    Require(!::rename(file.c_str(),(file+".original").c_str()),"Move parent Configure audit after final raw connection");
                    IO::WriteNew(file,"parent audit replaced after ConnectData\n"); auditMoved=true;
                };
                rig.Clear(); submit("configure","configure");
                Require(IO::String(Terminal(client,"configure").At("phase"))=="failed","Injected pre-run Configure did not fail");
                Require(preRunConfigured ? alarmInjected.load() : auditMoved,"Wrong pre-run fault boundary");
                IO::WriteNew(root+"/operation-configure.json",client.Operation("configure").body);
                IO::WriteNew(root+"/before-stop-status.json",direct.Status().body);
                const auto before=DecodeEccControlSystemStatus(direct.Status().body);
                Require(before.controller.state==3 && before.controller.outcomeUnknown && before.controlNeedsReconciliation && before.nodes.size()==2,"Pre-run fault parent gate differs");
                for (const auto &entry:raw) Require(entry.second->Connected()==!preRunConfigured && entry.second->Expected().empty(),"Raw connection/data boundary differs");
                rig.Clear(); submit("stop-production-after-failure","negative-pre-run");
                Require(IO::String(Terminal(client,"negative-pre-run").At("phase"))=="complete","Pre-run network negative stop failed");
                IO::WriteNew(root+"/operation-negative-pre-run.json",client.Operation("negative-pre-run").body);
                for (const auto &id:{"C0","C1"}) {
                    Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count(std::string(id)+".ctrl.pipeCtrl.intEnable=0")==1 &&
                        rig.Count(std::string(id)+".pulser-stop")==1,"Pre-run network independent stop coverage differs");
                    for (const auto &action:{"connect","start","stop","disconnect"}) Require(!rig.Has(std::string(id)+".sender-"+action),"Pre-run negative stop touched sender connection");
                }
                rig.Clear(); submit("stop-production-after-failure","negative-pre-run");
                submit("stop-production-after-failure","negative-pre-run-again");
                Require(IO::String(Terminal(client,"negative-pre-run-again").At("phase"))=="complete" && rig.Actions().empty(),"Repeated pre-run negative stop changed hardware");
                IO::WriteNew(root+"/operation-negative-pre-run-again.json",client.Operation("negative-pre-run-again").body);
                Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"start","unverified-start"))).code==409,"Pre-run stop cleared positive gate");
                const auto after=DecodeEccControlSystemStatus(direct.Status().body);
                Require(after.controller.state==3 && after.controller.outcomeUnknown && after.controlNeedsReconciliation,"Pre-run negative stop committed Ready or cleared UNKNOWN");
                for (std::size_t i=0;i<after.nodes.size();++i) {
                    const auto &a=after.nodes[i],&b=before.nodes[i];
                    Require(a.state=="ready" && a.sessionId==b.sessionId && a.receivers.size()==1 && b.receivers.size()==1 &&
                        a.receivers[0].pid==b.receivers[0].pid && !a.receivers[0].reaped && !a.receivers[0].sealVerified && a.receivers[0].frames==0,
                        "Pre-run negative stop replaced/stopped/sealed receiver");
                    Require(services[i]->Commands("start")==1 && services[i]->Commands("finish")==0,"Pre-run negative stop changed data lifecycle");
                }
                for (const auto &entry:raw) Require(entry.second->Connected()==!preRunConfigured && entry.second->Expected().empty(),"Negative stop closed retained raw socket or sent payload");
            } else if (preparationFailure) {
                execute("describe","describe",EccState::Described);
                bool sent=false;
                rig.w.c[1].onAction=[&](const std::string &s) {
                    rig.w.Record("C1."+s);
                    if (!sent) { sent=true; rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); }
                };
                submit("prepare","prepare"); const auto failed=Terminal(client,"prepare");
                Require(IO::String(failed.At("phase"))=="failed","Injected partial Prepare did not fail");
                IO::WriteNew(root+"/operation-prepare.json",client.Operation("prepare").body);
                rig.w.c[1].onAction=[&](const std::string &s) { rig.w.Record("C1."+s); };
                const auto gate=[&] {
                    const auto status=Read(client.Status()); const auto &controller=status.At("controller");
                    Require(IO::Boolean(controller.At("outcome_unknown")) && IO::Unsigned(controller.At("state"))==unsigned(EccState::Described) &&
                        IO::Boolean(status.At("control_needs_reconciliation")) && !IO::Boolean(status.At("operation_active")),"Preparation stop cleared UNKNOWN or committed another state");
                    for (const auto &service:services) {
                        const auto lifecycle=DecodeSupervisorStatus(service->Status().At("lifecycle"));
                        Require(lifecycle.state==DataSupervisorStatus::Fresh && lifecycle.receivers.empty() && service->Commands("start")==0,
                            "Prepare failure/negative stop launched a receiver");
                    }
                };
                if(preparationCleanup) {
                    submit("unprepare-after-failure","missing-stop-cleanup");Require(IO::String(Terminal(client,"missing-stop-cleanup").At("phase"))=="failed","Cleanup admitted without original stop proof");
                }
                gate(); const bool loss=scenario=="prepare-stop-loss";
                if (loss) rig.devices[0].at("ctrl")->failFirmwareStop=true;
                rig.Clear(); submit("stop-production-after-failure","negative-prepare");
                const auto stopped=Terminal(client,"negative-prepare");
                Require(IO::String(stopped.At("phase"))==(loss ? "failed" : "complete"),"Network preparation negative-stop result differs");
                IO::WriteNew(root+"/operation-negative-prepare.json",client.Operation("negative-prepare").body);
                for (unsigned int i=0;i<2;++i) {
                    const auto id="C"+std::to_string(i);
                    Require(rig.Count(id+".ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count(id+".ctrl.pipeCtrl.intEnable=0")==1 &&
                        rig.Count(id+".pulser-stop")==1 && !rig.w.alarms[i]->unsubscribes,"Network negative stop skipped independent targets or closed subscriptions");
                    for (const char *action:{"connect","start","stop","disconnect"}) Require(!rig.Has(id+".sender-"+action),"Preparation stop touched raw sender");
                }
                if(preparationCleanup) {
                    const bool powerLoss=scenario=="prepare-cleanup-power-loss";rig.devices[0].at("ctrl")->failPowerOff=powerLoss;
                    submit("unprepare-after-failure","failure-cleanup");const auto cleaned=Terminal(client,"failure-cleanup");
                    Require(IO::String(cleaned.At("phase"))==(powerLoss ? "failed" : "complete"),"Cleanup outcome differs");gate();
                    for(unsigned int board=0;board<2;++board)Require(rig.w.c[board].powered==0,"Cleanup skipped independent power-off");
                    Require(rig.w.alarms[0]->unsubscribes==(powerLoss ? 0 : 1),"Cleanup subscription outcome differs");
                    rig.devices[0].at("ctrl")->failPowerOff=false;const auto before=rig.w.Writes();
                    submit("unprepare-after-failure","cleanup-again");Require(IO::String(Terminal(client,"cleanup-again").At("phase"))=="failed" && rig.w.Writes()==before,"Another cleanup ID repeated uncertain steps");
                    IO::WriteNew(root+"/failure-cleanup-result.json","{\"passed\":true,\"unknown_preserved\":true,\"no_receivers_started\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+"}");
                    if(preparationRecovery) {
                        const bool powerChanged=scenario=="prepare-cleanup-recovery-power",auditChanged=scenario=="prepare-cleanup-recovery-audit";
                        if(powerChanged)rig.w.c[0].powered=1;
                        if(auditChanged){std::ofstream changed(root+"/controller/operations/failure-cleanup/failure-unprepare/step-0.jsonl",std::ios::app);changed<<"{}\n";}
                        const auto writes=rig.w.Writes();
                        const auto recoveryId=qtPreparationRecovery ? "qt-preparation-recovery" : "prepare-recovery";
                        auto recovery=Request(identity,"reconcile-preparation",recoveryId);recovery.failedOperationId="prepare";
                        if(qtPreparationRecovery) {
                            IO::WriteNew(root+"/client-options.json","{\"port\":"+std::to_string(port)+",\"credential\":"+EncodeJsonString(credential)+",\"service_id\":"+EncodeJsonString(IO::String(identity.At("service_id")))+",\"client_id\":"+EncodeJsonString(IO::String(identity.At("client_id")))+",\"client_epoch\":"+EncodeJsonString(std::string(64,'a'))+",\"snapshot_sha256\":"+EncodeJsonString(IO::String(identity.At("snapshot_sha256")))+"}");
                            std::vector<std::string> words={argv[9],root+"/client-options.json",root+"/qt-preparation-result.json"};std::vector<char *> arguments;
                            for(auto &word:words)arguments.push_back(&word[0]);arguments.push_back(nullptr);pid_t probe=0;
                            Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),environ),"Spawn production preparation client probe");
                            int status=0;Require(::waitpid(probe,&status,0)==probe && WIFEXITED(status) && WEXITSTATUS(status)==0,"Preparation client probe failed");
                        }

                        if(!qtPreparationRecovery)Require(client.PostOperation(EncodeEccControlRequest(recovery)).code==202,"Preparation recovery request not recorded");
                        const auto recovered=Terminal(client,recoveryId);
                        Require(IO::String(recovered.At("phase"))==((powerChanged || auditChanged) ? "failed" : "complete"),"Preparation recovery terminal differs");
                        Require(rig.w.Writes()==writes,"Preparation recovery wrote hardware");
                        if(powerChanged || auditChanged)gate();
                        else {
                            const auto state=DecodeEccControlSystemStatus(direct.Status().body);
                            Require(state.controller.state==2 && !state.controller.outcomeUnknown && !state.controlNeedsReconciliation,"Preparation recovery did not clear only original hold");
                            for(const auto &node:state.nodes)Require(node.receivers.empty(),"Recovery created receiver");
                            execute("prepare","fresh-prepare",EccState::Prepared);
                        }
                        IO::WriteNew(root+"/preparation-recovery-result.json","{\"passed\":true,\"recovery_succeeded\":"+std::string(powerChanged || auditChanged ? "false" : "true")+",\"no_recovery_hardware_writes\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+"}");
                    }
                } else {
                gate(); rig.devices[0].at("ctrl")->failFirmwareStop=false; rig.Clear();
                Require(submit("stop-production-after-failure","negative-prepare").code==202,"Duplicate negative stop rejected");
                submit("stop-production-after-failure","negative-prepare-again");
                Require(IO::String(Terminal(client,"negative-prepare-again").At("phase"))==(loss ? "failed" : "complete"),"Earlier unconfirmed stop was fabricated successful after restoration");
                IO::WriteNew(root+"/operation-negative-prepare-again.json",client.Operation("negative-prepare-again").body);
                Require(rig.Actions().empty(),"Duplicate/new-ID negative stop replayed old production steps");
                Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"prepare","retry-prepare"))).code==409,"Negative stop cleared positive command gate");
                gate();
                std::map<std::string,unsigned int> accepted; std::ifstream log(root+"/controller/network/operations.jsonl"); std::string row;
                while (std::getline(log,row)) { const auto event=ParseStrictJson(row); if (IO::String(event.At("event"))=="accepted") ++accepted[DecodeEccControlRequest(IO::String(event.At("request"))).command]; }
                Require(accepted==std::map<std::string,unsigned int>{{"describe",1},{"prepare",1},{"stop-production-after-failure",2}},"Preparation failure duplicated accepted original request");
                }
            } else if (qt) {
                std::string nodes="[";
                for (std::size_t i=0;i<plans.size();++i) {
                    if (nodes.back()!='[') nodes+=',';
                    nodes+="{\"id\":"+std::to_string(i)+",\"role\":\"cobo\",\"host\":\"127.0.0.1\",\"native_sender_id\":"+EncodeJsonString(plans[i].sources[0].id)+",\"native_agent_id\":"+EncodeJsonString(remote.endpoints[i].agentId)+"}";
                }
                nodes+=']';
                const auto gui="{\"experiment\":\"NativeIntegration\",\"run_number\":0,\"getdaq\":{\"simulation\":false,\"native_configuration_status\":true,\"native_alarm_status\":"+((qtAlarm || qtAlarmAcquisitionRecovery) ? std::string("true") : std::string("false"))+
                    ",\"native_receiver_failure_status\":"+(qtReceiverFailure && !qtSourceRecovery ? std::string("true") : std::string("false"))+",\"native_source_status\":"+(qtSourceRecovery ? std::string("true") : std::string("false"))+",\"native_catalog\":"+(catalog ? std::string("true") : std::string("false"))+",\"config_dir\":"+EncodeJsonString(root+"/unused-legacy-config")+",\"output_dir\":"+EncodeJsonString(root+"/gui-runs")+
                    ",\"poll_ms\":60,\"timeout_ms\":2000,\"step_timeout_ms\":240000,\"native_ecc\":{\"address\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"client_epoch\":"+EncodeJsonString(std::string(64,'a'))+
                    ",\"snapshot_sha256\":"+EncodeJsonString(IO::String(identity.At("snapshot_sha256")))+",\"input_binding_sha256\":"+EncodeJsonString(IO::String(identity.At("input_binding_sha256")))+",\"timeout_ms\":2000,\"status_max_age_ms\":10000,\"record_dir\":"+EncodeJsonString(root+"/gui-client-records")+"},\"nodes\":"+nodes+"}}\n";
                const auto file=root+"/gui.json"; IO::WriteNew(file,gui); IO::Fd log(::open((root+"/gui-probe.log").c_str(),O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600)); Require(log.value>=0,"Actual GUI probe log");
                posix_spawn_file_actions_t actions; Require(!::posix_spawn_file_actions_init(&actions),"GUI probe spawn actions");
                ::posix_spawn_file_actions_adddup2(&actions,log.value,STDOUT_FILENO); ::posix_spawn_file_actions_adddup2(&actions,log.value,STDERR_FILENO);
                std::vector<std::string> words={argv[9],file}; if (catalogFailure) words.push_back("--expect-catalog-failure");
                if (qtAlarm) words.push_back("--expect-alarm");
                if (qtReceiverFailure) words.push_back(qtSourceRecovery ? "--expect-source-cleanup" : qtWindowCleanup ? "--expect-receiver-cleanup" : "--expect-receiver-failure");
                std::vector<char *> args; for (auto &word:words) args.push_back(&word[0]); args.push_back(nullptr);
                pid_t pid=0; const auto spawned=::posix_spawn(&pid,argv[9],&actions,nullptr,args.data(),environ); ::posix_spawn_file_actions_destroy(&actions); Require(!spawned && pid>0,"Spawn actual Qt RunControl probe");
                int status=0;
                auto captured=std::chrono::steady_clock::now();
                bool guiAlarmSent=false; bool guiReceiverFailed=false;
                try { Wait([&] {
                    const auto result=::waitpid(pid,&status,WNOHANG); Require(result>=0,"Observe actual Qt probe");
                    struct stat marker;
#ifdef GETDAQ_TEST_ICE_SOURCE
                    if(qtSourceRecovery && !guiReceiverFailed && !::lstat((root+"/gui-runs/hardware/NativeIntegration/run_0000/source-failure-probe-ready").c_str(),&marker)) {
                        Wait([&]{for(const auto &entry:iceProducers)if(entry.second->read[0]!=entry.second->write[0] || entry.second->read[1]!=entry.second->write[1])return false;return true;},5000);
                        IO::WriteNew(root+"/gui-before-receiver-failure.json",direct.Status().body);
                        iceSources.begin()->second->ReportFailure("Synthetic asynchronous GUI ICE source fault");guiReceiverFailed=true;
                    }
#endif
                    if (qtReceiverFailure && !qtSourceRecovery && !guiReceiverFailed && !::lstat((root+"/gui-runs/hardware/NativeIntegration/run_0000/receiver-failure-probe-ready").c_str(),&marker)) {
                        const auto before=DecodeEccControlSystemStatus(direct.Status().body);
                        IO::WriteNew(root+"/gui-before-receiver-failure.json",direct.Status().body);
                        for (auto &service:services) service->Capture();rig.Clear();guiReceiverFailed=true;
                        Require(!::kill(before.nodes[0].receivers[0].pid,SIGKILL),"Kill fixture-owned receiver after actual GUI Running");
                    }
                    if(qtAlarmAcquisitionRecovery && guiReceiverFailed && !guiAlarmSent &&
                       IO::String(Read(direct.ReceiverFailure()).At("phase"))=="complete") {
                        IO::WriteNew(root+"/gui-before-alarm-status.json",direct.Status().body);guiAlarmSent=true;
                        rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                    }
                    if (qtAlarm && !guiAlarmSent && !::lstat((root+"/gui-runs/hardware/NativeIntegration/run_0000/alarm-probe-ready").c_str(),&marker)) {
                        IO::WriteNew(root+"/gui-before-alarm-status.json",direct.Status().body); guiAlarmSent=true; rig.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current());
                    }
                    if (std::chrono::steady_clock::now()-captured>=std::chrono::seconds(1)) { for (auto &service:services) service->Capture(); captured=std::chrono::steady_clock::now(); }
                    return result==pid;
                },510000); }
                catch (...) { ::kill(pid,SIGKILL); ::waitpid(pid,nullptr,0); throw; }
                Require(WIFEXITED(status) && !WEXITSTATUS(status),"Actual Qt full GUI workflow failed; inspect gui-probe.log");
                if (qtReceiverFailure) {
                    Require(guiReceiverFailed,"Actual GUI receiver fault not injected at Running");
                    const auto policy=DecodeEccControlReceiverFailureStatus(direct.ReceiverFailure().body);
                    if(!qtSourceRecovery)Require(qtAcquisitionRecovery ? !policy.latched && policy.phase=="watching" : policy.latched && policy.phase=="complete" && policy.failedIds.size()==1,"Actual GUI receiver policy phase differs");
                    const auto after=DecodeEccControlSystemStatus(direct.Status().body),before=DecodeEccControlSystemStatus(IO::ReadFile(root+"/gui-before-receiver-failure.json",1024*1024));
                    Require(qtAcquisitionRecovery ? !after.controller.outcomeUnknown && !after.controlNeedsReconciliation && after.controller.state==3 : after.controller.outcomeUnknown && after.controlNeedsReconciliation && after.controller.state==5 && after.controller.operationId==before.controller.operationId,"Actual GUI receiver recovery/fault state differs");
                    for (std::size_t i=0;i<after.nodes.size();++i) Require(after.nodes[i].sessionId==before.nodes[i].sessionId && after.nodes[i].receivers[0].pid==before.nodes[i].receivers[0].pid && after.nodes[i].receivers[0].reaped==qtWindowCleanup,"Actual GUI receiver lifecycle replaced/finished");
                    if(!qtAcquisitionRecovery)for (const auto &id:{"C0","C1"}) Require(rig.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 && rig.Count(std::string(id)+".sender-stop")==unsigned(qtFailureBreakup) && rig.Count(std::string(id)+".sender-disconnect")==unsigned(qtFailureBreakup),"Actual GUI receiver fault skipped/replayed producer or sender cleanup");
                    IO::WriteNew(root+"/receiver-failure-policy.json",direct.ReceiverFailure().body);
                    if(!qtAcquisitionRecovery)IO::WriteNew(root+"/operation-receiver-stop.json",direct.Operation(policy.operationId).body);
                }
                if (qtAlarm) {
                    Require(guiAlarmSent,"Actual GUI alarm was not triggered at Running");
                    const auto final=DecodeEccControlSystemStatus(direct.Status().body),before=DecodeEccControlSystemStatus(IO::ReadFile(root+"/gui-before-alarm-status.json",1024*1024));
                    Require(qtStoppedAlarmRelease ? final.controller.state==3 && !final.controlNeedsReconciliation && !final.controller.outcomeUnknown :
                        final.controller.state==4 && final.controlNeedsReconciliation && !final.controller.outcomeUnknown,"Actual GUI alarm release/hold state differs");
                    for (std::size_t i=0;i<final.nodes.size();++i) Require(final.nodes[i].state==(qtStoppedAlarmRelease ? "stopped" : "ready") && final.nodes[i].sessionId==before.nodes[i].sessionId &&
                        final.nodes[i].receivers[0].pid==before.nodes[i].receivers[0].pid && final.nodes[i].receivers[0].reaped==qtStoppedAlarmRelease &&
                        (!qtStoppedAlarmRelease || final.nodes[i].receivers[0].sealVerified),"Actual GUI alarm receiver identity/seal differs");
                } else if (!catalogFailure && !qtReceiverFailure) State(direct,EccState::Idle);
                else if (catalogFailure) {
                    const auto stopped=Read(direct.Status());
                    Require(IO::Unsigned(stopped.At("controller").At("state"))==unsigned(EccState::Prepared) && IO::Boolean(stopped.At("control_needs_reconciliation")) &&
                        IO::Boolean(stopped.At("data").At("needs_reconciliation")) && !IO::Boolean(stopped.At("operation_active")),"Catalog failure must retain Prepared plus global/data reconciliation gates");
                }
                if (!qtAlarm && !qtReceiverFailure) CheckFiles(Read(direct.Status()),raw,4,catalogFailure);
                std::map<std::string,unsigned int> accepted; std::ifstream journal(root+"/controller/network/operations.jsonl"); std::string row;
                JsonLimits guiJournalLimits;guiJournalLimits.stringBytes=1024*1024;
                while (std::getline(journal,row)) { const auto event=ParseStrictJson(row,guiJournalLimits); if (IO::String(event.At("event"))=="accepted") ++accepted[DecodeEccControlRequest(IO::String(event.At("request"))).command]; }
                std::map<std::string,unsigned int> expected={{"describe",1},{"prepare",1},{"configure",1},{"start",1},{"stop",1},{"pause",1},{"resume",1},{"breakup",1},{"reset",2}};
                if (catalog) expected["catalog"]=1;
                if (catalogFailure) expected.erase("reset");
                if (qtAlarm) expected={{"describe",1},{"prepare",1},{"configure",1},{"start",1},{"stop",1}};
                if(qtStoppedAlarmRelease){expected["prepare"]=2;expected["release-stopped-alarm"]=1;}
                if (qtReceiverFailure) expected={{"describe",1},{"prepare",1},{"configure",1},{"start",1},{"stop-production-after-failure",1}};
                if(qtWindowCleanup) expected["abort-failed-receivers"]=1;
                if(qtFailureBreakup)expected["breakup-after-failure"]=1;
                if(qtReceiverRetirement)expected["retire-failed-receivers"]=1;
                if(qtAcquisitionUnprepare)expected["unprepare-acquisition-after-failure"]=1;
                if(qtAcquisitionRecovery){expected["reconcile-acquisition"]=1;expected["prepare"]=2;}
                if(qtAlarmAcquisitionRecovery){expected.erase("reconcile-acquisition");expected["reconcile-acquisition-with-alarm"]=1;expected["stop-production-after-failure"]=2;}
                if(qtSourceRecovery){expected.erase("reconcile-acquisition");expected["reconcile-acquisition-with-sources"]=1;expected["retire-failed-sources"]=1;}
                Require(accepted==expected,"GUI duplicated whole-system commands per board");
                if(catalogRecovery) {
                    execute("describe","catalog-recovery-describe",EccState::Described);
                    execute("prepare","catalog-recovery-prepare",EccState::Prepared);
                    execute("configure","catalog-recovery-configure",EccState::Ready);
                    execute("start","catalog-recovery-start",EccState::Running);
                    execute("stop","catalog-recovery-stop",EccState::Ready);
                    execute("breakup","catalog-recovery-breakup",EccState::Prepared);
                    CheckFiles(Read(direct.Status()),raw,2);
                    const auto before=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(before.nodes.size()==2,"Catalog recovery fixture lost a fixed data node");
                    const auto guiCatalogs=DecodeEccControlSystemStatus(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/data-catalogs.json",1024*1024));
                    Require(guiCatalogs.nodes.size()==plans.size(),"Original GUI catalog receipt lost a fixed data node before replacement");
                    for(std::size_t i=0;i<plans.size();++i) {
                        const auto official=plans[i].catalogRoot+"/NativeIntegration/run_0000/"+plans[i].deploymentId;
                        const auto oldReceipt=DecodeEccControlCatalogReceipt(InspectReceiverCatalog(official));
                        const auto oldRequest=DecodeReceiverCatalogRequest(IO::ReadFile(official+"/request.json",1024*1024));
                        Require(!oldReceipt.requestKey.empty() && !oldReceipt.manifestSha256.empty() && oldRequest.experiment=="NativeIntegration" && oldRequest.runNumber==0 &&
                            oldRequest.receiver.deploymentId==plans[i].deploymentId,
                            "Catalog recovery fixture lacks an independently verified original Run catalog on a node");
                        const auto guiNode=std::find_if(guiCatalogs.nodes.begin(),guiCatalogs.nodes.end(),[&](const EccControlDataNodeStatus &node) {
                            return node.agentId=="node-"+std::to_string(i);
                        });
                        Require(guiNode!=guiCatalogs.nodes.end() && guiNode->catalogVerified &&
                            DecodeEccControlCatalogReceipt(InspectReceiverCatalog(guiNode->catalog.directory)).requestKey==guiNode->catalog.requestKey &&
                            guiNode->catalog.requestKey==oldReceipt.requestKey,
                            "Original GUI catalog receipt differs from the independently verified pre-replacement Run catalog");
                    }
                    auto replace=Request(Read(direct.Identity()),"catalog","catalog-recovery-original");
                    replace.catalogExperiment="NativeIntegration";replace.catalogRunNumber=0;replace.catalogReplaceExisting=true;
                    const auto replacementResponse=direct.PostOperation(EncodeEccControlRequest(replace));
                    Require(replacementResponse.code==202,("ECC did not accept the replacement catalog request: HTTP "+std::to_string(replacementResponse.code)+" "+replacementResponse.body).c_str());
                    const auto failed=Terminal(direct,replace.operationId);
                    Require(IO::String(failed.At("phase"))=="failed","Injected replacement Publish failure did not become a terminal failed operation");
                    Require(catalogRecoveryFaultInjected.load(),"Replacement Prepare did not arm the deterministic Publish failure");
                    Require(!blockedCatalogPath.empty(),"Catalog recovery fixture did not bind the exact replacement backup collision");
                    const auto held=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(held.controller.state==unsigned(EccState::Prepared) && held.controlNeedsReconciliation && held.dataNeedsReconciliation,
                        "Replacement Publish failure did not retain the ECC reconciliation gate");
                    struct stat blocker;Require(!::lstat(blockedCatalogPath.c_str(),&blocker) && S_ISREG(blocker.st_mode) && !::unlink(blockedCatalogPath.c_str()),
                        "Cannot remove the synthetic backup collision before explicit recovery");
                    blockedCatalogPath.clear();
                    auto resume=Request(Read(direct.Identity()),"resume-catalog-replacement","catalog-recovery-resume");resume.failedOperationId=replace.operationId;
                    const auto recoveryResponse=direct.PostOperation(EncodeEccControlRequest(resume));
                    Require(recoveryResponse.code==202,("ECC service rejected the exact retained replacement recovery: HTTP "+std::to_string(recoveryResponse.code)+" "+recoveryResponse.body).c_str());
                    const auto recovered=Terminal(direct,resume.operationId);
                    Require(IO::String(recovered.At("phase"))=="complete",("ECC service replacement recovery did not complete: "+IO::String(recovered.At("message"))).c_str());
                    const auto final=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(final.controller.state==unsigned(EccState::Prepared) && !final.controlNeedsReconciliation && !final.dataNeedsReconciliation &&
                        final.nodes.size()==2 && final.nodes[0].catalogVerified && final.nodes[1].catalogVerified,
                        "Successful ECC service recovery did not verify every catalog receipt and clear only its reconciliation gate");
                    for(const auto &node:final.nodes)
                        Require(DecodeEccControlCatalogReceipt(InspectReceiverCatalog(node.catalog.directory)).requestKey==node.catalog.requestKey,
                            "Recovered catalog receipt differs from the independently reopened node catalog");
                    Require(services[0]->Commands("catalog")==4 && services[1]->Commands("catalog")==3,
                        "Replacement recovery did not perform one Prepare, one failed initial Publish, and one Publish per fixed node");
                    IO::WriteNew(root+"/catalog-recovery-service-result.json","{\"passed\":true,\"original_failed_replacement_observed\":true,\"same_replacement_resumed\":true,\"all_node_receipts_verified\":true,\"control_reconciliation_cleared\":true,\"real_hardware\":false}\n");
                }
                if(qtStoppedAlarmRelease) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto ack=DecodeEccControlOperation(IO::String(saved.At("native_alarm_acknowledgement_operation")));
                    Require(ack.phase=="complete" && ack.request.command=="release-stopped-alarm" && ack.request.failedOperationId==ack.request.alarmOperationId &&
                        IO::String(saved.At("phase"))=="Interrupted" && IO::Boolean(saved.At("hardware_recovery_complete")) && IO::Boolean(saved.At("duration_incomplete")),
                        "Stopped alarm release lost interrupted Run/acknowledgement proof");
                    for(const auto &service:services)Require(service->Commands("start")==1 && service->Commands("finish")==1 && !service->Commands("abort-failed-session"),"Stopped alarm GUI replayed or aborted data lifecycle");
                    IO::WriteNew(root+"/stopped-alarm-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                        ",\"real_main_window_and_runcontrol\":true,\"original_run_interrupted\":true,\"normal_receiver_finish_verified\":true,\"fresh_prepare_verified\":true}\n");
                }

                if(qtFailureBreakup) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto operation=DecodeEccControlOperation(IO::String(saved.At("native_failure_breakup_operation")));
                    const auto held=DecodeEccControlControllerStatus(operation.resultJson);
                    Require(operation.phase=="complete" && operation.request.command=="breakup-after-failure" && held.outcomeUnknown &&
                        IO::Boolean(saved.At("duration_incomplete")),"GUI connection cleanup journal cleared/failed original Run");
                    IO::WriteNew(root+"/failure-breakup-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"cancel_requests\":0,\"confirmed_requests\":1,\"run_incomplete\":true,\"receiver_cleanup_subsequent\":true}");
                }
                if(qtReceiverRetirement) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto op=DecodeEccControlOperation(IO::String(saved.At("native_receiver_retirement_operation")));
                    const auto held=DecodeEccControlControllerStatus(op.resultJson);const auto current=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(op.phase=="complete" && op.request.command=="retire-failed-receivers" && held.outcomeUnknown && !current.dataNeedsReconciliation && (qtAcquisitionRecovery ? !current.controlNeedsReconciliation : current.controlNeedsReconciliation),"GUI retirement/recovery hold differs");
                    for(const auto &service:services)Require(service->Commands("retire-failed-session")==1 && service->Commands("start")==1 && service->Commands("finish")==0,"GUI retirement replayed/sealed/restarted data");
                    IO::WriteNew(root+"/receiver-retirement-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"cancel_requests\":0,\"confirmed_requests\":1,\"parent_unknown_preserved\":true,\"run_incomplete\":true}\n");
                }
                if(qtAcquisitionUnprepare) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto op=DecodeEccControlOperation(IO::String(saved.At("native_acquisition_unprepare_operation")));
                    Require(op.phase=="complete" && op.request.command=="unprepare-acquisition-after-failure" && DecodeEccControlControllerStatus(op.resultJson).outcomeUnknown,"GUI acquisition Unprepare not held/complete");
                    for(unsigned int i=0;i<2;++i)Require(rig.w.c[i].powered==(qtAcquisitionRecovery ? rig.options.cobos[i].prepare.settings.inputs.asadMask : 0) && rig.w.alarms[i]->unsubscribes==1,"GUI power-off/fresh Prepare owned closure missing");
                    IO::WriteNew(root+"/acquisition-unprepare-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"cancel_requests\":0,\"confirmed_requests\":1,\"parent_unknown_preserved\":true,\"run_incomplete\":true}\n");
                }
                if(qtAcquisitionRecovery) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto op=DecodeEccControlOperation(IO::String(saved.At("native_acquisition_recovery_operation")));
                    Require(op.phase=="complete" && op.request.command==(qtSourceRecovery ? "reconcile-acquisition-with-sources" : qtAlarmAcquisitionRecovery ? "reconcile-acquisition-with-alarm" : "reconcile-acquisition") &&
                        DecodeEccControlControllerStatus(op.resultJson).state==2 && IO::Boolean(saved.At("hardware_recovery_complete")) &&
                        IO::Boolean(saved.At("duration_incomplete")) && IO::String(saved.At("phase"))=="Interrupted",
                        "GUI acquisition recovery lost original interrupted Run proof");
                    IO::WriteNew(root+"/acquisition-recovery-window-result.json","{\"passed\":true,\"encoding_minor\":"+
                        std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"cancel_requests\":0,\"confirmed_requests\":1,\"run_incomplete\":true,\"fresh_prepare_verified\":true}\n");
                }
                if(qtSourceRecovery) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto retired=DecodeEccControlOperation(IO::String(saved.At("native_source_retirement_operation")));
                    const auto recovered=DecodeEccControlOperation(IO::String(saved.At("native_source_recovery_operation")));
                    Require(retired.phase=="complete" && retired.request.command=="retire-failed-sources" && DecodeEccControlControllerStatus(retired.resultJson).outcomeUnknown &&
                        recovered.phase=="complete" && recovered.request.command=="reconcile-acquisition-with-sources","GUI lost original source retirement/recovery receipts");
                    for(const auto &service:services)Require(service->Commands("start")==1 && service->Commands("finish")==0 && service->Commands("abort-failed-session")==1 && service->Commands("retire-failed-session")==1,"GUI source recovery repeated/finished original data lifecycle");
                    IO::WriteNew(root+"/source-recovery-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+",\"real_main_window_and_runcontrol\":true,\"original_run_interrupted\":true,\"old_data_unsealed\":true,\"fresh_prepare_verified\":true}\n");
                }
                if(qtAlarmAcquisitionRecovery) {
                    const auto saved=ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits);
                    const auto ack=DecodeEccControlOperation(IO::String(saved.At("native_alarm_acknowledgement_operation")));
                    Require(guiAlarmSent && ack.phase=="complete" && ack.request.command=="reconcile-acquisition-with-alarm" &&
                        !ack.request.alarmOperationId.empty() && !IO::Boolean(Read(direct.Alarms()).At("latched")),"GUI did not acknowledge the original alarm and rearm policy");
                    IO::WriteNew(root+"/alarm-acquisition-recovery-window-result.json","{\"passed\":true,\"encoding_minor\":"+std::to_string(rig.options.encodingMinor)+
                        ",\"real_main_window_and_runcontrol\":true,\"cancel_requests\":0,\"confirmed_requests\":1,\"run_incomplete\":true,\"fresh_prepare_verified\":true,\"original_alarm_acknowledged\":true}\n");
                }
                Require((ParseStrictJson(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/journal.json",1024*1024),guiJournalLimits).At("phase").text=="Complete")==(!catalogFailure && !qtAlarm && !qtReceiverFailure),"Actual GUI Run completion differs from catalog/alarm outcome");
                if (catalog && !catalogFailure && !catalogRecovery) {
                    const auto references=DecodeEccControlSystemStatus(IO::ReadFile(root+"/gui-runs/hardware/NativeIntegration/run_0000/data-catalogs.json",1024*1024));
                    Require(references.nodes.size()==2,"GUI catalog node coverage differs");
                    for (const auto &node:references.nodes) Require(node.catalogVerified && DecodeEccControlCatalogReceipt(InspectReceiverCatalog(node.catalog.directory)).requestKey==node.catalog.requestKey,"Actual GUI catalog record/file binding differs");
                }
                if (catalogFailure) {
                    const auto status=DecodeEccControlSystemStatus(direct.Status().body);
                    Require(status.controlNeedsReconciliation && status.dataNeedsReconciliation && status.nodes[0].catalogVerified && !status.nodes[1].catalogVerified,"Partial catalog lost independent result or cleared gate");
                    Require(IO::ReadFile(plans[1].catalogRoot+"/NativeIntegration/run_0000/"+plans[1].deploymentId+"/existing-marker.txt",1024)=="Preserve this existing dataset\n","Catalog overwrote prior dataset");
                    for (const auto &service:services) Require(service->Commands("catalog")==1,"Partial catalog replayed a data-node request");
                    std::string failedCatalogId;std::ifstream operations(root+"/controller/network/operations.jsonl");std::string operationRow;
                    while(std::getline(operations,operationRow)) {
                        const auto event=ParseStrictJson(operationRow,guiJournalLimits);
                        if(IO::String(event.At("event"))=="accepted") {
                            const auto request=DecodeEccControlRequest(IO::String(event.At("request")));
                            if(request.command=="catalog")failedCatalogId=request.operationId;
                        }
                    }
                    Require(!failedCatalogId.empty(),"Missing original failed catalog operation for negative recovery gate test");
                    auto invalidRecovery=Request(Read(direct.Identity()),"resume-catalog-replacement","reject-nonreplacement-catalog-recovery");
                    invalidRecovery.failedOperationId=failedCatalogId;
                    const auto recoveryResponse=direct.PostOperation(EncodeEccControlRequest(invalidRecovery));
                    IO::WriteNew(root+"/catalog-recovery-gate-test.json","{\"http_status\":"+std::to_string(recoveryResponse.code)+",\"failed_operation_id\":"+EncodeJsonString(failedCatalogId)+"}\n");
                    Require(recoveryResponse.code==409,
                            ("Catalog failure without explicit replacement authorization did not reject recovery: HTTP "+std::to_string(recoveryResponse.code)+" "+recoveryResponse.body).c_str());
                    for (const auto &service:services) Require(service->Commands("catalog")==1,"Rejected recovery sent another catalog command");
                }
            } else {
            execute("describe","describe",EccState::Described);
            rig.Clear(); submit("prepare","prepare"); Wait([&] { return IO::String(Read(client.Operation("prepare")).At("phase"))=="running"; });
            const auto began=std::chrono::steady_clock::now(); Read(client.Status()); Require(std::chrono::steady_clock::now()-began<std::chrono::milliseconds(500),"GET status blocked behind Prepare");
            Require(submit("prepare","prepare").code==202,"In-flight duplicate rejected/replayed");
            const auto writesBeforeConcurrentClient=rig.w.Writes();
            const auto concurrentClientRequest=Request(identity,"describe","concurrent-client-describe");
            Require(client.PostOperation(EncodeEccControlRequest(concurrentClientRequest)).code==409,
                "A second control client using the same owner epoch was queued while Prepare was active");
            Require(rig.w.Writes()==writesBeforeConcurrentClient,
                "Rejected concurrent control client caused an extra hardware command");
            auto other=Request(identity,"describe","other-owner"); other.clientEpoch=std::string(64,'b'); Require(client.PostOperation(EncodeEccControlRequest(other)).code==409,"Busy/takeover request accepted");
            if (scenario=="ecc-interrupted") {
                Wait([&] { std::lock_guard<std::mutex> lock(rig.w.logMutex); return !rig.w.actions.empty(); },15000);
                process->CrashAndRestart(); rig.Clear(); const auto reopened=Read(direct.Identity());
                Require(IO::String(reopened.At("instance_id"))!=IO::String(identity.At("instance_id")),"Interrupted ECC reused process instance");
                const auto unfinished=Read(direct.Operation("prepare")); Require(IO::String(unfinished.At("phase"))=="unknown","Interrupted Prepare was replayed/fabricated complete");
                const auto status=Read(direct.Status()); Require(IO::Boolean(status.At("control_needs_reconciliation")) && IO::Unsigned(status.At("controller").At("state"))==unsigned(EccState::Described),"Interrupted Prepare fabricated Prepared/cleared gate");
                Require(direct.PostOperation(EncodeEccControlRequest(Request(reopened,"prepare","repeat-prepare"))).code==409,"Reopened service retried unfinished Prepare");
                for (const auto &service:services) Require(DecodeSupervisorStatus(service->Status().At("lifecycle")).state==DataSupervisorStatus::Fresh,"Interrupted Prepare launched receiver");
                { std::lock_guard<std::mutex> lock(rig.w.logMutex); Require(rig.w.actions.empty(),"Interrupted ECC replayed a hardware command"); }
            } else {
            const auto prepared=Terminal(client,"prepare"); Require(IO::String(prepared.At("phase"))=="complete",("Network Prepare failed: "+IO::String(prepared.At("message"))).c_str()); State(client,EccState::Prepared);
            Require(client.PostOperation(EncodeEccControlRequest(other)).code==409,"Completed Prepare allowed another client epoch to take control");
            if(maintenanceWindow) {
#include "cobo_maintenance_network_case.h"
            }
            if (scenario=="request-fault") {
                const auto path=root+"/controller/network/operations/prepare/request.json",bytes=IO::ReadFile(path,8192);
                { std::fstream file(path,std::ios::in|std::ios::out|std::ios::binary); file.put('!'); file.flush(); Require(bool(file),"Inject completed request corruption"); }
                rig.Clear(); Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"configure","fault-configure"))).code==409,"Corrupted historical intent allowed new hardware action");
                { std::ofstream file(path,std::ios::binary|std::ios::trunc); file<<bytes; }
                Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"configure","restored-configure"))).code==409,"Restored intent cleared fault gate");
                Require(IO::Boolean(Read(client.Status()).At("control_needs_reconciliation")) && !rig.Has("C0.sender-connect"),"Intent fault gate/producer proof differs");
                { std::lock_guard<std::mutex> lock(rig.w.logMutex); Require(rig.w.actions.empty(),"Historical intent fault allowed hardware mutation"); }
            } else if (fdtEmpty || iceEmpty) {
                execute("configure","configure",EccState::Ready);for (auto &service:services) service->Capture();
                for (const auto &entry:raw) Require(!entry.second->Connected() && entry.second->Expected().empty(),"FDT Connect opened a data connection before Start");
                execute("breakup","breakup-unused",EccState::Prepared);CheckFiles(Read(client.Status()),raw,0,false,iceExpected);
                execute("reset","unprepare",EccState::Described);execute("reset","forget",EccState::Idle);
                for (const auto &service:services) Require(service->Commands("start")==1 && service->Commands("finish")==1,"Unused FDT source restarted/replayed receiver actions");
            } else {
                execute("configure","configure",EccState::Ready); const auto ready=Read(client.Status()); for (auto &service:services) service->Capture(); rig.Clear();
                const auto readyObservation=direct.Receivers(); const auto liveProcesses=DecodeEccControlReceiverObservation(readyObservation.body);
                Require(readyObservation.code==200 && liveProcesses.nodes.size()==2,"Live ECC receiver observation unavailable");
                for (const auto &node:liveProcesses.nodes) Require(node.available && !node.busy && node.receivers.size()==1 && node.receivers[0].state=="running","Live ECC process state differs");
                {std::lock_guard<std::mutex> lock(rig.w.logMutex);Require(rig.w.actions.empty(),"Receiver observation sent a hardware command");}
                IO::WriteNew(root+"/ready-receiver-observation.json",readyObservation.body);
                const auto sourceResponse=direct.Sources();Require(sourceResponse.code==200,"Source HTTP observation unavailable");
                const auto sourceObservation=DecodeEccControlSourceObservation(sourceResponse.body);
                Require(sourceObservation.instanceId==IO::String(identity.At("instance_id")) && sourceObservation.sources.size()==2 && !sourceObservation.busy,"Source HTTP identity/whole set differs");
                for(const auto &source:sourceObservation.sources)Require(source.phase==(ice ? "available" : "legacy") && (!ice || (source.state=="connected" && !source.sourceEndAcknowledged)),"Source HTTP phase/bound state differs");
                {std::lock_guard<std::mutex> lock(rig.w.logMutex);Require(rig.w.actions.empty(),"Source observation sent a production command");}
                IO::WriteNew(root+"/ready-source-observation.json",sourceResponse.body);
#ifdef GETDAQ_TEST_ICE_SOURCE
                if(observeSources) {
                    auto &source=*iceSources.begin()->second;source.ArmObservation();
                    auto pending=std::async(std::launch::async,[&] {return direct.Sources();});
                    try {
                        source.WaitForObservation();const auto started=std::chrono::steady_clock::now();
                        Require(direct.Status().code==200,"Slow source blocked ECC status");
                        const auto overlap=direct.Sources();Require(overlap.code==409,"Overlapping source read was queued/accepted");
                        Require(std::chrono::steady_clock::now()-started<std::chrono::milliseconds(400),"Slow source blocked status/overlap replies");
                        Require(pending.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready,"Source barrier no longer pending");
                    }catch(...) {source.ReleaseObservation();pending.wait();throw;}
                    source.ReleaseObservation();const auto response=pending.get();Require(response.code==200,"Original source observation lost response");
                    const auto observation=DecodeEccControlSourceObservation(response.body);Require(observation.sources.size()==2 && observation.sources[0].phase=="available","Slow source observation incomplete after release");
                    {std::lock_guard<std::mutex> lock(rig.w.logMutex);Require(rig.w.actions.empty(),"Source overlap/status reads sent production commands");}
                    IO::WriteNew(root+"/source-overlap-proof.json","{\"passed\":true,\"status_responsive\":true,\"overlap_status\":409,\"hardware_actions\":0}\n");
                }
#endif
                if (scenario=="start-ack-loss") {
                    Rejects([&] { submit("start","start"); },"Incomplete supervisor HTTP"); Require(relay->dropped==1,"ECC acceptance ACK loss not executed");
                    const auto completed=Terminal(client,"start"); Require(IO::String(completed.At("phase"))=="complete","Accepted Start cancelled after caller disconnected"); State(client,EccState::Running);
                    // 只通过GET观察原操作；下面结束用全新的明确Stop/Breakup，不重发Start。
                } else execute("start","start",EccState::Running);
                if(fdt) for(const auto &entry:raw) Require(entry.second->Connected(),"FDT Start did not connect raw sender");
                rig.Index(index,native.auditRoot+"/operations/start/start","start");
                if (scenario=="ecc-restart") {
                    rig.Clear(); process->CrashAndRestart(); const auto reopened=Read(direct.Identity()); Require(IO::String(reopened.At("instance_id"))!=IO::String(identity.At("instance_id")),"ECC reopened instance unchanged");
                    Require(IO::String(Read(direct.Operation("start")).At("phase"))=="complete" && IO::Boolean(Read(direct.Status()).At("control_needs_reconciliation")),"Restart lost historical operation/cleared gate");
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(identity,"start","stale"))).code==412,"Stale ECC instance accepted");
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(reopened,"start","new-start"))).code==409,"Reopened ECC resumed production");
                    for (const auto &service:services) { const auto state=DecodeSupervisorStatus(service->Status().At("lifecycle")); Require(state.state==DataSupervisorStatus::Ready,"Restart implicitly killed receiver"); }
                    { std::lock_guard<std::mutex> lock(rig.w.logMutex); Require(rig.w.actions.empty(),"ECC restart replayed hardware action"); }
                } else {
                    rig.Clear(); execute("pause","pause",EccState::Paused); rig.Clear(); execute("resume","resume",EccState::Running); rig.Index(index,native.auditRoot+"/operations/resume/resume","resume");
                    execute("stop","stop",EccState::Ready); rig.Clear(); execute("start","restart",EccState::Running); rig.Index(index,native.auditRoot+"/operations/restart/start","start"); execute("stop","stop-again",EccState::Ready);
                    if(fdt) for(const auto &entry:raw) Require(!entry.second->Connected(),"FDT Stop retained the physical TCP connection");
                    const auto retained=Read(client.Status());
                    for (std::size_t i=0;i<2;++i) {
                        const auto a=DecodeSupervisorStatus(ready.At("data").At("nodes").items[i].At("lifecycle")),b=DecodeSupervisorStatus(retained.At("data").At("nodes").items[i].At("lifecycle"));
                        Require(a.sessionId==b.sessionId && a.receivers[0].pid==b.receivers[0].pid,"Pause/Resume or Stop/Start replaced receiver session/PID");
                    }
                    if (scenario=="truncated") {
                        raw.at("C0")->truncate=true; submit("breakup","breakup"); const auto failed=Terminal(client,"breakup");
                        Require(IO::String(failed.At("phase"))=="failed" && IO::Boolean(failed.At("result").At("outcome_unknown")),"Truncated receiver fabricated Prepared");
                        const auto second=DecodeSupervisorStatus(services[1]->Status().At("lifecycle")); Require(second.state==DataSupervisorStatus::Stopped && second.receivers[0].sealVerified,"Independent receiver seal lost after first failed");
                        Require(client.PostOperation(EncodeEccControlRequest(Request(identity,"breakup","repeat-breakup"))).code==409,"UNKNOWN Breakup replayed");
                    } else {
                        execute("breakup","breakup",EccState::Prepared); CheckFiles(Read(client.Status()),raw,6,false,iceExpected);
                        execute("configure","configure-second",EccState::Ready); const auto second=Read(client.Status());
                        for (std::size_t i=0;i<2;++i) Require(IO::String(ready.At("data").At("nodes").items[i].At("lifecycle").At("session_id"))!=IO::String(second.At("data").At("nodes").items[i].At("lifecycle").At("session_id")),"New Configure reused sealed session");
                        execute("start","start-second",EccState::Running); execute("stop","stop-second",EccState::Ready); execute("breakup","breakup-second",EccState::Prepared); CheckFiles(Read(client.Status()),raw,2,false,iceExpected);
                        execute("reset","unprepare",EccState::Described); execute("reset","forget",EccState::Idle);
                        Require(services[0]->Commands("start")==2 && services[1]->Commands("start")==2 && services[0]->Commands("finish")==2 && services[1]->Commands("finish")==2,"Remote actions were replayed");
                        std::map<std::string,unsigned int> accepted; std::ifstream log(root+"/controller/network/operations.jsonl"); std::string row;
                        while (std::getline(log,row)) { const auto event=ParseStrictJson(row); if (IO::String(event.At("event"))=="accepted") ++accepted[DecodeEccControlRequest(IO::String(event.At("request"))).command]; }
                        auto expected=std::map<std::string,unsigned int>{{"describe",1},{"prepare",1},{"configure",2},{"start",3},{"stop",3},{"pause",1},{"resume",1},{"breakup",2},{"reset",2}};
                        if(maintenanceWindow){expected["configure-cobo"]=1;expected["reprepare-cobo"]=1;expected["set-cobo-led"]=1;expected["trigger-asad-pulser"]=1;expected["blink-cobo-leds"]=1;expected["flip-cobo-led"]=1;expected["pulse-cobo-led"]=2;}
                        Require(accepted==expected,"Duplicate/ACK loss replayed ECC hardware workflow");
                    }
                }
            }
            }
            }
            if (scenario=="handoff-service-0" || scenario=="handoff-service-1" || changedPlans || coldSame) {
                const auto inputs=root+"/successor-inputs";IO::CreateDirectory(inputs);
                auto nextOptions=native;nextOptions.auditRoot=root+"/successor-controller";nextOptions.deploymentId="planned-network-successor";
                if (changedPlans) {
                    auto changed=DecodeSystemExperimentArtifact(native.experimentArtifactBytes);changed.configurationSha256.clear();changed.artifactSha256.clear();
                    Require(!changed.settings.cobos.empty() && !changed.settings.cobos[0].configure.asad.boards.empty(),"Fixture needs a configurable AsAd delay");
                    auto &delay=changed.settings.cobos[0].configure.asad.boards[0].triggerDelay;delay=(delay+1)%32;
                    nextOptions.experimentArtifactBytes=EncodeSystemExperimentArtifact(changed);
                }
                const auto nextArtifact=DecodeSystemExperimentArtifact(nextOptions.experimentArtifactBytes);
                auto nextRemote=remote;auto nextPlans=plans;std::vector<EccReceiverPlanTarget> targets;
                if (changedPlans) for (std::size_t i=0;i<nextPlans.size();++i) {
                    auto &next=nextPlans[i];next.auditRoot=services[i]->root+"/next-deployment";next.deploymentId="changed-receiver-plan";
                    next.configurationSha256=nextArtifact.configurationSha256;next.artifactSha256=nextArtifact.artifactSha256;
                    for (auto &source:next.sources) {source.queueMiB=2;source.maxFrameMiB=2;source.rotateMiB=2;}
                    const auto bytes=IO::EncodeOptions(next,ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)));
                    EccReceiverPlanTarget target;target.agentId=services[i]->agent;target.planSha256=ConfigurationSha256(bytes);target.planFile=inputs+"/reviewed-plan-"+std::to_string(i)+".json";
                    IO::WriteNew(target.planFile,bytes);Require(!::chmod(target.planFile.c_str(),0400),"Seal reviewed fixture plan");
                    targets.push_back(target);nextRemote.endpoints[i].planSha256=target.planSha256;
                }
                const auto nextFile=WriteConfiguration(inputs,argv[1],nextOptions,nextRemote,port,credential);
                FixedEccConfiguration nextConfiguration(nextFile);const auto planFile=inputs+"/successor.json";
                EccSuccessorPlan::Create(planFile,nextConfiguration);EccSuccessorPlan plan(planFile,nextConfiguration);
                Require(direct.ControlHandoff().code==503,"Uncommitted ECC exposed handoff receipt");
                auto request=Request(identity,changedPlans ? "retire-data-plans" : "handoff-data-control","handoff-data");
                request.successorReceiverPlans=targets;
                request.successorScopeSha256=plan.ScopeSha256();request.successorEpoch=plan.Epoch();request.successorPlanSha256=plan.Sha256();
                request.successorDataBindingSha256=plan.DataBindingSha256();
                if(planWindow || partialCompletion || failedNodeCompletion || (coldCompletion && !coldSame))request.successorPlanFile=planFile;
                const auto originalStatusForCompletion=(partialCompletion || failedNodeCompletion || coldCompletion) ? direct.Status().body : std::string();
                const auto hardwareBefore=rig.w.actions.size();
                std::string failedChildOperationId;
                auto unsupported=request;unsupported.successorDataBindingSha256=std::string(64,'0');
                Require(direct.PostOperation(EncodeEccControlRequest(unsupported)).code==412,"ECC accepted changed receiver plans");
                const char *tool=::getenv("GETDAQ_TEST_ECC_HANDOFF_TOOL");
                const bool useTool=tool && (scenario=="handoff-service-1" || scenario=="plan-transition-1");
                if (useTool) {
                    std::vector<std::string> arguments={changedPlans ? "retire-submit" : "submit",config,nextFile,planFile,request.clientEpoch,request.operationId,inputs+"/tool-evidence"};
                    if (changedPlans) {
                        std::string document="{\"schema\":1,\"kind\":\"getdaq.reviewed-data-plan-targets\",\"plans\":[";
                        for (const auto &target:targets) {if(document.back()!='[')document+=',';document+="{\"agent_id\":"+EncodeJsonString(target.agentId)+",\"plan_sha256\":"+EncodeJsonString(target.planSha256)+",\"plan_file\":"+EncodeJsonString(target.planFile)+"}";}
                        IO::WriteNew(inputs+"/targets.json",document+"]}\n");arguments.push_back(inputs+"/targets.json");
                    }
                    HandoffTool(tool,arguments,inputs+"/tool-submit.log");
                    auto inspect=arguments;inspect[0]=changedPlans ? "retire-inspect" : "inspect";HandoffTool(tool,inspect,inputs+"/tool-inspect.log");
                    HandoffTool(tool,arguments,inputs+"/tool-repeat-submit.log",false);
                } else if(planWindow) {
                    std::string document="{\"schema\":1,\"kind\":\"getdaq.reviewed-data-plan-targets\",\"plans\":[";
                    for(const auto &target:targets) {if(document.back()!='[')document+=',';document+="{\"agent_id\":"+EncodeJsonString(target.agentId)+",\"plan_sha256\":"+EncodeJsonString(target.planSha256)+",\"plan_file\":"+EncodeJsonString(target.planFile)+"}";}
                    IO::WriteNew(inputs+"/targets.json",document+"]}\n");IO::CreateDirectory(root+"/gui-switch-evidence");
                    std::string nodes="[";
                    for(std::size_t i=0;i<plans.size();++i) {if(nodes.back()!='[')nodes+=',';nodes+="{\"id\":"+std::to_string(i)+",\"role\":\"cobo\",\"host\":\"127.0.0.1\",\"native_sender_id\":"+EncodeJsonString(plans[i].sources[0].id)+",\"native_agent_id\":"+EncodeJsonString(remote.endpoints[i].agentId)+"}";}
                    nodes+=']';
                    const auto gui="{\"experiment\":\"NativeIntegration\",\"run_number\":0,\"getdaq\":{\"simulation\":false,\"config_dir\":"+EncodeJsonString(root+"/unused-legacy-config")+",\"output_dir\":"+EncodeJsonString(root+"/gui-runs")+
                        ",\"poll_ms\":60,\"timeout_ms\":2000,\"step_timeout_ms\":180000,\"native_ecc\":{\"address\":\"127.0.0.1\",\"port\":"+std::to_string(port)+",\"credential_file\":"+EncodeJsonString(credential)+
                        ",\"service_id\":\"test-ecc\",\"client_id\":\"test-console\",\"client_epoch\":"+EncodeJsonString(request.clientEpoch)+",\"snapshot_sha256\":"+EncodeJsonString(request.snapshotSha256)+",\"input_binding_sha256\":"+EncodeJsonString(IO::String(identity.At("input_binding_sha256")))+
                        ",\"timeout_ms\":2000,\"status_max_age_ms\":10000,\"record_dir\":"+EncodeJsonString(root+"/gui-client-records")+"},\"nodes\":"+nodes+"}}\n";
                    IO::WriteNew(root+"/gui-plan.json",gui);
                    std::future<void> restore,nodeRepair;
                    if(failedNodeWindow) {
                        nodeRepair=std::async(std::launch::async,[&] {
                            try {
                                Wait([&] {try {failedChildOperationId=AcceptedOperationId(*services[1],"retire-plan");return true;} catch(...) {return false;}},180000);
                                IO::WriteNew(root+"/retirement-fault-marker","private fault gate opened after durable child acceptance\n");
                                Wait([&] {struct stat st;return !::lstat((root+"/gui-node-repair-request").c_str(),&st);},180000);
                                const auto childOperationId=failedChildOperationId;
                                const auto childDirectory=plans[1].auditRoot+"/control/operations/"+childOperationId;
                                const auto originalFailure=services[1]->Operation(childOperationId);
                                Require(IO::String(originalFailure.At("phase"))=="failed" && IO::String(originalFailure.At("command"))=="retire-plan","GUI requested repair before the original child had a failed terminal result");
                                const auto originalRequestBytes=IO::ReadFile(childDirectory+"/request.json",16384);
                                const auto originalRequest=DecodeSupervisorControlRequest(originalRequestBytes);
                                const auto parentRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-node-repair-request",8192));
                                Require(originalRequest.operationId==childOperationId && originalRequest.command==SupervisorCommand::RetirePlan &&
                                    originalRequest.controllerId==remote.controllerId && originalRequest.parentAuditReference==root+"/controller/operations/"+
                                    parentRequest.operationId+"/data-handoff","GUI-linked child failure did not retain the exact original parent audit reference");
                                const auto backup=targets[1].planFile+".backup";struct stat planState,backupState;
                                Require(!::lstat(backup.c_str(),&backupState) && ::lstat(targets[1].planFile.c_str(),&planState)!=0,
                                    "Injected accepted child failure did not preserve only the reviewed plan backup");
                                const auto exactPlan=IO::ReadFile(backup,1024*1024);
                                Require(ConfigurationSha256(exactPlan)==targets[1].planSha256 && !::rename(backup.c_str(),targets[1].planFile.c_str()) &&
                                    ConfigurationSha256(IO::ReadFile(targets[1].planFile,1024*1024))==targets[1].planSha256 && !::chmod(targets[1].planFile.c_str(),0400),
                                    "Operator did not restore the exact reviewed plan bytes before node repair");
                                auto repair=originalRequest;repair.command=SupervisorCommand::CompletePlanRetirement;
                                repair.failedRetirementOperationId=childOperationId;repair.operationId="operator-node-window-repair";
                                SupervisorHttpClient childClient(services[1]->direct);
                                Require(childClient.Submit(repair).code==202,"Explicit operator node repair was not accepted");
                                JsonValue repairResult;Wait([&] {repairResult=services[1]->Operation(repair.operationId);const auto phase=IO::String(repairResult.At("phase"));
                                    Require(phase!="failed" && phase!="unknown","Explicit operator node repair failed");return phase=="complete";});
                                const auto retainedFailure=services[1]->Operation(childOperationId);
                                Require(IO::String(retainedFailure.At("phase"))=="failed" && IO::String(retainedFailure.At("command"))=="retire-plan" &&
                                    IO::ReadFile(childDirectory+"/request.json",16384)==originalRequestBytes && services[1]->Commands("complete-plan-retirement")==1 &&
                                    services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,
                                    "Explicit node repair rewrote the child failure or replayed a sibling request");
                                Require(direct.ControlHandoff().code==503,
                                    "Parent exported a retirement receipt before the Qt operator confirmed completion");
                                IO::WriteNew(root+"/gui-node-repair-proof.json","{\"passed\":true,\"original_child_failure_retained\":true,\"parent_was_not_repaired\":true,\"node_operation_id\":"+
                                    EncodeJsonString(childOperationId)+",\"repair_operation_id\":"+EncodeJsonString(repair.operationId)+",\"exact_reviewed_plan_restored\":true}\n");
                                IO::WriteNew(root+"/gui-node-repair-complete","Separate node repair completed; parent Qt intent remains explicit\n");
                            } catch(const std::exception &e) {
                                try {IO::WriteNew(root+"/gui-node-repair-failed",e.what());} catch(...) {}
                                throw;
                            }
                        });
                    }
                    if(completionWindow) {
                        Require(!::unlink(targets[1].planFile.c_str()),"Remove only owned second reviewed plan for actual window completion");
                        const auto exactPlan=IO::EncodeOptions(nextPlans[1],ConfigurationSha256(IO::ReadFile(argv[3],64*1024*1024)));
                        restore=std::async(std::launch::async,[&,exactPlan] {
                            Wait([&] {struct stat st;return !::lstat((root+"/gui-completion-restore").c_str(),&st);},120000);
                            Require(ConfigurationSha256(exactPlan)==targets[1].planSha256,"GUI fixture restore changed original reviewed target");
                            IO::WriteNew(targets[1].planFile,exactPlan);Require(!::chmod(targets[1].planFile.c_str(),0400),"Restore exact missing reviewed plan");
                            IO::WriteNew(root+"/gui-completion-restored","Fixture restored exact original plan; no GUI data-host file access\n");
                        });
                    }
                    std::vector<std::string> words={argv[9],root+"/gui-plan.json",planFile,inputs+"/targets.json",root+"/gui-switch-evidence"};
                    if(completionWindow)words.push_back("--partial-completion");
                    if(failedNodeWindow)words.push_back("--failed-node-completion");
                    std::vector<char *> arguments;for(auto &word:words)arguments.push_back(&word[0]);arguments.push_back(nullptr);
                    pid_t probe=0;Require(!::posix_spawn(&probe,argv[9],nullptr,nullptr,arguments.data(),environ),"Start production plan transition window");int status=0;
                    Require(::waitpid(probe,&status,0)==probe && WIFEXITED(status) && WEXITSTATUS(status)==0,"Production plan transition window failed");
                    if(restore.valid())restore.get();
                    if(nodeRepair.valid())nodeRepair.get();
                    request=DecodeEccControlRequest(IO::ReadFile(root+"/gui-switch-evidence/request.json",8192));
                    Require(request.command=="retire-data-plans" && request.successorPlanFile==planFile,"GUI did not bind the reviewed service-host identity");
                } else {
                    if(partialPlans && !failedNodeScenario)Require(!::unlink(targets[1].planFile.c_str()),"Remove only owned second reviewed plan");
                    if(failedNodeCompletion) {
                        const auto dataStatus=ParseStrictJson(direct.Status().body);const auto scope=IO::String(dataStatus.At("data").At("controller_scope_sha256")),epoch=IO::String(dataStatus.At("data").At("controller_epoch"));
                        const auto originalPath=root+"/controller/operations/"+request.operationId+"/data-handoff";
                        failedChildOperationId=ConfigurationSha256("getdaq.remote-data-operation:1\n"+scope+'\n'+epoch+'\n'+originalPath+"\nretire-plan\n"+services[1]->agent);
                        const auto failedDirectory=plans[1].auditRoot+"/control/operations/"+failedChildOperationId;
                        Require(!::symlink((failedDirectory+"/request.json").c_str(),(root+"/retirement-fault-marker").c_str()),"Create private pending child-acceptance marker");
                    }
                    Require(direct.PostOperation(EncodeEccControlRequest(request)).code==202,"ECC did not accept explicit handoff");
                }
                auto operation=Terminal(direct,request.operationId);
                SupervisorHttpResponse receipt;
                if(failedNodeScenario) {
                    Require(operation.At("phase").text=="failed","ECC hid the accepted failed-node retirement");
                    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,"Failed-node scenario did not accept both original child requests exactly once");
                    if(!failedNodeWindow)Require(direct.ControlHandoff().code==503,"Failed-node parent exported a retirement receipt");
                    IO::WriteNew(root+"/partial-retirement-operation.json",direct.Operation(request.operationId).body);
                    IO::WriteNew(root+"/original-fenced-status.json",direct.Status().body);
                    if(failedNodeWindow)failedChildOperationId=AcceptedOperationId(*services[1],"retire-plan");
                    const auto backup=targets[1].planFile+".backup";struct stat fault;
                    if(failedNodeCompletion) {
                        Require(!::lstat(backup.c_str(),&fault) && ::lstat(targets[1].planFile.c_str(),&fault)!=0,"Fault did not occur after child acceptance at the reviewed-plan read");
                        Require(!::rename(backup.c_str(),targets[1].planFile.c_str()) && ConfigurationSha256(IO::ReadFile(targets[1].planFile,1024*1024))==targets[1].planSha256 && !::chmod(targets[1].planFile.c_str(),0400),"Restore the exact reviewed plan inode and bytes");
                    }
                    const auto failedDirectory=plans[1].auditRoot+"/control/operations/"+failedChildOperationId;
                    const auto childFailure=services[1]->Operation(failedChildOperationId);Require(childFailure.At("phase").text=="failed" && IO::String(childFailure.At("command"))=="retire-plan","Injected child failure was not journaled as the original retirement");
                    const auto originalChildRequest=DecodeSupervisorControlRequest(IO::ReadFile(failedDirectory+"/request.json",16384));
                    std::string repairId;
                    if(failedNodeCompletion) {
                        auto repair=originalChildRequest;repair.command=SupervisorCommand::CompletePlanRetirement;repair.failedRetirementOperationId=failedChildOperationId;repair.operationId="operator-node-repair";
                        const auto expectedRepair=repair;
                        Require(EncodeSupervisorControlRequest(repair)==EncodeSupervisorControlRequest(expectedRepair),"Operator repair changed the original child retirement scope");
                        SupervisorHttpClient childClient(services[1]->direct);Require(childClient.Submit(repair).code==202,"Operator's node repair was not accepted");
                        repairId=repair.operationId;
                    } else {
                        const auto repairProof=ParseStrictJson(IO::ReadFile(root+"/gui-node-repair-proof.json",65536));
                        Require(IO::Boolean(repairProof.At("passed")) && IO::Boolean(repairProof.At("exact_reviewed_plan_restored")) &&
                            IO::Boolean(repairProof.At("parent_was_not_repaired")) && IO::String(repairProof.At("node_operation_id"))==failedChildOperationId,
                            "Qt fixture did not wait for the independent exact-scope node repair");
                        repairId=IO::String(repairProof.At("repair_operation_id"));
                        Require(!::lstat(targets[1].planFile.c_str(),&fault) && ::lstat(backup.c_str(),&fault)!=0 &&
                            ConfigurationSha256(IO::ReadFile(targets[1].planFile,1024*1024))==targets[1].planSha256,
                            "Qt parent completion began before the exact reviewed plan was restored on the node");
                    }
                    JsonValue repairResult;Wait([&]{repairResult=services[1]->Operation(repairId);const auto phase=repairResult.At("phase").text;Require(phase!="failed" && phase!="unknown","Explicit child repair failed");return phase=="complete";});
                    const auto failedAfterRepair=services[1]->Operation(failedChildOperationId);
                    Require(failedAfterRepair.At("phase").text=="failed" && EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(IO::ReadFile(failedDirectory+"/request.json",16384)))==EncodeSupervisorControlRequest(originalChildRequest) &&
                        services[1]->Commands("complete-plan-retirement")==1 && services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,"Node repair changed original failure or replayed a sibling");
                    EccControlRequest completion;
                    if(failedNodeCompletion) {
                        completion=Request(identity,"complete-data-handoff","complete-original-handoff");completion.failedOperationId=request.operationId;
                        Require(direct.PostOperation(EncodeEccControlRequest(completion)).code==202,"ECC parent did not accept explicit completion observation");
                    } else {
                        completion=DecodeEccControlRequest(IO::ReadFile(root+"/gui-switch-evidence/completion-request.json",8192));
                        Require(completion.command=="complete-data-handoff" && completion.failedOperationId==request.operationId,
                            "Qt did not explicitly complete the failed parent only after operator node repair");
                    }
                    operation=Terminal(direct,completion.operationId);Require(operation.At("phase").text=="complete","ECC parent HTTP failed to observe the explicit node repair proof");
                    IO::WriteNew(root+"/completion-operation.json",direct.Operation(completion.operationId).body);
                    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1 && rig.w.actions.size()==hardwareBefore,"Parent completion replayed a child or wrote hardware");
                    IO::WriteNew(root+"/partial-completion-result.json","{\"passed\":true,\"original_failure_retained\":true,\"both_original_children_accepted_once\":true,\"old_controller_still_fenced\":true,\"no_completion_hardware_writes\":true,\"operator_cli\":false,\"operator_window\":"+std::string(failedNodeWindow ? "true" : "false")+",\"explicit_node_repair\":true}\n");
                    Require(direct.Operation(request.operationId).code==200 && Terminal(direct,request.operationId).At("phase").text=="failed","Parent completion rewrote the original failed retirement");
                    IO::WriteNew(root+"/handoff-operation.json",direct.Operation(request.operationId).body);
                    for(const auto &command:{"prepare","reset","stop-production-after-failure"})
                        Require(direct.PostOperation(EncodeEccControlRequest(Request(identity,command,std::string("fenced-")+command))).code==409,"Completed original ECC accepted a new hardware command");
                    receipt=direct.ControlHandoff();Require(receipt.code==200,"Explicit failed-node completion did not export the original retirement receipt");
                    const auto oldData=Read(direct.Status());IO::WriteNew(root+"/post-repair-fenced-status.json",direct.Status().body);
                    Require(oldData.At("control_needs_reconciliation").text=="true" && rig.w.actions.size()==hardwareBefore,"Repair released old control or wrote hardware");
                    process->Stop();
                } else if(partialPlans && !partialCompletion && !completionWindow && !coldCompletion) {
                    Require(operation.At("phase").text=="failed","Partial retirement claimed complete");
                    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==0,"Partial retirement replayed or fabricated node acceptance");
                    Require(direct.ControlHandoff().code==503,"Partial retirement exported a complete receipt");
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(identity,"prepare","partial-restart"))).code==409,"Partial retirement unfenced old ECC");
                    IO::WriteNew(root+"/partial-retirement-operation.json",direct.Operation(request.operationId).body);
                    IO::WriteNew(root+"/original-fenced-status.json",direct.Status().body);
                    process->Stop();process->Start();const auto cold=Read(direct.Identity());
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(cold,"prepare","partial-cold"))).code==409,"Cold partial retirement cleared fence");
                    Require(direct.ControlHandoff().code==503,"Cold partial retirement exported a receipt");process->Stop();
                    IO::WriteNew(root+"/partial-plan-result.json","{\"passed\":true,\"first_node_retired\":true,\"second_node_not_accepted\":true,\"cold_original_fenced\":true,\"automatic_retry\":false}\n");
                } else {
                if(coldCompletion) {
#include "ecc_cold_completion_network_case.h"
                } else {
                if(partialCompletion) {
#include "ecc_plan_completion_network_case.h"
                }
                if(completionWindow) {
                    Require(operation.At("phase").text=="failed","Window completion rewrote original parent result");
                    IO::WriteNew(root+"/partial-retirement-operation.json",direct.Operation(request.operationId).body);
                    const auto completedRequest=DecodeEccControlRequest(IO::ReadFile(root+"/gui-switch-evidence/completion-request.json",8192));
                    operation=Terminal(direct,completedRequest.operationId);IO::WriteNew(root+"/completion-operation.json",direct.Operation(completedRequest.operationId).body);
                    Require(completedRequest.failedOperationId==request.operationId,"Actual window completed another parent");
                    Require(services[0]->Commands("retire-plan")==1 && services[1]->Commands("retire-plan")==1,"Actual window omitted/replayed original child");
                }
                Require(operation.At("phase").text=="complete","ECC handoff did not commit");
                IO::WriteNew(root+"/handoff-operation.json",direct.Operation(request.operationId).body);
                Require(direct.PostOperation(EncodeEccControlRequest(request)).code==202,"Same handoff ID was not observed idempotently");
                auto conflict=request;conflict.successorEpoch=std::string(64,'f');
                Require(direct.PostOperation(EncodeEccControlRequest(conflict)).code==412,"Changed target reused original handoff ID");
                for (const auto &command:{"prepare","reset","stop-production-after-failure"})
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(identity,command,std::string("fenced-")+command))).code==409,"Old ECC accepted a new command after handoff");
                Wait([&]{receipt=direct.ControlHandoff();return receipt.code==200;});
                if (useTool)Require(IO::ReadFile(inputs+"/tool-evidence/receipt.json",1024*1024)==receipt.body,"Operator tool exported a different receipt");
                if(planWindow)Require(IO::ReadFile(root+"/gui-switch-evidence/receipt.json",1024*1024)==receipt.body,"GUI exported a different original receipt");
                const auto oldData=Read(direct.Status());IO::WriteNew(root+"/original-fenced-status.json",direct.Status().body);
                Require(oldData.At("control_needs_reconciliation").text=="true","Original ECC fence not visible");
                for (const auto &service:services)Require(service->Commands(changedPlans ? "retire-plan" : "handoff-control")==1,"ECC replayed node handoff");
                Require(rig.w.actions.size()==hardwareBefore,"Handoff changed hardware");
                if(scenario=="handoff-service-0" || scenario=="handoff-service-1" || partialCompletion)
                    process->ShutdownAfterHandoff(identity,request.operationId);
                else process->Stop();
                }
                const auto receiptFile=inputs+"/old-handoff.json";IO::WriteNew(receiptFile,receipt.body);
                if (changedPlans) {
                    const auto retired=ParseStrictJson(receipt.body);
                    for (std::size_t i=0;i<services.size();++i) {
                        const JsonValue *node=nullptr;
                        for (const auto &entry:retired.At("nodes").items) if(IO::String(entry.At("agent_id"))==services[i]->agent)node=&entry;
                        Require(node!=nullptr,"Retirement receipt omitted fixture data node");
                        const auto proof=plans[i].auditRoot+"/control/operations/"+IO::String(node->At("operation_id"))+"/plan-retirement.json";
                        services[i]->ReplacePlan(nextPlans[i],proof);
                    }
                }
                auto nextEndpoint=process->direct;
                EccProcess successor(argv[5],nextFile,inputs,nextEndpoint,{"--successor-plan",planFile,"--handoff-receipt",receiptFile});
                NativeControlHttpClient nextClient(nextEndpoint);const auto nextStatus=DecodeEccControlSystemStatus(nextClient.Status().body);
                Require(nextStatus.dataScopeSha256==plan.ScopeSha256() && nextStatus.dataEpoch==plan.Epoch() &&
                    !nextStatus.dataNeedsReconciliation && !nextStatus.controlNeedsReconciliation,"Planned ECC executable did not adopt original proof");
                Require(nextStatus.nodes.size()==2,"Planned ECC lost a data node");
                for (const auto &node:nextStatus.nodes) {
                    if(changedPlans)Require(node.state=="fresh" && node.receivers.empty(),"Changed plan adopted an old receiver cycle");
                    else Require(node.state=="stopped" && !node.receivers.empty() && node.receivers[0].reaped && node.receivers[0].sealVerified,"Planned ECC lost normal sealed session");
                }
                IO::WriteNew(root+"/successor-startup-status.json",nextClient.Status().body);
                Require(rig.w.actions.size()==hardwareBefore,"Successor startup changed hardware");
                if(changedPlans || coldSame) {
                    const auto newIdentity=Read(nextClient.Identity());
                    Require(newIdentity.At("configuration_sha256").text==nextArtifact.configurationSha256 && newIdentity.At("artifact_sha256").text==nextArtifact.artifactSha256 &&
                        (changedPlans ? newIdentity.At("configuration_sha256").text!=identity.At("configuration_sha256").text : newIdentity.At("configuration_sha256").text==identity.At("configuration_sha256").text),"New ECC did not use reviewed experiment parameters");
                    IO::WriteNew(root+"/successor-identity.json",nextClient.Identity().body);
                    const std::vector<std::pair<std::string,EccState>> commands={{"describe",EccState::Described},{"prepare",EccState::Prepared},{"configure",EccState::Ready},{"start",EccState::Running},{"stop",EccState::Ready},{"breakup",EccState::Prepared},{"reset",EccState::Described},{"reset",EccState::Idle}};
                    for(std::size_t i=0;i<commands.size();++i) {
                        const auto id="new-plan-"+std::to_string(i);Require(nextClient.PostOperation(EncodeEccControlRequest(Request(newIdentity,commands[i].first,id))).code==202,"Changed-plan successor rejected command");
                        const auto op=Terminal(nextClient,id);if(op.At("phase").text!="complete")throw std::runtime_error("Changed-plan successor command failed: "+commands[i].first);
                        State(nextClient,commands[i].second);
                        if(commands[i].first=="breakup")CheckFiles(Read(nextClient.Status()),raw,2);
                    }
                    IO::WriteNew(root+"/successor-new-cycle-status.json",nextClient.Status().body);
                }
                successor.Stop();
                bool refused=false;try {successor.Start();} catch (const std::exception &) {refused=true;}
                Require(refused,"Consumed successor plan restarted automatically");
                process->Start();const auto reopenedIdentity=Read(direct.Identity());
                for (const auto &command:{"prepare","reset","stop-production-after-failure"})
                    Require(direct.PostOperation(EncodeEccControlRequest(Request(reopenedIdentity,command,std::string("cold-fenced-")+command))).code==409,"Cold original ECC lost persisted handoff fence");
                Require(direct.ControlHandoff().code==503,"Cold ECC exported original live receipt as new authority");
                if(partialCompletion || failedNodeCompletion || completionWindow || failedNodeWindow || coldCompletion) {
                    auto coldCompletion=Request(reopenedIdentity,"complete-data-handoff","cold-completion");coldCompletion.failedOperationId=request.operationId;
                    Require(direct.PostOperation(EncodeEccControlRequest(coldCompletion)).code==409,"Cold ECC resumed original live completion");
                }
                process->Stop();
                if (!changedPlans) for (const auto &service:services)Require(service->Commands("handoff-control")==1,"Successor startup replayed handoff");
                if(partialCompletion && tool) {
                    HandoffTool(tool,{"review-cold",config,root+"/controller/operations/"+request.operationId+"/data-handoff",inputs+"/cold-review-completed"},inputs+"/cold-review-completed.log");
                    const auto review=ParseStrictJson(IO::ReadFile(inputs+"/cold-review-completed/review.json",1024*1024));
                    Require(!IO::Boolean(review.At("current_remote_identity_checked")) && !IO::Boolean(review.At("cold_recovery_executed")),"Offline review claimed live recovery");
                    for(const auto &node:review.At("nodes").items)Require(node.At("historical_delivery").text=="uncertain_or_accepted","Later accepted completion became a replayable rejection");
                    for(auto &service:services){service->Capture();service->Stop();}
                    Require(!::unlink(credential.c_str()),"Remove owned expired ECC credential for offline-only review");
                    for(const auto &endpoint:remote.endpoints)Require(!::unlink(endpoint.control.credentialFile.c_str()),"Remove owned expired node credential for offline-only review");
                    const auto originalNativeAudit=IO::ReadFile(root+"/controller/ecc.jsonl",64*1024*1024);
                    FixedEccConfiguration offline(config,FixedEccConfiguration::OfflineEvidenceOnly());
                    Require(!offline.ControlAccessValidated() && offline.BindingSha256()==IO::String(identity.At("input_binding_sha256")),"Offline configuration changed original input binding or claimed control");
                    Rejects([&]{EccControlJobs forbidden(offline);},"Offline evidence configuration");
                    Rejects([&]{FixedEccConfiguration control(config);},"credential");
                    HandoffTool(tool,{"review-cold",config,root+"/controller/operations/"+request.operationId+"/data-handoff",inputs+"/cold-review-no-credentials"},inputs+"/cold-review-no-credentials.log");
                    Require(IO::ReadFile(root+"/controller/ecc.jsonl",64*1024*1024)==originalNativeAudit,"Offline review/control refusal changed original native journal");
                    IO::WriteNew(root+"/cold-offline-result.json","{\"passed\":true,\"all_services_stopped\":true,\"expired_credentials_removed\":true,\"offline_configuration_cannot_start_control\":true,\"normal_configuration_still_requires_credentials\":true,\"original_input_binding_retained\":true,\"native_audit_unchanged\":true}\n");
                }
                IO::WriteNew(root+"/service-handoff-result.json","{\"passed\":true,\"two_independent_ecc_processes\":true,\"consumed_plan_restart_refused\":true,\"cold_original_fenced\":true,\"graceful_shutdown_after_handoff\":"+std::string(scenario=="handoff-service-0" || scenario=="handoff-service-1" || partialCompletion ? "true" : "false")+",\"operator_cli\":"+std::string(useTool ? "true" : "false")+",\"handoff_hardware_writes\":0,\"encoding_minor\":"+std::to_string(native.endpoints.encodingMinor)+"}\n");
                }
            }
            IO::WriteNew(root+"/final-status.json",(scenario=="handoff-service-0" || scenario=="handoff-service-1" || changedPlans || coldSame) ? IO::ReadFile(root+"/original-fenced-status.json",1024*1024) : direct.Status().body); index.flush(); Require(bool(index),"Flush network acquisition index");
            for (auto &service:services) if (service->pid>0) service->Capture();
            if (scenario=="normal-0") {
                const auto before=direct.Status().body; rig.Clear(); services[0]->Stop();
                auto options=process->direct; options.timeoutMs=3000; NativeControlHttpClient observer(options);
                const auto partial=observer.Receivers(); const auto unavailable=DecodeEccControlReceiverObservation(partial.body);
                Require(partial.code==200 && !unavailable.nodes[0].available && unavailable.nodes[1].available,"Offline data node masked healthy peer or fabricated observation");
                IO::WriteNew(root+"/partial-receiver-observation.json",partial.body);
                HangingObservationEndpoint hanging(services[0]->port);
                const auto started=std::chrono::steady_clock::now(); auto pending=std::async(std::launch::async,[&] {return observer.Receivers();});
                Wait([&] {return hanging.accepted.load();});
                const auto statusStart=std::chrono::steady_clock::now(); Require(direct.Status().body==before,"Process observation changed ECC cache");
                Require(std::chrono::steady_clock::now()-statusStart<std::chrono::milliseconds(500),"Slow data node blocked ECC status event loop");
                Require(observer.Receivers().code==409,"Concurrent observation queued another remote task");
                const auto bounded=pending.get(); const auto timed=DecodeEccControlReceiverObservation(bounded.body);
                Require(bounded.code==200 && std::chrono::steady_clock::now()-started<std::chrono::milliseconds(2500),"Receiver observation exceeded whole budget");
                Require(!timed.nodes[0].available && !timed.nodes[1].available,"Timed-out/unattempted nodes falsely available");
                IO::WriteNew(root+"/timed-receiver-observation.json",bounded.body);
                {std::lock_guard<std::mutex> lock(rig.w.logMutex);Require(rig.w.actions.empty(),"Read-only receiver fault observation sent hardware action");}
            }
        } catch (...) { for (auto &s:raw) s.second->FixtureClose();for(auto &stop:iceCleanup) stop(); for (auto &service:services) try { service->Capture(); } catch (...) {} throw; }
        for (auto &s:raw) s.second->FixtureClose();for(auto &stop:iceCleanup) stop();
        if(maintenanceWindow || partialCompletion || failedNodeCompletion || completionWindow || failedNodeWindow || coldCompletion) {
            // Do not let destructor cleanup hide a nonzero sanitizer exit.
            process->Stop();
            IO::WriteNew(root+((partialCompletion || failedNodeCompletion || completionWindow || failedNodeWindow || coldCompletion) ? "/completion-ecc-exit.json" : "/maintenance-ecc-exit.json"),"{\"normal_exit\":true,\"exit_code\":0}\n");
        }
        process.reset();
        if(receiverBarrier) {Require(receiverBarrier->Failure().empty(),"Receiver observation barrier failed");receiverBarrier.reset();}
        if (dataRelay) { Require(dataRelay->failure.empty() && dataRelay->dropped==0 && (catalogRecovery ? catalogRecoveryFaultInjected.load() : alarmInjected.load()),
            ("Data observation relay failed/dropped response (failure="+dataRelay->failure+", dropped="+std::to_string(dataRelay->dropped.load())+
             ", catalog_recovery_fault="+(catalogRecoveryFaultInjected.load()?"true":"false")+", alarm_injected="+(alarmInjected.load()?"true":"false")+")").c_str()); dataRelay.reset(); }
        if (relay) { Require(relay->failure.empty() && relay->dropped==1,"ECC relay result differs"); relay.reset(); }
        if(coldDataRelay) {Require(coldDataRelay->failure.empty() && coldDataRelay->dropped==1,"Cold data relay failed");coldDataRelay.reset();}
        services.clear(); adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy(); communicator->destroy();
        const bool receivers=(!descriptionRecovery || descriptionSuccess) && !preparationWindow && !hardwareObservation && (!inputFault || scenario.find("prepared")==std::string::npos) && scenario!="request-fault" && scenario!="ecc-interrupted" && !preparationFailure && !alarmUnknownPrepare;
        const bool complete=!iceSourceRecovery && !qtSourceRecovery && (maintenanceWindow || changedPlans || coldSame || scenario=="handoff-service-0" || scenario=="handoff-service-1" || scenario=="normal-0" || scenario=="normal-1" || zbuf || fdt || (ice && !iceReceiverFailure) || scenario=="start-ack-loss" || (qt && !catalogFailure && !qtAlarm && !qtReceiverFailure));
        IO::WriteNew(root+"/scope.json","{\"passed\":true,\"scenario\":"+EncodeJsonString(scenario)+",\"real_ecc_network_service\":true,\"real_data_services\":2,\"real_receivers\":"+(receivers ? "true" : "false")+
            ",\"post_reconciliation_cycle_tested\":"+((descriptionSuccess || iceSourceRecovery) ? "true" : "false")+",\"full_ecc_workflow_tested\":"+(complete ? "true" : "false")+",\"real_qt_runcontrol\":"+((maintenanceWindow || planWindow || qt || descriptionWindow || preparationWindow) ? "true" : "false")+",\"real_data_catalog\":"+(catalog ? "true" : "false")+
            ",\"startup_configuration_recorded\":"+(qt ? "true" : "false")+",\"synthetic_reviewed_publication\":"+(reviewedConfiguration ? "true" : "false")+
            ",\"real_alarm_policy\":"+(alarmCase || alarmUnknown || qtAlarm ? "true" : "false")+",\"standalone_cobo_mode\":true,\"physical_hosts\":1,\"real_hardware\":false,\"throughput_verified\":false}\n");
        std::cout<<"PASS ECC network "<<scenario<<(preparationWindow ? "; production MainWindow/RunControl original Prepare failure, stopped/cleanup/recovery intents; no receivers/raw\n" : preparationFailure ? "; retained Prepare failure and bound stop/cleanup/recovery; no receivers/raw\n" : descriptionRecovery ? "; bound original Describe reconciliation, no hardware writes during verification\n" : inputFault ? "; external-input hold, retained stop and audit/no-takeover boundaries\n" : hardwareObservation ? "; authenticated actual hardware-state GET; mock hardware, no receivers/raw\n" : "; actual ECC process -> 2 data services -> native receivers and direct raw TCP; synthetic localhost only\n");
    } catch (const std::exception &e) { if(!blockedCatalogPath.empty())::unlink(blockedCatalogPath.c_str());try {if(!testRoot.empty())IO::WriteNew(testRoot+"/test-failure.txt",e.what());} catch (...) {} std::cerr<<"FAIL "<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1; }
}
