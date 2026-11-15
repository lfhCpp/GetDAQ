#include "nativeecccoordinator.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include "hardwarestateprotocol.h"
#include <cerrno>
#include "hardwareaudit.h"
#include "system_acquisition_fixture.h"
#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <future>
#include <functional>
#include <limits>
#include <sys/stat.h>
namespace {
using namespace GetDAQAcquisitionFixture;
// 仅测试控制侧的数据生命周期门槛。没有运行receiver、没有原始MFM/真实seal证明。
class TestData : public EccDataLifecycle {
    World &world;
    mutable std::mutex mutex;
    mutable std::condition_variable changed;
    mutable bool entered=false,released=false;
public:
    bool running=false,failStart=false,failFinish=false,blockPrepare=false;
    unsigned int starts=0,finishes=0;
    unsigned int aborts=0,retirements=0;
    bool failRetirement=false;
    std::function<void()> onRetirement;
    mutable unsigned int readyChecks=0;
    std::function<void()> onValidate;
    std::function<void()> onReceiversStarted;
    std::function<void(unsigned int)> onReady;
    std::function<void(unsigned int)> onStopped;
    mutable unsigned int stoppedChecks=0;
    explicit TestData(World &w) : world(w) {}
    std::string BindingSha256() const override { return GetDAQ::ConfigurationSha256("test-only-data-lifecycle"); }
    void BindController(const std::string &snapshot,const std::string &root) override {
        Require(snapshot.size()==64 && !root.empty() && root[0]=='/',"Test lifecycle parent scope binding missing");
    }
    bool NeedsReconciliation() const override { return false; } // 明确测试替身，无真实数据侧核对。
    void Validate(EccCommand c,EccState,const std::vector<DataSenderOptions> &senders,const std::string &config,const std::string &artifact) const override {
        Require(senders.size()==4,"Test data lifecycle received wrong binding");
        Require(config.size()==64 && artifact.size()==64,"Test lifecycle did not receive both parent digests");
        if (!running && c==EccCommand::Stop) throw std::runtime_error("Test receiver validation would block Stop");
        if (onValidate) onValidate();
        if (blockPrepare && c==EccCommand::Prepare) {
            std::unique_lock<std::mutex> lock(mutex); entered=true; changed.notify_all();
            if (!changed.wait_for(lock,std::chrono::seconds(20),[this] { return released; })) throw std::runtime_error("Fixture validation gate timed out");
        }
    }
    void WaitForValidation() {
        std::unique_lock<std::mutex> lock(mutex);
        Require(changed.wait_for(lock,std::chrono::seconds(10),[this] { return entered; }),"Prepare did not reach validation gate");
    }
    void Release() { std::lock_guard<std::mutex> lock(mutex); released=true; changed.notify_all(); }
    void VerifyStopped() const override { ++stoppedChecks;if(onStopped)onStopped(stoppedChecks);if (running) throw std::runtime_error("Test receiver is not stopped"); }
    void VerifyReady() const override { ++readyChecks; if (onReady) onReady(readyChecks); if (!running) throw std::runtime_error("Test receiver is not ready"); }
    void StartReceivers(const std::string &path,const std::vector<DataSenderOptions> &) override {
        VerifyStopped(); CreateHardwareAuditDirectory(path); HardwareAudit audit(path+"/data-lifecycle.jsonl");
        audit.Append("{\"phase\":\"intent\",\"test_double\":true,\"receiver_executed\":false}");
        ++starts; world.Record("data.receiver-start");
        if (failStart) throw std::runtime_error("Injected receiver start failure");
        running=true; audit.Append("{\"phase\":\"complete\",\"test_double\":true,\"receiver_executed\":false}");
        if (onReceiversStarted) onReceiversStarted();
    }
    void FinishReceivers(const std::string &path) override {
        VerifyReady(); CreateHardwareAuditDirectory(path); HardwareAudit audit(path+"/data-lifecycle.jsonl");
        audit.Append("{\"phase\":\"intent\",\"test_double\":true,\"receiver_executed\":false}");
        ++finishes; world.Record("data.receiver-finish");
        if (failFinish) throw std::runtime_error("Injected receiver seal failure");
        running=false; audit.Append("{\"phase\":\"complete\",\"test_double\":true,\"receiver_executed\":false}");
    }
    void AbortFailedReceivers(const std::string &path,const std::string &parent) override {
        Require(!parent.empty(),"Test abort parent missing");CreateHardwareAuditDirectory(path);
        HardwareAudit audit(path+"/data-lifecycle.jsonl");++aborts;running=false;
        audit.Append("{\"phase\":\"complete\",\"test_double\":true,\"receiver_executed\":false}");
    }
    void RetireFailedReceivers(const std::string &path,const std::string &parent) override {
        Require(!parent.empty() && aborts==1 && !running,"Test retirement lacks original Abort");
        CreateHardwareAuditDirectory(path);HardwareAudit audit(path+"/data-lifecycle.jsonl");++retirements;
        if(onRetirement)onRetirement();if(failRetirement)throw std::runtime_error("Injected data retirement failure");
        audit.Append("{\"phase\":\"complete\",\"test_double\":true,\"receiver_executed\":false}");
    }
};
NativeEccOptions Options(Rig &r,const std::string &root) {
    NativeEccOptions o; o.auditRoot=root; o.deploymentId="test-only-ecc"; o.endpoints=r.options; o.senders=r.senders;
    SystemAlignmentOptions topology; topology.encodingMinor=r.options.encodingMinor; topology.timeoutMs=r.options.timeoutMs;
    topology.exclusiveOwnerConfirmed=topology.daqStoppedConfirmed=true;
    for (const auto &c:r.options.cobos) {
        AlignmentCoBoEndpoint e; e.id=c.id; e.nodeProxy=c.prepare.nodeProxy; e.linkProxy=c.linkProxy;
        e.expectedNodeName=c.prepare.expectedNodeName; e.description=c.prepare.description; e.enabled=c.enabled; topology.cobos.push_back(e);
    }
    SystemExperimentArtifact artifact; artifact.profileSha256=GetDAQ::ConfigurationSha256("test-only-profile");
    ExperimentConfigurationSource source; source.path="test-only-typed-settings"; source.sha256=GetDAQ::ConfigurationSha256("test-only-source"); artifact.sources.push_back(source);
    for (const auto &c:r.options.cobos) {
        SystemExperimentCoBo e; e.id=c.id; e.enabled=c.enabled; e.prepare=c.prepare.settings; e.configure=c.configure; artifact.settings.cobos.push_back(e);
    }
    for (const auto &m:r.options.mutants) {
        topology.mutants.push_back(m.endpoint);
        SystemExperimentMutant e; e.id=m.endpoint.id; e.settings=m.settings; e.settings.moduleCount=unsigned(r.options.mutants.size()); artifact.settings.mutants.push_back(e);
    }
    o.description=BuildSystemDescriptionOptions(topology,DescriptionPolicy::UseExisting);
    o.experimentArtifactBytes=EncodeSystemExperimentArtifact(artifact); return o;
}
void HardwareObservation(Rig &rig,const NativeEccOptions &options,TestData &data) {
    NativeEccCoordinator ecc(options,data);const auto wrap=[&]{const auto bytes=ecc.ObserveHardwareStateJson();return "{\"instance_id\":\""+std::string(64,'a')+"\","+bytes.substr(1);};
    const auto writes=rig.w.Writes();const auto initial=ecc.Status();const auto observed=DecodeHardwareStateObservation(wrap());
    Require(!observed.busy && observed.consistent && observed.nodes.size()==options.description.nodes.size(),"Hardware observation count/busy");
    Require(observed.coboLifecycleSource=="controller-baseline" && !observed.coboLifecycleIsHardwareReadback && observed.coboLifecycle.size()==options.endpoints.cobos.size(),"Hardware observation omitted cached per-CoBo lifecycle or overstated its source");
    for(const auto &board:observed.coboLifecycle){const auto found=std::find_if(options.endpoints.cobos.begin(),options.endpoints.cobos.end(),[&](const SystemCoBoSettings &candidate){return candidate.id==board.id;});Require(found!=options.endpoints.cobos.end() && board.state==(found->enabled ? "fresh" : "disabled"),"Initial CoBo command phase differs");}
    for(const auto &node:observed.nodes)Require(node.available && node.values.size()==(node.role=="cobo" ? 4 : 1),"Bound hardware state unavailable");
    Require(rig.w.Writes()==writes && ecc.Operations().empty() && ecc.Status().state==initial.state && ecc.Status().operationId==initial.operationId,"Observation mutated hardware/ECC/journal");
    const auto name=rig.nodes[0]->fName;rig.nodes[0]->fName="wrong-node";const auto unavailable=DecodeHardwareStateObservation(wrap());rig.nodes[0]->fName=name;
    Require(!unavailable.nodes.front().available && unavailable.nodes.front().values.empty(),"Wrong metadata retained values");
    auto corrupt=wrap();const auto marker=corrupt.find("\"recovery_verified\":false");Require(marker!=std::string::npos,"Recovery scope marker missing");corrupt.replace(marker,25,"\"recovery_verified\":true");Rejects([&]{DecodeHardwareStateObservation(corrupt);},"Invalid bounded");
    ecc.Execute(EccCommand::Describe,"describe-observation");const auto beforeBusy=rig.w.Writes();
    std::promise<void> entered,release;const auto gate=release.get_future().share();data.onValidate=[&]{entered.set_value();gate.wait();throw std::runtime_error("fixture validation release");};
    auto command=std::async(std::launch::async,[&]{Rejects([&]{ecc.Execute(EccCommand::Prepare,"busy-prepare");},"fixture validation release");});entered.get_future().wait();HardwareStateObservation busy;try{busy=DecodeHardwareStateObservation(wrap());}catch(...){release.set_value();command.get();throw;}release.set_value();command.get();data.onValidate={};
    Require(busy.busy && busy.coboLifecycle.empty() && rig.w.Writes()==beforeBusy,"Busy hardware query read/wrote during command or retained lifecycle snapshot");for(const auto &node:busy.nodes)Require(!node.available && node.values.empty(),"Busy query retained observations");
    // Real Ice RPC held at the fixture; a negative request must still acquire
    // the coordinator lock. Its rejection keeps parent state/ID unchanged,
    // so only the command epoch can invalidate this in-flight observation.
    std::promise<void> readEntered,readRelease,readExited;
    auto enteredRead=readEntered.get_future();
    auto releasedRead=readRelease.get_future().share();
    auto exitedRead=readExited.get_future();
    std::atomic<bool> firstRead{true};
    auto device=rig.devices[0].at("ctrl");
    device->beforeReadField=[&] {
        if (!firstRead.exchange(false)) return;
        readEntered.set_value();
        releasedRead.wait();
        readExited.set_value();
    };
    const auto beforeRace=ecc.Status();
    auto sample=std::async(std::launch::async,[&]{return DecodeHardwareStateObservation(wrap());});
    HardwareStateObservation withdrawn;
    try {
        Require(enteredRead.wait_for(std::chrono::seconds(2))==std::future_status::ready,"Observation did not reach held Ice field RPC");
        Rejects([&]{ecc.StopProductionAfterFailure("read-race-negative");},"requires UNKNOWN");
        readRelease.set_value();
        exitedRead.wait();
        withdrawn=sample.get();
    } catch (...) {
        if (releasedRead.wait_for(std::chrono::milliseconds(0))!=std::future_status::ready) readRelease.set_value();
        sample.wait();
        // Callback completion precedes removal; no mutation of a live callback.
        if (!firstRead.load()) exitedRead.wait();
        device->beforeReadField={};
        throw;
    }
    device->beforeReadField={};
    Require(!withdrawn.busy && !withdrawn.consistent,"In-flight sample survived a negative command with unchanged parent ID");
    for (const auto &node:withdrawn.nodes) Require(!node.available && node.values.empty(),"Command race retained stale hardware bits");
    Require(ecc.Status().state==beforeRace.state && ecc.Status().operationId==beforeRace.operationId && rig.w.Writes()==beforeBusy,"Rejected negative command changed parent/hardware");
    std::ofstream raceReport(options.auditRoot+"/hardware-state-race.json");
    raceReport<<"{\"held_actual_ice_rpc\":true,\"negative_request_acquired_lock\":true,\"parent_state_and_id_unchanged\":true,\"samples_withdrawn\":true,\"hardware_writes\":0}\n";
    raceReport.close();Require(bool(raceReport),"Save hardware state race evidence");
    const auto bytes=wrap();std::ofstream report(options.auditRoot+"/hardware-state-test.json");report<<bytes;report.close();Require(bool(report),"Save hardware state evidence");
}
void Ownership(Rig &rig,NativeEccOptions options,TestData &data) {
    const auto parent=options.auditRoot.substr(0,options.auditRoot.find_last_of('/'));
    options.hostControlOwnershipRoot=parent+"/ownership";
    Require(::mkdir(options.hostControlOwnershipRoot.c_str(),0700)==0,"Create ownership root");
    rig.Clear();
    std::unique_ptr<NativeEccCoordinator> first(new NativeEccCoordinator(options,data));
    Require(rig.Actions().empty(),"Ownership startup called hardware");
    const auto state=GetDAQ::ParseStrictJson(first->OwnershipJson());
    Require(state.At("enabled").text=="true" && state.At("held_at_last_check").text=="true" && state.At("board_global_lock_verified").text=="false",
            "Ownership scope was misrepresented");
    auto conflicting=options; conflicting.auditRoot=parent+"/conflicting"; conflicting.deploymentId="another-deployment";
    bool rejected=false;
    try { NativeEccCoordinator second(conflicting,data); } catch (const std::exception &) { rejected=true; }
    Require(rejected && rig.Actions().empty(),"Conflicting controller did not fail before hardware");
    struct stat status;
    Require(::lstat(conflicting.auditRoot.c_str(),&status)!=0 && errno==ENOENT,"Conflict created an ECC audit root");
    const auto resource=state.At("resources").items.front().text;
    const auto file=options.hostControlOwnershipRoot+'/'+GetDAQ::ConfigurationSha256("getdaq.host-board:1\n"+resource)+".lock";
    std::ofstream mutate(file,std::ios::app); mutate<<'x'; mutate.close();
    rejected=false; try { first->Execute(EccCommand::Describe,"ownership-fault"); } catch (const std::exception &) { rejected=true; }
    Require(rejected && rig.Actions().empty() && first->Status().outcomeUnknown && first->Status().needsReconciliation,
            "Reservation change did not fence hardware and latch UNKNOWN");
    first.reset();
    NativeEccCoordinator next(conflicting,data);
    Require(GetDAQ::ParseStrictJson(next.OwnershipJson()).At("held_at_last_check").text=="true" && rig.Actions().empty(),
            "Reservation release/reacquire did not work without hardware commands");
}
void State(NativeEccCoordinator &ecc,EccState expected,const char *id) {
    const auto s=ecc.Status(); Require(s.state==expected && !s.transition && !s.outcomeUnknown && !s.needsReconciliation && s.operationId==id,"ECC committed status differs");
    const auto op=ecc.Operation(id); Require(op.phase=="complete" && op.to==expected,"ECC journal commit differs");
}
void CoBoPhase(Rig &rig,NativeEccCoordinator &ecc,const NativeEccOptions &options,const std::string &expected,const std::string &expectedSource="live-controller-cache") {
    const auto writesBefore=rig.w.Writes();
    const auto operationBefore=ecc.Status().operationId;
    const auto bytes=ecc.ObserveHardwareStateJson();
    const auto wrapped="{\"instance_id\":"+JsonString(std::string(64,'a'))+","+bytes.substr(1);
    const auto observation=DecodeHardwareStateObservation(wrapped);
    Require(!observation.busy && observation.consistent &&
        observation.coboLifecycleSource==expectedSource && !observation.coboLifecycleIsHardwareReadback &&
        observation.coboLifecycle.size()==options.endpoints.cobos.size(),"CoBo lifecycle observation is incomplete or misrepresented");
    for(const auto &configured:options.endpoints.cobos) {
        const auto found=std::find_if(observation.coboLifecycle.begin(),observation.coboLifecycle.end(),[&](const HardwareStateObservation::CoBoLifecycle &entry) {
            return entry.id==configured.id;
        });
        const auto phase=configured.enabled ? expected : std::string("disabled");
        const auto error="CoBo lifecycle phase differs for "+configured.id+": expected "+phase;
        Require(found!=observation.coboLifecycle.end() && found->state==phase,error.c_str());
    }
    Require(rig.w.Writes()==writesBefore && ecc.Status().operationId==operationBefore,
        "Read-only lifecycle observation wrote hardware or changed the ECC operation");
}
void Unknown(NativeEccCoordinator &ecc,EccState last,const char *id) {
    const auto s=ecc.Status(); Require(s.state==last && s.outcomeUnknown && s.operationId==id && ecc.Operation(id).phase=="unknown","ECC failure did not retain persistent UNKNOWN");
}
std::string Child(const NativeEccOptions &o,const std::string &id,const std::string &name) { return o.auditRoot+"/operations/"+id+'/'+name; }
void Maintenance(Rig &r,NativeEccOptions &o,TestData &data) {
    std::unique_ptr<NativeEccCoordinator> ecc(new NativeEccCoordinator(o,data));
    Rejects([&] {ecc->ReprepareCoBo("too-early","C0");},"current state");
    Rejects([&] {ecc->SetCoBoLed("led-too-early","C0",0,0,1);},"current state");
    ecc->Execute(EccCommand::Describe,"describe");ecc->Execute(EccCommand::Prepare,"prepare");
    const auto before=r.w.Writes();const auto operations=ecc->Operations().size();
    Rejects([&] {ecc->Execute(EccCommand::ReprepareCoBo,"missing-target");},"explicit active node");
    Rejects([&] {ecc->ReprepareCoBo("wrong-target","absent");},"explicit active node");
    data.running=true;
    Rejects([&] {ecc->ReprepareCoBo("maintenance","C0");},"not stopped");
    data.running=false;
    Require(r.w.Writes()==before && ecc->Operations().size()==operations,"Rejected native maintenance wrote hardware or persisted a new operation");
    const auto otherWrites=r.w.c[1].writes;const auto tdc=r.w.tdc.load(),links=r.w.links.load(),multi=r.w.multi.load();
    ecc->ReprepareCoBo("maintenance","C0");State(*ecc,EccState::Prepared,"maintenance");
    Require(ecc->Operation("maintenance").command==11 && r.w.c[1].writes==otherWrites && r.w.tdc==tdc && r.w.links==links && r.w.multi==multi,
            "Native maintenance changed another board or repeated alignment");
    const auto intent=GetDAQPrepareFixture::ReadFile(Child(o,"maintenance","operation.jsonl"));
    Require(intent.find("\"cobo_id\":\"C0\"")!=std::string::npos,"Native maintenance target is missing from its durable intent");
    const auto singleOtherWrites=r.w.c[1].writes,singleDisabledWrites=r.w.c[2].writes,singleBefore=r.w.c[0].writes;
    ecc->ConfigureCoBo("configure-cobo-one","C0");State(*ecc,EccState::Prepared,"configure-cobo-one");
    Require(ecc->Operation("configure-cobo-one").command==17 && r.w.c[0].writes>singleBefore &&
            r.w.c[1].writes==singleOtherWrites && r.w.c[2].writes==singleDisabledWrites,
            "Native single-CoBo Configure was not journaled or wrote another board");
    const auto configureIntent=GetDAQPrepareFixture::ReadFile(Child(o,"configure-cobo-one","operation.jsonl"));
    const auto configureAudit=GetDAQPrepareFixture::ReadFile(Child(o,"configure-cobo-one","maintenance/configure-cobo.jsonl"));
    Require(configureIntent.find("\"cobo_id\":\"C0\"")!=std::string::npos &&
            configureAudit.find("\"system_ready_claimed\":false")!=std::string::npos,
            "Single-CoBo Configure journal omitted target or bounded lifecycle claim");
    const auto writes=r.w.Writes();Rejects([&] {ecc->ReprepareCoBo("maintenance","C1");},"already has an audit directory");
    Require(r.w.Writes()==writes,"Duplicate maintenance ID was replayed");
    const auto ledCalls=r.nodes[0]->ledCalls.load(),otherLedCalls=r.nodes[1]->ledCalls.load();
    const auto ledWrites=r.w.Writes();const auto ledOperations=ecc->Operations().size();data.running=true;
    Rejects([&] {ecc->SetCoBoLed("led-running","C0",0,0,1);},"not stopped");
    data.running=false;
    Rejects([&] {ecc->SetCoBoLed("led-invalid","C0",4,0,1);},"Invalid selected CoBo LED");
    Require(r.nodes[0]->ledCalls==ledCalls && r.w.Writes()==ledWrites && ecc->Operations().size()==ledOperations,
            "Rejected LED maintenance called hardware or entered the journal");
    ecc->SetCoBoLed("led-on","C0",0,HardwareStep::LedP,HardwareStep::LedOn);
    State(*ecc,EccState::Prepared,"led-on");
    Require(ecc->Operation("led-on").command==12 && r.nodes[0]->ledCalls==ledCalls+1 && r.nodes[1]->ledCalls==otherLedCalls,
            "LED operation did not call exactly the selected CoBo once");
    Require(r.nodes[0]->lastLedType==int(get::cobo::LedP) && r.nodes[0]->lastLedAsad==0 &&
            r.nodes[0]->lastLedState==int(get::cobo::LedOn),"LED type/AsAd/state changed in transit");
    const auto ledIntent=GetDAQPrepareFixture::ReadFile(Child(o,"led-on","maintenance/led.jsonl"));
    Require(ledIntent.find("\"asad_id\":0")!=std::string::npos && ledIntent.find("\"led_type\":0")!=std::string::npos &&
            ledIntent.find("\"led_state\":1")!=std::string::npos,"Durable LED intent omitted the exact selection");
    const auto blinkBefore=r.nodes[0]->blinkCalls.load(),otherBlinkBefore=r.nodes[1]->blinkCalls.load();
    const auto blinkWrites=r.w.Writes();data.running=true;
    Rejects([&] {ecc->BlinkCoBoLeds("blink-running","C0");},"not stopped");data.running=false;
    Rejects([&] {ecc->BlinkCoBoLeds("blink-inactive","C2");},"explicit active node");
    Require(r.nodes[0]->blinkCalls==blinkBefore && r.w.Writes()==blinkWrites,"Rejected LED blink reached hardware or changed hardware state");
    ecc->BlinkCoBoLeds("blink-once","C0");State(*ecc,EccState::Prepared,"blink-once");
    Require(ecc->Operation("blink-once").command==14 && r.nodes[0]->blinkCalls==blinkBefore+1 && r.nodes[1]->blinkCalls==otherBlinkBefore,
            "One-shot LED blink did not call only the selected CoBo manager once");
    const auto blinkIntent=GetDAQPrepareFixture::ReadFile(Child(o,"blink-once","maintenance/led-blink.jsonl"));
    Require(blinkIntent.find("\"pattern\":\"sequential-once\"")!=std::string::npos &&
            blinkIntent.find("\"physical_lamp_state_verified\":false")!=std::string::npos,"LED blink audit omitted pattern or overstated lamp verification");
    const auto flipBefore=r.nodes[0]->flipCalls.load(),otherFlipBefore=r.nodes[1]->flipCalls.load();
    const auto flipAllBefore=r.nodes[0]->flipAllCalls.load(),flipWrites=r.w.Writes();data.running=true;
    Rejects([&] {ecc->FlipCoBoLed("flip-running","C0",10);},"not stopped");data.running=false;
    Rejects([&] {ecc->FlipCoBoLed("flip-inactive","C2",10);},"explicit active node");
    Rejects([&] {ecc->FlipCoBoLed("flip-invalid","C0",17);},"0..15 or 16");
    Require(r.nodes[0]->flipCalls==flipBefore && r.nodes[0]->flipAllCalls==flipAllBefore && r.w.Writes()==flipWrites,
            "Rejected LED flip reached hardware or changed hardware state");
    ecc->FlipCoBoLed("flip-bit-10","C0",10);State(*ecc,EccState::Prepared,"flip-bit-10");
    Require(ecc->Operation("flip-bit-10").command==15 && r.nodes[0]->flipCalls==flipBefore+1 && r.nodes[0]->lastFlipLed==10 &&
            r.nodes[0]->flipAllCalls==flipAllBefore && r.nodes[1]->flipCalls==otherFlipBefore,
            "Single LED flip did not target exactly one bit on the selected CoBo");
    const auto flipIntent=GetDAQPrepareFixture::ReadFile(Child(o,"flip-bit-10","maintenance/led-flip.jsonl"));
    Require(flipIntent.find("\"led_id\":10")!=std::string::npos && flipIntent.find("\"automatic_retry\":false")!=std::string::npos &&
            flipIntent.find("\"physical_lamp_state_verified\":false")!=std::string::npos,"LED flip audit omitted target or overstated verification");
    ecc->FlipCoBoLed("flip-all","C0",16);State(*ecc,EccState::Prepared,"flip-all");
    Require(r.nodes[0]->flipCalls==flipBefore+1 && r.nodes[0]->flipAllCalls==flipAllBefore+1 && r.nodes[1]->flipAllCalls==0,
            "All-LED flip did not call exactly the selected manager once");
    const auto pulseCalls=r.nodes[0]->pulseCalls.load(),otherPulseCalls=r.nodes[1]->pulseCalls.load(),pulseWrites=r.w.Writes();data.running=true;
    Rejects([&] {ecc->PulseCoBoLed("pulse-running","C0",10,725);},"not stopped");data.running=false;
    Rejects([&] {ecc->PulseCoBoLed("pulse-inactive","C2",10,725);},"explicit active node");
    Rejects([&] {ecc->PulseCoBoLed("pulse-invalid-led","C0",17,725);},"0..15 or 16");
    Rejects([&] {ecc->PulseCoBoLed("pulse-invalid-period","C0",10,unsigned(std::numeric_limits<int>::max())+1u);},"period_ms in int range");
    Require(r.nodes[0]->pulseCalls==pulseCalls && r.w.Writes()==pulseWrites,"Rejected LED pulse reached hardware or changed journal");
    ecc->PulseCoBoLed("pulse-bit-10","C0",10,725);State(*ecc,EccState::Prepared,"pulse-bit-10");
    Require(ecc->Operation("pulse-bit-10").command==16 && r.nodes[0]->pulseCalls==pulseCalls+1 && r.nodes[1]->pulseCalls==otherPulseCalls &&
            r.nodes[0]->lastPulseLed==10 && r.nodes[0]->lastPulsePeriod==725,"LED pulse did not preserve target/period or ran more than once");
    const auto pulseIntent=GetDAQPrepareFixture::ReadFile(Child(o,"pulse-bit-10","maintenance/led-pulse.jsonl"));
    Require(pulseIntent.find("\"led_id\":10")!=std::string::npos && pulseIntent.find("\"period_ms\":725")!=std::string::npos &&
            pulseIntent.find("\"period_semantics\":\"half-period-ms\"")!=std::string::npos &&
            pulseIntent.find("\"automatic_retry\":false")!=std::string::npos,"LED pulse audit omitted exact command semantics");
    ecc->PulseCoBoLed("pulse-stop","C0",10,0);State(*ecc,EccState::Prepared,"pulse-stop");
    Require(r.nodes[0]->pulseCalls==pulseCalls+2 && r.nodes[0]->lastPulsePeriod==0,"Zero LED pulse period did not reach the board as stop command");
    const auto pulserCalls=r.w.pulserCalls;const auto pulserWrites=r.w.Writes();data.running=true;
    Rejects([&] {ecc->TriggerAsAdPulser("pulser-running","C0",250);},"not stopped");
    data.running=false;
    Rejects([&] {ecc->TriggerAsAdPulser("pulser-invalid","C0",1001);},"-1000..1000");
    Require(r.w.pulserCalls==pulserCalls && r.w.Writes()==pulserWrites,"Rejected pulser trigger reached hardware or changed the journal");
    ecc->TriggerAsAdPulser("pulser-once","C0",250);State(*ecc,EccState::Prepared,"pulser-once");
    Require(ecc->Operation("pulser-once").command==13 && r.w.pulserCalls[0]==pulserCalls[0]+1 &&
            r.w.pulserCalls[1]==pulserCalls[1] && r.w.pulserVoltages[0]==250,
            "Native pulser operation did not preserve selected target, voltage, or one-shot count");
    const auto pulserIntent=GetDAQPrepareFixture::ReadFile(Child(o,"pulser-once","operation.jsonl"));
    Require(pulserIntent.find("\"pulser_voltage_mv\":250")!=std::string::npos &&
            pulserIntent.find("\"cobo_id\":\"C0\"")!=std::string::npos,"Native pulser intent omitted target or voltage");
    for(int cycle=0;cycle<2;++cycle) {
        const auto suffix=std::to_string(cycle);
        ecc->Execute(EccCommand::Configure,"configure-"+suffix);
        ecc->Execute(EccCommand::Start,"start-"+suffix);ecc->Execute(EccCommand::Stop,"stop-"+suffix);
        ecc->Execute(EccCommand::Breakup,"breakup-"+suffix);
        if(!cycle) {ecc->ReprepareCoBo("after-breakup","C1");State(*ecc,EccState::Prepared,"after-breakup");}
    }
    ecc->Execute(EccCommand::Reset,"unprepare");ecc->Execute(EccCommand::Reset,"forget");State(*ecc,EccState::Idle,"forget");
    Require(data.starts==2 && data.finishes==2,"Maintenance replaced normal receiver lifecycle operations");
    ecc.reset();NativeEccCoordinator reopened(o,data);
    Require(reopened.Status().needsReconciliation && reopened.Operation("maintenance").command==11 && reopened.Operation("led-on").command==12 && reopened.Operation("after-breakup").phase=="complete",
            "Reopen lost the maintenance journal or bypassed cold reconciliation");
    Rejects([&] {reopened.ReprepareCoBo("cold-maintenance","C0");},"reconciliation");
    std::cout<<"PASS native ECC selected maintenance, two explicit cycles, journal reopen and cold hold encoding 1."<<r.options.encodingMinor<<std::endl;
}
void Normal(Rig &r,NativeEccOptions &o,TestData &data,std::ofstream &index) {
    const auto original=o;
    std::unique_ptr<NativeEccCoordinator> ecc(new NativeEccCoordinator(o,data));
    Require(ecc->ArtifactSha256()==GetDAQ::ConfigurationSha256(o.experimentArtifactBytes),"Raw artifact digest differs");
    Require(GetDAQPrepareFixture::ReadFile(o.auditRoot+"/experiment.json")==o.experimentArtifactBytes,"Raw immutable artifact bytes were re-encoded");
    // 原始输入变更不能修改已绑定的内部快照和执行参数。
    o.endpoints.cobos[0].configure.asad.generator["periodicModeEnabled"]="false";
    const auto writes=r.w.Writes();
    Rejects([&] { ecc->Execute(EccCommand::Describe,"../escape"); },"operation ID");
    Rejects([&] { ecc->Execute(EccCommand::Start,"illegal-start"); },"current state");
    Require(r.w.Writes()==writes && ecc->Operations().empty(),"Invalid command mutated hardware or consumed an ID");
    ecc->Execute(EccCommand::Describe,"describe-1"); State(*ecc,EccState::Described,"describe-1");
    CoBoPhase(r,*ecc,o,"fresh","controller-baseline");
    const auto described=r.w.Writes(); const auto count=ecc->Operations().size();
    Rejects([&] { ecc->Execute(EccCommand::Prepare,"describe-1"); },"already has an audit directory");
    auto &remote=r.devices[2].at("ctrl")->fConfig; const auto saved=remote;
    for (auto &reg:remote.registers) if (reg.descr.name=="mutantConfig") reg.fields.pop_back();
    Rejects([&] { ecc->Execute(EccCommand::Prepare,"prepare-1"); },"bound description");
    Require(r.w.Writes()==described && ecc->Operations().size()==count && !ecc->Status().outcomeUnknown,"Readonly global preflight consumed ID or changed hardware"); remote=saved;
    data.blockPrepare=true;
    auto preparing=std::async(std::launch::async,[&] { ecc->Execute(EccCommand::Prepare,"prepare-1"); });
    data.WaitForValidation();
    const auto begin=std::chrono::steady_clock::now(); const auto status=ecc->Status();
    Require(status.transition==int(EccCommand::Prepare) && std::chrono::steady_clock::now()-begin<std::chrono::seconds(1),"Status waited for slow validation");
    Rejects([&] { ecc->Execute(EccCommand::Reset,"not-queued"); },"not queued");
    data.Release(); preparing.get(); data.blockPrepare=false; State(*ecc,EccState::Prepared,"prepare-1");
    CoBoPhase(r,*ecc,o,"prepared");
    ecc->Execute(EccCommand::Configure,"configure-1"); State(*ecc,EccState::Ready,"configure-1");
    CoBoPhase(r,*ecc,o,"ready");
    {
        const auto bytes=ecc->ConfigureReadbacksJson();const auto captured=ParseStrictJson(bytes);
        Require(captured.At("phase").text=="captured" && captured.At("configure_operation_id").text=="configure-1" &&
            captured.At("historical").text=="true" && captured.At("actual_loaded_configuration_verified").text=="false", "Configure evidence falsely claims live/full loading");
        unsigned int active=0;for (const auto &c:r.options.cobos) if(c.enabled) ++active;
        Require(captured.At("entries").items.size()==active+r.options.mutants.size(),"Configure evidence omitted active hardware");
        for (const auto &entry:captured.At("entries").items) {
            const auto &evidence=entry.At("evidence");
            Require(evidence.At("audit_sha256").text.size()==64 && evidence.At("audit_chain_sha256").text.size()==64 &&
                evidence.At("checks_matched").text!="0", "Configure evidence lacks digest/checks");
            if (entry.At("stage").text=="cobo-configure") Require(evidence.At("writes_without_full_readback").text!="0","CoBo command/selector writes falsely claim full readback");
        }
        std::ofstream report(o.auditRoot+"/configure-readbacks-test.json");report<<bytes<<'\n';report.flush();Require(bool(report),"Save configure evidence");
    }
    Require(data.running && data.starts==1 && r.Has("C0.sender-connect"),"Ready committed without data lifecycle/connect");
    r.Before("data.receiver-start","C0.sender-connect");
    r.Clear(); ecc->Execute(EccCommand::Start,"start-1"); State(*ecc,EccState::Running,"start-1"); CoBoPhase(r,*ecc,o,"running");
    r.StartOrder(false); r.Index(index,Child(o,"start-1","start"),"start");
    r.Clear(); ecc->Execute(EccCommand::Pause,"pause-1"); State(*ecc,EccState::Paused,"pause-1"); CoBoPhase(r,*ecc,o,"paused");
    r.StopOrder(); r.Index(index,Child(o,"pause-1","pause"),"pause");
    r.Clear(); ecc->Execute(EccCommand::Resume,"resume-1"); State(*ecc,EccState::Running,"resume-1"); CoBoPhase(r,*ecc,o,"running");
    r.StartOrder(true); r.Index(index,Child(o,"resume-1","resume"),"resume");
    r.Clear(); ecc->Execute(EccCommand::Stop,"stop-1"); State(*ecc,EccState::Ready,"stop-1"); CoBoPhase(r,*ecc,o,"stopped");
    r.StopOrder(); r.Index(index,Child(o,"stop-1","stop"),"stop");
    Require(data.running && data.finishes==0 && data.starts==1,"Stop restarted/finished receiver");
    r.Clear(); ecc->Execute(EccCommand::Start,"start-2"); State(*ecc,EccState::Running,"start-2"); CoBoPhase(r,*ecc,o,"running");
    r.StartOrder(false); r.Index(index,Child(o,"start-2","start"),"start");
    for (const auto &id:{"C0","C1","M0","M1"}) Require(!r.Has(std::string(id)+".sender-connect") && !r.Has(std::string(id)+".sender-disconnect"),"Restart replaced retained connection");
    r.Clear(); ecc->Execute(EccCommand::Stop,"stop-2"); CoBoPhase(r,*ecc,o,"stopped"); r.StopOrder(); r.Index(index,Child(o,"stop-2","stop"),"stop");
    r.Clear(); ecc->Execute(EccCommand::Breakup,"breakup-1"); State(*ecc,EccState::Prepared,"breakup-1"); CoBoPhase(r,*ecc,o,"prepared");
    Require(!data.running && data.finishes==1,"Breakup did not wait for data lifecycle finish");
    r.Before("M1.sender-disconnect","data.receiver-finish");
    for (unsigned int i=0;i<2;++i) Require(r.w.alarms[i]->unsubscribes==0,"Breakup lost owned subscriptions");
    ecc->Execute(EccCommand::Configure,"configure-2"); State(*ecc,EccState::Ready,"configure-2"); CoBoPhase(r,*ecc,o,"ready");
    Require(ParseStrictJson(ecc->ConfigureReadbacksJson()).At("configure_operation_id").text=="configure-2","New Configure reused old receipt");
    Require(data.starts==2,"Reconfigure did not create a new data epoch");
    ecc->Execute(EccCommand::Breakup,"breakup-2"); State(*ecc,EccState::Prepared,"breakup-2"); CoBoPhase(r,*ecc,o,"prepared");
    ecc->Execute(EccCommand::Reset,"reset-prepared"); State(*ecc,EccState::Described,"reset-prepared"); CoBoPhase(r,*ecc,o,"fresh","controller-baseline");
    for (unsigned int i=0;i<2;++i) Require(r.w.alarms[i]->unsubscribes==1,"Reset did not close exactly its own alarm subscription");
    for (unsigned int i=0;i<3;++i) Require(r.w.c[i].powered==0,"Reset left CoBo/AsAd powered");
    const auto before=r.w.Writes(); r.Clear(); ecc->Execute(EccCommand::Reset,"reset-described"); State(*ecc,EccState::Idle,"reset-described");
    Require(r.w.Writes()==before && r.Actions().empty() && r.nodes[0]->fDevices.size()==o.description.nodes[0].description.devices.size(),"Described Reset destroyed remote topology");
    for (const auto &op:ecc->Operations()) {
        const auto root=GetDAQPrepareFixture::ReadFile(o.auditRoot+"/operations/"+op.id+"/operation.jsonl");
        Require(root.find("\"operation_id\":"+JsonString(op.id))!=std::string::npos && root.find("\"request_key\":"+JsonString(op.requestKey))!=std::string::npos &&
            root.find("\"snapshot_sha256\":"+JsonString(ecc->SnapshotSha256()))!=std::string::npos && root.find("backend_complete")!=std::string::npos,"Operation context is not bound to native child audits");
    }
    ecc.reset(); o=original;
    NativeEccCoordinator reopened(o,data);
    Require(reopened.Status().state==EccState::Idle && reopened.Status().needsReconciliation,"Restart cleared startup reconciliation gate");
    CoBoPhase(r,reopened,o,"unknown","controller-baseline");
    Rejects([&] { reopened.Execute(EccCommand::Describe,"restart-describe"); },"reconciliation");
    Rejects([&] { reopened.Reconcile("restart-recover",EccState::Idle,"operator assertion"); },"same original failure");
    Require(r.w.Writes()==before && reopened.Status().needsReconciliation,"Restart/recovery sent hardware writes or accepted assertion as evidence");
}
void DescriptionRecovery(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario)
{
    const bool deny=scenario.find("denial")!=std::string::npos;
    const bool auditFault=scenario.find("audit")!=std::string::npos;
    auto ecc=std::unique_ptr<NativeEccCoordinator>(new NativeEccCoordinator(o,data));
    r.devices[0].at("ctrl")->failStatusClear=true;
    Rejects([&]{ecc->Execute(EccCommand::Describe,"failed-describe");},"UNKNOWN");
    r.devices[0].at("ctrl")->failStatusClear=false;
    Require(ecc->Status().state==EccState::Idle && ecc->Status().outcomeUnknown,"Describe fault was not held");
    const auto writes=r.w.Writes();
    if(auditFault) {
        std::ofstream changed(o.auditRoot+"/operations/failed-describe/operation.jsonl",std::ios::app);changed<<" ";changed.close();
        Rejects([&]{ecc->ReconcileDescription("audit-recovery","failed-describe");},"immutable file");
        Require(ecc->Status().outcomeUnknown && ecc->Status().needsReconciliation && r.w.Writes()==writes,"Corrupt audit allowed recovery");return;
    }
    if(deny) {
        {std::lock_guard<std::mutex> lock(r.w.c[1].mutex);r.w.c[1].Set("mutantConfig","initDone",1);}
        Rejects([&]{ecc->ReconcileDescription("not-ready-recovery","failed-describe");},"expectation mismatch");
        Require(ecc->Status().outcomeUnknown && r.w.Writes()==writes,"Ready board recovered without repair");
        {std::lock_guard<std::mutex> lock(r.w.c[1].mutex);r.w.c[1].Set("mutantConfig","initDone",0);}
        data.running=true;Rejects([&]{ecc->ReconcileDescription("running-data-recovery","failed-describe");},"not stopped");data.running=false;
        Rejects([&]{ecc->ReconcileDescription("wrong-id-recovery","unrelated");},"same live failed Describe");
    }
    ecc->ReconcileDescription("verified-description","failed-describe");
    Require(ecc->Status().state==EccState::Described && !ecc->Status().outcomeUnknown && !ecc->Status().needsReconciliation && r.w.Writes()==writes,"Description verification failed to commit without writes");
    Require(ecc->Operation("failed-describe").phase=="unknown" && ecc->Operation("verified-description").command==10,"Recovery rewrote original history");
    Rejects([&]{ecc->ReconcileDescription("duplicate-recovery","failed-describe");},"same live failed Describe");
    if(!deny) {ecc->Execute(EccCommand::Prepare,"prepare-after-recovery");
        Require(ecc->Status().state==EccState::Prepared,"Verified description did not support original preparation workflow");}
    ecc.reset();NativeEccCoordinator cold(o,data);
    Rejects([&]{cold.ReconcileDescription("cold-recovery","failed-describe");},"same live failed Describe");
    Require(cold.Status().needsReconciliation,"Cold instance cleared recovery gate");
}
void Fault(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data); ecc.Execute(EccCommand::Describe,"describe"); ecc.Execute(EccCommand::Prepare,"prepare");
    if (scenario=="fault-reset") {
        r.devices[0].at("ctrl")->failPowerOff=true;
        Rejects([&] { ecc.Execute(EccCommand::Reset,"reset"); },"Unprepare incomplete"); Unknown(ecc,EccState::Prepared,"reset");
        Require(r.w.c[0].powered==0 && r.w.c[1].powered==0 && r.w.alarms[0]->unsubscribes==0 && r.w.alarms[1]->unsubscribes==0,"Reset ACK loss retried/left other power or cleaned alarms");
    } else {
        if (scenario=="fault-receiver-start") {
            data.failStart=true;
            Rejects([&] { ecc.Execute(EccCommand::Configure,"configure"); },"receiver start failure"); Unknown(ecc,EccState::Prepared,"configure");
            Require(!r.Has("C0.sender-connect") && data.starts==1,"Receiver failure connected a sender or retried launch");
            r.Clear(); ecc.StopProductionAfterFailure("negative-before-acquisition");
            Require(ecc.Status().outcomeUnknown && r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && !r.Has("C0.sender-stop"),
                "Configure failure without acquisition could not stop production");
        } else {
            ecc.Execute(EccCommand::Configure,"configure"); r.Clear();
            if (scenario=="fault-start") {
                r.sender["C0"]->failStart=true;
                Rejects([&] { ecc.Execute(EccCommand::Start,"start"); },"UNKNOWN"); Unknown(ecc,EccState::Ready,"start");
                Require(r.Count("C0.sender-start")==1 && !r.Has("C1.sender-start") && !r.Has("C0.ctrl.pipeCtrl.firmwareEnable=1"),"Start ACK loss replayed or continued");
            } else if (scenario=="fault-stop-receiver") {
                ecc.Execute(EccCommand::Start,"start"); r.Clear(); data.running=false;
                Rejects([&] { ecc.Execute(EccCommand::Stop,"stop"); },"not ready"); Unknown(ecc,EccState::Running,"stop");
                r.StopOrder(); r.Clear(); ecc.StopProductionAfterFailure("negative-after-stopped-child");
                Require(r.Actions().empty() && ecc.Status().outcomeUnknown,"Parent UNKNOWN replayed successfully stopped child production");
            } else if (scenario=="fault-operation-audit") {
                r.w.m[0].onAction=[&](const std::string &s) {
                    r.w.Record("M0."+s);
                    if (s=="CARD_B_EMC/TRIGGER_CTRL/run=1") Require(::unlink(Child(o,"start","operation.jsonl").c_str())==0,"Inject post-start operation log loss");
                };
                Rejects([&] { ecc.Execute(EccCommand::Start,"start"); },"audit identity"); Unknown(ecc,EccState::Ready,"start");
                Require(r.Has("M0.CARD_B_EMC/TRIGGER_CTRL/run=1"),"Operation audit loss did not occur after actual native Start");
                r.Clear(); ecc.StopProductionAfterFailure("negative-after-running-child");
                Require(ecc.Status().outcomeUnknown && r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && !r.Has("C0.sender-stop"),"Parent UNKNOWN could not stop Running child without touching sender");
            } else {
                data.failFinish=true;
                Rejects([&] { ecc.Execute(EccCommand::Breakup,"breakup"); },"receiver seal failure"); Unknown(ecc,EccState::Ready,"breakup");
                Require(data.running && data.finishes==1 && r.Count("M1.sender-disconnect")==1,"Seal failure did not preserve data/one-shot disconnect");
                for (unsigned int i=0;i<3;++i) Require(r.w.c[i].Get("mutantConfig","initDone")==0,"Breakup native child did not finish before seal failure");
            }
        }
    }
    const auto actions=r.Actions().size(); const auto writes=r.w.Writes();
    Rejects([&] { ecc.Execute(EccCommand::Reset,"retry-reset"); },"UNKNOWN");
    Rejects([&] { ecc.Reconcile("assertion-recovery",EccState::Idle,"claimed stopped"); },"same original failure");
    Require(r.Actions().size()==actions && r.w.Writes()==writes && ecc.Status().outcomeUnknown,"Unknown was replayed/cleaned by new ID or unverified recovery");
    if (scenario=="fault-start") {
        r.Clear(); ecc.StopProductionAfterFailure("explicit-negative-stop");
        Require(ecc.Status().outcomeUnknown && ecc.Operation("start").phase=="unknown" && r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 &&
            !r.Has("C0.sender-stop") && !r.Has("C0.sender-disconnect"),"Explicit failure stop changed unknown sender/ECC or skipped producer stop");
        r.Clear(); ecc.StopProductionAfterFailure("explicit-negative-stop-2"); Require(r.Actions().empty(),"Negative stop replayed previously attempted production stages");
        Rejects([&] { ecc.StopProductionAfterFailure("explicit-negative-stop"); },"already has an audit directory");
    }
}
void PreparationFault(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data); ecc.Execute(EccCommand::Describe,"describe");
    bool sent=false;
    r.w.c[1].onAction=[&](const std::string &s) {
        r.w.Record("C1."+s);
        if (!sent) { sent=true; r.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); }
    };
    Rejects([&] { ecc.Execute(EccCommand::Prepare,"prepare"); },"alarm interlock");
    Unknown(ecc,EccState::Described,"prepare");
    r.w.c[1].onAction=[&](const std::string &s) { r.w.Record("C1."+s); };
    Require(r.w.alarms[0]->subscribes==1 && !r.w.alarms[0]->unsubscribes && !data.starts,"Partial Prepare lost owner or started receiver");
    const bool stopLoss=scenario=="failure-unprepare-stop-loss" || scenario=="fault-prepare-stop-loss",nameFault=scenario=="fault-prepare-stop-name";
    const bool auditFault=scenario=="fault-negative-audit",directoryFault=scenario=="fault-negative-directory";
    if(scenario.find("failure-unprepare-")==0) {
        const auto before=r.w.Writes();Rejects([&]{ecc.UnprepareAfterFailure("without-stop-proof");},"confirmed production stop");
        Require(r.w.Writes()==before,"Missing stop proof caused cleanup writes");
        Rejects([&]{ecc.ReconcilePreparation("recover-before-cleanup","prepare");},"same live failed Prepare");
    }
    const auto negative=Child(o,"negative-prepare",""); const auto operation=negative+"operation.jsonl";
    bool replaced=false;
    if (auditFault || directoryFault) r.w.c[1].onAction=[&](const std::string &s) {
        r.w.Record("C1."+s);
        if (!replaced && s=="ctrl.pipeCtrl.intEnable=0") {
            replaced=true;
            if (auditFault) {
                Require(!::rename(operation.c_str(),(operation+".original").c_str()),"Move negative intent while final hardware stage completes");
                std::ofstream replacement(operation); replacement<<"replaced intent\n"; replacement.flush(); Require(bool(replacement),"Replace negative intent");
            } else {
                const auto directory=negative.substr(0,negative.size()-1);
                Require(!::rename(directory.c_str(),(directory+".original").c_str()),"Move negative operation directory");
                CreateHardwareAuditDirectory(directory);
            }
        }
    };
    if (stopLoss) r.devices[0].at("ctrl")->failFirmwareStop=true;
    if (nameFault) r.nodes[0]->fName="changed-after-prepare-failure";
    r.Clear();
    if (auditFault || directoryFault) Rejects([&] { ecc.StopProductionAfterFailure("negative-prepare"); },"identity");
    else if (stopLoss || nameFault) Rejects([&] { ecc.StopProductionAfterFailure("negative-prepare"); },"stop unconfirmed");
    else ecc.StopProductionAfterFailure("negative-prepare");
    Unknown(ecc,EccState::Described,"prepare");
    r.Before("M0.CARD_B_EMC/TRIGGER_CTRL/run=0","M1.CARD_B_EMC/TRIGGER_CTRL/run=0");
    r.Before("M1.CARD_B_EMC/TRIGGER_CTRL/run=0","M2.CARD_B_EMC/TRIGGER_CTRL/run=0");
    Require(r.Count("C1.ctrl.pipeCtrl.firmwareEnable=0")==1 && r.Count("C1.ctrl.pipeCtrl.intEnable=0")==1 &&
        !r.Has("C2.pulser-stop") && !r.Has("C0.sender-stop") && !r.Has("C0.sender-disconnect") && !data.starts && !data.finishes,
        "Preparation negative stop skipped independent board or touched data lifecycle");
    for (unsigned int i=0;i<2;++i) Require(!r.w.alarms[i]->unsubscribes,"Negative stop unsubscribed owned alarms");
    if (nameFault) Require(!r.Has("C0.pulser-stop") && !r.Has("C0.ctrl.pipeCtrl.firmwareEnable=0"),"Changed node received stop writes");
    else Require(r.Count("C0.ctrl.pipeCtrl.firmwareEnable=0")==1 && r.Count("C0.ctrl.pipeCtrl.intEnable=0")==1,"Stop failure aborted sibling field");
    if (auditFault || directoryFault) {
        Require(replaced && ecc.Status().needsReconciliation,"Negative audit replacement did not latch storage fault");
        const auto directory=negative.substr(0,negative.size()-1);
        if (auditFault) {
            Require(!::unlink(operation.c_str()) && !::rename((operation+".original").c_str(),operation.c_str()),"Restore original negative intent identity");
        } else {
            Require(!::rename(directory.c_str(),(directory+".replacement").c_str()) && !::rename((directory+".original").c_str(),directory.c_str()),"Restore negative operation directory identity");
        }
        r.Clear(); Rejects([&] { ecc.StopProductionAfterFailure("negative-after-restoration"); },"latched");
        Require(r.Actions().empty() && ecc.Status().outcomeUnknown && ecc.Status().needsReconciliation,"Restored negative audit cleared gate or allowed replay");
        return;
    }
    const auto audit=GetDAQPrepareFixture::ReadFile(Child(o,"negative-prepare","operation.jsonl"));
    Require(audit.find("\"retained_object\":\"preparation\"")!=std::string::npos && audit.find("\"ecc_unknown_preserved\":true")!=std::string::npos,
        "Preparation stop audit did not bind retained parent");
    if(scenario.find("failure-unprepare-")==0) {
        r.Clear();
        if(stopLoss) {Rejects([&]{ecc.UnprepareAfterFailure("unconfirmed-stop-cleanup");},"confirmed production stop");Require(r.Actions().empty(),"Unconfirmed stop permitted power-off");return;}
        const bool powerLoss=scenario=="failure-unprepare-power-loss",monitorFailure=scenario=="failure-unprepare-monitor-fault";
        r.devices[0].at("ctrl")->failPowerOff=powerLoss;r.devices[0].at("asad")->failMonitoring=monitorFailure;
        if(powerLoss || monitorFailure)Rejects([&]{ecc.UnprepareAfterFailure("cleanup");},"incomplete");
        else ecc.UnprepareAfterFailure("cleanup");
        Require(ecc.Status().outcomeUnknown && ecc.Status().state==EccState::Described && !data.starts && !data.finishes,"Cleanup cleared parent or changed data");
        const auto lifecycle=DecodeHardwareStateObservation("{\"instance_id\":\""+std::string(64,'a')+"\","+ecc.ObserveHardwareStateJson().substr(1));
        for(const auto &board:lifecycle.coboLifecycle) {
            const auto configured=std::find_if(o.endpoints.cobos.begin(),o.endpoints.cobos.end(),[&](const SystemCoBoSettings &candidate){return candidate.id==board.id;});
            Require(configured!=o.endpoints.cobos.end(),"Cleanup observation added an unbound CoBo");
            const auto expected=!configured->enabled ? "disabled" : powerLoss || monitorFailure ? "unknown" : "fresh";
            Require(board.state==expected,"Failure-unprepare CoBo lifecycle differs from confirmed cleanup/readback outcome");
        }
        for(unsigned int i=0;i<3;++i)Require(r.w.c[i].powered==0,"Cleanup skipped independent declared board power-off");
        if(powerLoss || monitorFailure)Require(r.w.alarms[0]->unsubscribes==0,"Unconfirmed cleanup canceled owned alarm");
        else Require(r.w.alarms[0]->unsubscribes==1,"Successful cleanup did not close original subscription");
        r.devices[0].at("ctrl")->failPowerOff=false;r.devices[0].at("asad")->failMonitoring=false;r.Clear();
        Rejects([&]{ecc.UnprepareAfterFailure("another-cleanup-id");},"no Run/cold adoption or replay");Require(r.Actions().empty(),"New cleanup ID repeated an uncertain action");
        if(scenario.find("failure-unprepare-recover-")==0) {
            const bool auditDamage=scenario=="failure-unprepare-recover-audit";
            const bool powerChanged=scenario=="failure-unprepare-recover-power";
            const bool manifestChanged=scenario=="failure-unprepare-recover-manifest" || scenario=="failure-unprepare-recover-manifest-final";
            const auto tamperAt=scenario=="failure-unprepare-recover-manifest-final" ? 3u : 2u;
            if(manifestChanged) {data.stoppedChecks=0;data.onStopped=[&](unsigned int count){if(count==tamperAt){std::ofstream changed(Child(o,"recover-prepare","observation-manifest.json"),std::ios::app);changed<<"{}\n";}};}
            if(auditDamage) {std::ofstream file(Child(o,"cleanup","failure-unprepare/step-0.jsonl"),std::ios::app);file<<"{}\n";}
            if(powerChanged)r.w.c[0].powered=1;
            if(auditDamage || powerChanged || manifestChanged) {
                Rejects([&]{ecc.ReconcilePreparation("recover-prepare","prepare");},(auditDamage || manifestChanged) ? "immutable file" : "expectation mismatch");
                Require(ecc.Status().outcomeUnknown && r.Actions().empty(),"Rejected recovery wrote hardware or cleared UNKNOWN");
            } else {
                const auto writes=r.w.Writes();ecc.ReconcilePreparation("recover-prepare","prepare");
                Require(!ecc.Status().outcomeUnknown && !ecc.Status().needsReconciliation && ecc.Status().state==EccState::Described &&
                    ecc.Status().operationId=="recover-prepare" && r.w.Writes()==writes && !data.starts && !data.finishes,"Recovery failed or wrote hardware/data");
                Require(ecc.Operation("prepare").phase=="unknown","Recovery replaced original failed history");
                const auto recovery=ecc.Operation("recover-prepare");Require(recovery.evidence.size()<=4096 && recovery.evidence.find("observation_manifest_sha256")!=std::string::npos,"Recovery proof is not bounded by the journal budget");
                const auto manifest=GetDAQPrepareFixture::ReadFile(Child(o,"recover-prepare","observation-manifest.json"));Require(!manifest.empty() && recovery.evidence.find(ConfigurationSha256(manifest))!=std::string::npos,"Recovery did not bind the complete observation manifest");
                Rejects([&]{ecc.ReconcilePreparation("again-recover","prepare");},"same live failed Prepare");
                ecc.Execute(EccCommand::Prepare,"fresh-prepare");
                Require(ecc.Status().state==EccState::Prepared && !ecc.Status().outcomeUnknown,"Recovery did not allow a fresh Prepare");
            }
        }return;
    }
    r.devices[0].at("ctrl")->failFirmwareStop=false; r.nodes[0]->fName="CoBo0"; r.Clear();
    if (stopLoss || nameFault) Rejects([&] { ecc.StopProductionAfterFailure("negative-prepare-again"); },"stop unconfirmed");
    else ecc.StopProductionAfterFailure("negative-prepare-again");
    Require(r.Actions().empty(),"New operation ID replayed preparation stop after restoration");
    Rejects([&] { ecc.StopProductionAfterFailure("negative-prepare"); },"already has an audit directory");
    Rejects([&] { ecc.Execute(EccCommand::Reset,"unverified-reset"); },"UNKNOWN");
}
void FailureBreakup(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario)
{
    NativeEccCoordinator ecc(o,data);ecc.Execute(EccCommand::Describe,"describe");ecc.Execute(EccCommand::Prepare,"prepare");
    ecc.Execute(EccCommand::Configure,"configure");ecc.Execute(EccCommand::Start,"start");
    const bool broken=scenario=="failure-breakup-broken";
    const bool unknown=scenario=="failure-breakup-unknown";
    if(broken){data.failFinish=true;Rejects([&]{ecc.Execute(EccCommand::Breakup,"original-breakup");},"seal failure");}
    else if(unknown){r.sender["C0"]->failStop=true;Rejects([&]{ecc.Execute(EccCommand::Pause,"original-pause");},"sender flush incomplete");r.sender["C0"]->failStop=false;}
    else ecc.HoldReceiverFailure("start");
    const auto original=ecc.Status().operationId;const auto from=ecc.Status().state;
    r.Clear();Rejects([&]{ecc.BreakupAfterFailure("without-stop-proof");},"confirmed original production stop");Require(r.Actions().empty(),"Failure Breakup ran before stop proof");
    ecc.StopProductionAfterFailure("negative-stop");r.Clear();
    const auto starts=data.starts,finishes=data.finishes;
    const bool stopLoss=scenario=="failure-breakup-stop-loss",disconnectLoss=scenario=="failure-breakup-disconnect-loss";
    r.sender["C0"]->failStop=stopLoss;r.sender["C0"]->failDisconnect=disconnectLoss;
    if(unknown) {
        Rejects([&]{ecc.BreakupAfterFailure("failure-cleanup");},"unknown sender");Require(r.Actions().empty(),"Unknown sender preflight caused actions");
    } else if(stopLoss || disconnectLoss) {
        Rejects([&]{ecc.BreakupAfterFailure("failure-cleanup");},"unconfirmed");
        Require(r.Count("C0.sender-stop")==1 && r.Count("C1.sender-stop")==1 && r.Count("M0.sender-stop")==1,"Failed cleanup skipped independent stop targets");
        Require(r.Count("C0.sender-disconnect")==unsigned(disconnectLoss) && r.Count("C1.sender-disconnect")==1 && r.Count("M0.sender-disconnect")==1,"Failed cleanup skipped independent disconnect or closed unknown source");
    } else {
        ecc.BreakupAfterFailure("failure-cleanup");
        if(broken)Require(r.Actions().empty(),"Already Broken cleanup replayed hardware/senders");
        else Require(r.Count("C0.sender-stop")==1 && r.Count("C1.sender-stop")==1 && r.Count("M0.sender-stop")==1 &&
            r.Count("C0.sender-disconnect")==1 && r.Count("C1.sender-disconnect")==1 && r.Count("M0.sender-disconnect")==1 && r.Count("M1.sender-disconnect")==1,"Cleanup failed to stop/close original known senders");
        for(const auto &c:o.endpoints.cobos)Require(r.w.c[std::size_t(c.id.back()-'0')].Get("mutantConfig","initDone")==0,"Cleanup left CoBo ready");
        const auto audit=GetDAQPrepareFixture::ReadFile(Child(o,"failure-cleanup","operation.jsonl"));
        Require(audit.find("\"retained_acquisition_broken\":true")!=std::string::npos && audit.find("\"receiver_lifecycle_changed\":false")!=std::string::npos,"Failure cleanup proof lacks retained/data scope");
    }
    Require(ecc.Status().outcomeUnknown && ecc.Status().state==from && ecc.Status().operationId==original && data.starts==starts && data.finishes==finishes && data.running,"Failure cleanup cleared parent or changed/sealed receivers");
    r.sender["C0"]->failStop=r.sender["C0"]->failDisconnect=false;r.Clear();
    Rejects([&]{ecc.BreakupAfterFailure("another-cleanup");},"no replay or cold takeover");Require(r.Actions().empty(),"Another cleanup ID replayed an uncertain action");
}
void ReceiverRetirement(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data);ecc.Execute(EccCommand::Describe,"describe");ecc.Execute(EccCommand::Prepare,"prepare");
    ecc.Execute(EccCommand::Configure,"configure");ecc.Execute(EccCommand::Start,"start");ecc.HoldReceiverFailure("start");
    r.Clear();Rejects([&]{ecc.RetireFailedReceivers("without-stop");},"same failed acquisition");Require(data.retirements==0 && r.Actions().empty(),"Missing stop touched data/hardware");
    ecc.StopProductionAfterFailure("production-stop");ecc.BreakupAfterFailure("connections");
    r.Clear();Rejects([&]{ecc.RetireFailedReceivers("without-abort");},"receiver Abort proof");Require(data.retirements==0 && r.Actions().empty(),"Missing Abort performed retirement");
    ecc.AbortFailedReceivers("receiver-abort");const auto original=ecc.Status();
    const bool oldStop=scenario=="receiver-retirement-old-stop",freshStop=scenario=="receiver-retirement-fresh-stop";
    if(oldStop || freshStop)ecc.StopProductionAfterFailure("fresh-production-stop");
    const bool audit=scenario=="receiver-retirement-audit",connection=scenario=="receiver-retirement-connections";
    const bool failure=scenario=="receiver-retirement-loss",during=scenario=="receiver-retirement-final-audit";
    const auto path=Child(o,connection ? "connections" : "receiver-abort","operation.jsonl");
    if(audit || connection) {std::ofstream file(path,std::ios::app);file<<" ";}
    if(oldStop) {std::ofstream file(Child(o,"production-stop","operation.jsonl"),std::ios::app);file<<" ";}
    if(during)data.onStopped=[&](unsigned int){std::ofstream file(path,std::ios::app);file<<" ";};
    data.failRetirement=failure;r.Clear();
    if(audit || connection || during || oldStop)Rejects([&]{ecc.RetireFailedReceivers("retirement");},"changed");
    else if(failure)Rejects([&]{ecc.RetireFailedReceivers("retirement");},"Injected data retirement failure");
    else ecc.RetireFailedReceivers("retirement");
    Require(r.Actions().empty() && data.aborts==1 && data.starts==1 && data.finishes==0 && !data.running,"Retirement touched hardware or restarted/sealed data");
    Require(ecc.Status().outcomeUnknown && ecc.Status().state==original.state && ecc.Status().operationId==original.operationId,"Retirement cleared/rebound original parent UNKNOWN");
    Require(data.retirements==unsigned(!(audit || connection || oldStop)),"Retirement did not respect audit preflight");
    if(!(audit || connection || during || oldStop))Rejects([&]{ecc.RetireFailedReceivers("another-retirement");},"no replay or cold takeover");
    else Rejects([&]{ecc.Execute(EccCommand::Reset,"reset-held");},"reconciliation");
}
void AcquisitionUnprepare(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data);ecc.Execute(EccCommand::Describe,"describe");ecc.Execute(EccCommand::Prepare,"prepare");
    ecc.Execute(EccCommand::Configure,"configure");ecc.Execute(EccCommand::Start,"start");ecc.HoldReceiverFailure("start");
    ecc.StopProductionAfterFailure("production-stop");ecc.BreakupAfterFailure("connections");ecc.AbortFailedReceivers("receiver-abort");
    r.Clear();Rejects([&]{ecc.UnprepareAcquisitionAfterFailure("without-retirement");},"data retirement proof");Require(r.Actions().empty(),"Missing retirement proof permitted power-off");
    ecc.RetireFailedReceivers("retirement");const auto original=ecc.Status();
    const bool powerLoss=scenario=="acquisition-unprepare-power-loss",monitorFault=scenario=="acquisition-unprepare-monitor-fault";
    const bool auditFault=scenario=="acquisition-unprepare-audit",lateFault=scenario=="acquisition-unprepare-late-audit";
    const auto retirementFile=Child(o,"retirement","operation.jsonl");
    if(auditFault){std::ofstream file(retirementFile,std::ios::app);file<<" ";}
    const auto beforeStopped=data.stoppedChecks;
    if(lateFault)data.onStopped=[&](unsigned int n){if(n==beforeStopped+2){std::ofstream file(retirementFile,std::ios::app);file<<" ";}};
    r.devices[0].at("ctrl")->failPowerOff=powerLoss;r.devices[0].at("asad")->failMonitoring=monitorFault;r.Clear();
    if(auditFault || lateFault)Rejects([&]{ecc.UnprepareAcquisitionAfterFailure("power-off");},"changed");
    else if(powerLoss || monitorFault)Rejects([&]{ecc.UnprepareAcquisitionAfterFailure("power-off");},"incomplete");
    else ecc.UnprepareAcquisitionAfterFailure("power-off");
    Require(ecc.Status().outcomeUnknown && ecc.Status().state==original.state && ecc.Status().operationId==original.operationId,"Unprepare cleared/rebound parent UNKNOWN");
    Require(data.starts==1 && data.finishes==0 && data.aborts==1 && data.retirements==1 && !data.running,"Unprepare restarted/sealed/modified data lifecycle");
    if(auditFault)Require(r.Actions().empty(),"Bad retirement audit touched hardware");
    else for(unsigned int i=0;i<3;++i)Require(r.w.c[i].powered==0,"Unprepare skipped independent declared board power-off");
    if(powerLoss || monitorFault)Require(r.w.alarms[0]->unsubscribes==0,"Unconfirmed power-off closed owned alarm");
    else if(!auditFault)Require(r.w.alarms[0]->unsubscribes==1,"Confirmed power-off did not close original subscription");
    if(!(powerLoss || monitorFault || auditFault || lateFault)) {
        const auto bytes=GetDAQPrepareFixture::ReadFile(Child(o,"power-off","operation.jsonl"));
        Require(bytes.find("all_power_off_observed")!=std::string::npos && bytes.find("owned_subscriptions_closed")!=std::string::npos,"Missing actual complete unprepare proof");
    }
    r.devices[0].at("ctrl")->failPowerOff=false;r.devices[0].at("asad")->failMonitoring=false;r.Clear();
    if(auditFault || lateFault)Rejects([&]{ecc.Execute(EccCommand::Reset,"reset-held");},"reconciliation");
    else Rejects([&]{ecc.UnprepareAcquisitionAfterFailure("repeat-power-off");},"no replay or cold takeover");
    Require(r.Actions().empty(),"Unknown/incomplete cleanup replayed hardware action");
}
void AcquisitionRecovery(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data);
    ecc.Execute(EccCommand::Describe,"describe");ecc.Execute(EccCommand::Prepare,"prepare");
    ecc.Execute(EccCommand::Configure,"configure");ecc.Execute(EccCommand::Start,"start");
    ecc.HoldReceiverFailure("start");
    Require(ecc.Operation("start").phase=="complete","Receiver failure rewrote confirmed Start history");
    Rejects([&]{ecc.ReconcileAcquisition("premature-recovery","start");},"same original");
    ecc.StopProductionAfterFailure("production-stop");ecc.BreakupAfterFailure("connections");
    ecc.AbortFailedReceivers("receiver-abort");ecc.RetireFailedReceivers("retirement");
    ecc.UnprepareAcquisitionAfterFailure("power-off");
    const auto writes=r.w.Writes();
    Rejects([&]{ecc.ReconcileAcquisition("wrong-original","another-start");},"same original");
    if(scenario=="acquisition-recovery-input-0" || scenario=="acquisition-recovery-input-1") {
        ecc.HoldControlInputFailure("start");
        for(const auto &id:{"input-held-recovery","input-held-recovery-again"})
            Rejects([&]{ecc.ReconcileAcquisition(id,"start");},"same original");
        Require(ecc.Status().outcomeUnknown && ecc.Status().needsReconciliation &&
                ecc.Status().operationId=="start" && ecc.Operation("start").phase=="complete" &&
                r.w.Writes()==writes && data.starts==1 && data.finishes==0,
                "Acquisition recovery cleared an independent input hold or replayed hardware/data");
        return;
    }
    const bool audit=scenario=="acquisition-recovery-audit";
    const bool late=scenario=="acquisition-recovery-late-audit";
    const auto proof=Child(o,"power-off","operation.jsonl");
    if(audit){std::ofstream file(proof,std::ios::app);file<<" ";}
    if(late){const auto before=data.stoppedChecks;data.onStopped=[&,before](unsigned int n){
        if(n==before+3){std::ofstream file(proof,std::ios::app);file<<" ";}};}
    if(audit || late) {
        Rejects([&]{ecc.ReconcileAcquisition("verified-acquisition","start");},"changed");
        Require(ecc.Status().outcomeUnknown && ecc.Status().needsReconciliation && r.w.Writes()==writes,
                "Changed cleanup proof cleared the original acquisition fence or wrote hardware");
        return;
    }
    ecc.ReconcileAcquisition("verified-acquisition","start");
    Require(ecc.Status().state==EccState::Described && !ecc.Status().transition &&
            ecc.Status().operationId=="verified-acquisition" &&
            ecc.Operation("verified-acquisition").phase=="reconciled",
            "Acquisition recovery did not persist the distinct reconciliation result");
    Require(!ecc.Status().outcomeUnknown && !ecc.Status().needsReconciliation && r.w.Writes()==writes,
            "Acquisition reconciliation failed to clear only the verified original hold without writes");
    Require(ecc.Operation("start").phase=="complete" && ecc.Operation("verified-acquisition").command==10 &&
            data.starts==1 && data.finishes==0 && data.aborts==1 && data.retirements==1 && !data.running,
            "Acquisition recovery replayed data, sealed the failed Run, or rewrote original history");
    Rejects([&]{ecc.Execute(EccCommand::Start,"start-without-prepare");},"");
    ecc.Execute(EccCommand::Prepare,"fresh-prepare");State(ecc,EccState::Prepared,"fresh-prepare");
}
void PreRunFault(Rig &r,NativeEccOptions &o,TestData &data,const std::string &scenario) {
    NativeEccCoordinator ecc(o,data); ecc.Execute(EccCommand::Describe,"describe"); ecc.Execute(EccCommand::Prepare,"prepare");
    const bool configured=scenario.find("configured")!=std::string::npos;
    if (configured) data.onReceiversStarted=[&] { r.w.alarms[0]->sendAsadAlarm("CoBo0",1,Ice::Current()); };
    else data.onReady=[](unsigned int count) { if (count==2) throw std::runtime_error("Injected final ready observation failure"); };
    r.Clear(); Rejects([&] { ecc.Execute(EccCommand::Configure,"configure"); },configured ? "alarm interlock" : "final ready observation failure");
    Unknown(ecc,EccState::Prepared,"configure");
    Require(data.starts==1 && data.finishes==0 && data.running && data.readyChecks==unsigned(configured ? 1 : 2),"Pre-run fixture failed at a different lifecycle boundary");
    Require(r.Count("C0.sender-connect")==unsigned(!configured) && r.Count("C1.sender-connect")==unsigned(!configured),"Configured/Ready sender boundary differs");
    const auto checks=data.readyChecks; r.Clear();
    ecc.StopProductionAfterFailure("negative-pre-run"); Unknown(ecc,EccState::Prepared,"configure");
    for (const auto &id:{"C0","C1"}) Require(r.Count(std::string(id)+".ctrl.pipeCtrl.firmwareEnable=0")==1 &&
        r.Count(std::string(id)+".ctrl.pipeCtrl.intEnable=0")==1,"Pre-run negative stop skipped independent producer");
    for (const auto &id:{"C0","C1","M0","M1"}) for (const auto &action:{"connect","start","stop","disconnect"})
        Require(!r.Has(std::string(id)+".sender-"+action),"Pre-run negative stop touched retained sender records");
    Require(data.readyChecks==checks && data.starts==1 && !data.finishes,"Negative stop queried/modified receiver lifecycle");
    const auto parent=GetDAQPrepareFixture::ReadFile(Child(o,"negative-pre-run","operation.jsonl"));
    const auto child=GetDAQPrepareFixture::ReadFile(Child(o,"negative-pre-run","stop-production/system-run.jsonl"));
    const auto expected=configured ? SystemAcquisitionClient::State::Configured : SystemAcquisitionClient::State::Ready;
    Require(parent.find("\"retained_object\":\"acquisition\"")!=std::string::npos &&
        child.find("\"previous_run_state\":"+std::to_string(unsigned(expected)))!=std::string::npos,"Pre-run stop bypassed original acquisition object");
    r.Clear(); ecc.StopProductionAfterFailure("negative-pre-run-again"); Require(r.Actions().empty(),"New ID replayed pre-run producer stops");
    Rejects([&] { ecc.Execute(EccCommand::Start,"unverified-start"); },"UNKNOWN");
}
void Storage(Rig &r,NativeEccOptions o,TestData &data) {
    const auto parent=o.auditRoot; struct stat st;
    auto bad=o; bad.description.nodes.back().nodeProxy=bad.description.nodes.front().nodeProxy;
    Rejects([&] { NativeEccCoordinator ecc(bad,data); },"Duplicate canonical");
    Require(::lstat(parent.c_str(),&st)!=0,"Invalid local binding created durable root");
    bad=o; bad.senders.front().circularBufferMask=8;
    Rejects([&] { NativeEccCoordinator ecc(bad,data); },"mask/encoding");
    Require(::lstat(parent.c_str(),&st)!=0,"Invalid sender binding created durable root");
    {
        NativeEccCoordinator ecc(o,data);
        Rejects([&] { NativeEccCoordinator duplicate(o,data); },"locked");
        auto mismatch=o; mismatch.deploymentId="another-deployment";
        Rejects([&] { NativeEccCoordinator other(mismatch,data); },"immutable");
        const auto before=r.w.Writes();
        { std::fstream file(o.auditRoot+"/experiment.json",std::ios::in|std::ios::out|std::ios::binary); file.seekp(0); file.put('!'); file.flush(); Require(bool(file),"Inject same-inode artifact corruption"); }
        Rejects([&] { ecc.Execute(EccCommand::Describe,"tamper"); },"content differs");
        Require(ecc.Status().needsReconciliation && ecc.Operations().empty() && r.w.Writes()==before,"Artifact corruption entered hardware/cleared gate");
        { std::fstream file(o.auditRoot+"/experiment.json",std::ios::in|std::ios::out|std::ios::binary); file.seekp(0); file.put(o.experimentArtifactBytes[0]); file.flush(); }
        Rejects([&] { ecc.Execute(EccCommand::Describe,"restored"); },"latched");
    }
    o.auditRoot=parent+"-late";
    {
        NativeEccCoordinator ecc(o,data); const auto before=r.w.Writes();
        data.onValidate=[&] {
            Require(::rename((o.auditRoot+"/operations").c_str(),(o.auditRoot+"/old-operations").c_str())==0,"Inject operation directory replacement");
            CreateHardwareAuditDirectory(o.auditRoot+"/operations");
        };
        Rejects([&] { ecc.Execute(EccCommand::Describe,"late-binding"); },"identity");
        Require(ecc.Status().needsReconciliation && ecc.Status().outcomeUnknown && ecc.Operation("late-binding").phase=="unknown" && r.w.Writes()==before,"Post-intent directory replacement did not fence hardware");
        data.onValidate=std::function<void()>();
    }
}
}
int main(int argc,char **argv) {
    std::signal(SIGPIPE,SIG_IGN); Ice::CommunicatorPtr communicator;
    try {
        Require(argc==6 && std::string(argv[4])=="--scenario","Pass CoBo/MuTaNT fixtures, evidence index and --scenario");
        const std::string scenario=argv[5]; const bool normal=scenario=="normal-0" || scenario=="normal-1" || scenario=="normal-host-ownership";
        const bool maintenance=scenario=="maintenance-0" || scenario=="maintenance-1";
        const bool failureBreakup=scenario.find("failure-breakup-")==0;
        const bool receiverRetirement=scenario.find("receiver-retirement-")==0;
        const bool acquisitionUnprepare=scenario.find("acquisition-unprepare-")==0;
        const bool acquisitionRecovery=scenario.find("acquisition-recovery-")==0;
        const bool prepareFault=scenario.find("failure-unprepare-")==0 || scenario=="fault-prepare-0" || scenario=="fault-prepare-1" || scenario=="fault-prepare-stop-loss" || scenario=="fault-prepare-stop-name" || scenario=="fault-negative-audit" || scenario=="fault-negative-directory";
        const bool preRunFault=scenario=="fault-pre-run-configured-0" || scenario=="fault-pre-run-configured-1" || scenario=="fault-pre-run-ready-0" || scenario=="fault-pre-run-ready-1";
        const bool hardwareObservation=scenario=="hardware-observation-0" || scenario=="hardware-observation-1";
        const bool descriptionRecovery=scenario.find("description-recovery-")==0;
        Require(maintenance || acquisitionRecovery || acquisitionUnprepare || failureBreakup || receiverRetirement || descriptionRecovery || hardwareObservation || normal || prepareFault || preRunFault || scenario=="host-ownership" || scenario=="fault-start" || scenario=="fault-seal" || scenario=="fault-reset" || scenario=="fault-receiver-start" || scenario=="fault-stop-receiver" || scenario=="fault-operation-audit" || scenario=="storage","Unknown native ECC scenario");
        const auto cobo=LoadHardwareDescription(argv[1]),mutant=LoadHardwareDescription(argv[2]);
        Ice::InitializationData init; init.properties=Ice::createProperties(); init.properties->setProperty("Ice.ThreadPool.Server.Size","4"); communicator=Ice::initialize(init);
        auto adapter=communicator->createObjectAdapterWithEndpoints("NativeEccFixture","tcp -h 127.0.0.1 -p 0");
        const int minor=((descriptionRecovery || acquisitionRecovery || acquisitionUnprepare || failureBreakup || receiverRetirement || scenario.find("failure-unprepare-")==0) && scenario.back()=='0') || scenario=="maintenance-0" || scenario=="hardware-observation-0" || scenario=="normal-0" || scenario=="receiver-retirement-0" || scenario=="fault-prepare-0" || scenario=="fault-pre-run-configured-0" || scenario=="fault-pre-run-ready-0" ? 0 : 1; Rig r(communicator,adapter,cobo,mutant,minor); adapter->activate();
        char directory[]="/tmp/getdaq-native-ecc-XXXXXX"; Require(::mkdtemp(directory),"Create ECC fixture parent");
        auto options=Options(r,std::string(directory)+"/controller"); TestData data(r.w);
        if (scenario=="normal-host-ownership") {
            options.hostControlOwnershipRoot=std::string(directory)+"/ownership";
            Require(::mkdir(options.hostControlOwnershipRoot.c_str(),0700)==0,"Create normal workflow ownership root");
        }
        std::ofstream index(argv[3]); Require(bool(index),"Create native ECC evidence index");
        if(maintenance)Maintenance(r,options,data);
        else if(descriptionRecovery)DescriptionRecovery(r,options,data,scenario);
        else if (hardwareObservation)HardwareObservation(r,options,data);
        else if (scenario=="host-ownership") Ownership(r,options,data);
        else if (normal) Normal(r,options,data,index);
        else if(acquisitionRecovery)AcquisitionRecovery(r,options,data,scenario);
        else if(acquisitionUnprepare)AcquisitionUnprepare(r,options,data,scenario);
        else if(receiverRetirement)ReceiverRetirement(r,options,data,scenario);
        else if(failureBreakup)FailureBreakup(r,options,data,scenario);
        else if (prepareFault) PreparationFault(r,options,data,scenario);
        else if (preRunFault) PreRunFault(r,options,data,scenario);
        else if (scenario=="storage") Storage(r,options,data);
        else Fault(r,options,data,scenario);
        std::ofstream evidence(std::string(argv[3])+".root.json");
        evidence<<"{\"scenario\":"<<JsonString(scenario)<<",\"root\":"<<JsonString(options.auditRoot)
            <<",\"status\":\"fixture_finished\",\"data_lifecycle_test_double\":true,\"real_receiver_executed\":false}\n";
        evidence.flush(); Require(bool(evidence),"Save native ECC root evidence");
        adapter->deactivate(); adapter->waitForDeactivate(); adapter->destroy(); communicator->destroy();
        std::cout<<"PASS native ECC "<<scenario<<": "<<(scenario=="host-ownership" ? "startup conflict and UNKNOWN fence before hardware calls" : "persistent operation/configuration binding and mock hardware flows")
                 <<"; data lifecycle is an explicit test double, not receiver/seal or production network proof\n";
    } catch (const std::exception &e) {
        std::cerr<<"FAIL "<<e.what()<<'\n'; if (communicator) try { communicator->destroy(); } catch (...) {} return 1;
    }
    return 0;
}
