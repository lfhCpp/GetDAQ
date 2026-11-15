#include "eccprotocol.h"
#include "configurationsha256.h"
#include "configuration_readback_fixture.h"
#include "strictjson.h"
#include "source_observation_fixture.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <limits>
namespace {
using namespace GetDAQ;
void Require(bool value,const char *message) { if (!value) throw std::runtime_error(message); }
unsigned int gRejectionCheck=0;
template<class F> void Reject(F fn) {
    ++gRejectionCheck;
    bool rejected=false; try { fn(); } catch (const std::exception &) { rejected=true; }
    if (!rejected) throw std::runtime_error("Invalid portable ECC message was accepted at rejection check "+std::to_string(gRejectionCheck));
}
std::string Read(const std::string &path) { std::ifstream file(path); Require(bool(file),"Cannot read actual archived protocol message"); std::ostringstream bytes; bytes<<file.rdbuf(); return bytes.str(); }
}
int main(int argc,char **argv) {
    try {
        const auto readback=ConfigurationReadbackFixture::Cache();
        const auto decodedReadback=DecodeEccControlConfigureReadbacks(readback);
        Require(decodedReadback.entries.size()==1 && decodedReadback.entries[0].evidence.startedAtUnixMs==9007199254740993ull &&
            decodedReadback.entries[0].evidence.writesReadbackVerified==1,"Configure readback codec lost counts/exact64");
        for(const auto phase:{"not-observed","pending","unavailable"})Require(DecodeEccControlConfigureReadbacks(ConfigurationReadbackFixture::Cache(false,phase)).entries.empty(),"Incomplete cache invented boards");
        const auto changeReadback=[&](const std::string &from,const std::string &to){auto b=readback;const auto at=b.find(from);Require(at!=std::string::npos,"Readback test replacement");b.replace(at,from.size(),to);return b;};
        Require(DecodeEccControlConfigureReadbacks(changeReadback("\"id\":\"C0\"","\"id\":\"板卡 0\"")).entries[0].id=="板卡 0","Readback codec rejected a valid UTF-8 hardware ID");
        Reject([&]{DecodeEccControlConfigureReadbacks(changeReadback("\"id\":\"C0\"","\"id\":\"*\""));});
        Reject([&]{DecodeEccControlConfigureReadbacks(changeReadback("\"actual_loaded_configuration_verified\":false","\"actual_loaded_configuration_verified\":true"));});
        Reject([&]{DecodeEccControlConfigureReadbacks(changeReadback("\"historical\":true","\"historical\":false"));});
        Reject([&]{DecodeEccControlConfigureReadbacks(changeReadback("\"writes_without_full_readback\":1","\"writes_without_full_readback\":0"));});
        Reject([&]{DecodeEccControlConfigureReadbacks(changeReadback("hardware-0.jsonl","../hardware-0.jsonl"));});
        Reject([&]{DecodeEccControlConfigureReadbacks(std::string(1024*1024+1,' '));});
        const std::string ownership="{\"schema\":1,\"kind\":\"getdaq.ecc-ownership\",\"instance_id\":\""+std::string(64,'a')+
            "\",\"snapshot_sha256\":\""+std::string(64,'b')+"\",\"ownership\":{\"enabled\":true,\"scope\":\"single-host-cooperating-ecc\",\"directory\":\"/locks\",\"held_at_last_check\":true,\"fault_latched\":false,\"cached\":true,\"board_global_lock_verified\":false,\"cross_host_lock_verified\":false,\"resources\":[\"127.0.0.1\",\"127.0.0.10\",\"127.0.0.2\"]}}";
        Require(DecodeEccControlOwnership(ownership).resources.size()==3,"Ownership scope/resources lost");
        const auto modified=[&](const std::string &from,const std::string &to) {
            auto body=ownership;const auto at=body.find(from);Require(at!=std::string::npos,"Ownership test replacement missing");body.replace(at,from.size(),to);return body;
        };
        for (const auto &address:{"localhost","127.000.0.1","224.0.0.1","127.0.0.256","127.0.0.1.2"})
            Reject([&] {DecodeEccControlOwnership(modified("127.0.0.1\"",std::string(address)+"\""));});
        Reject([&] {DecodeEccControlOwnership(modified("\"board_global_lock_verified\":false","\"board_global_lock_verified\":true"));});
        Reject([&] {DecodeEccControlOwnership(modified("\"cached\":true","\"cached\":false"));});
        Reject([&] {DecodeEccControlOwnership(modified("\"fault_latched\":false","\"fault_latched\":true"));});
        Reject([&] {DecodeEccControlOwnership(modified("/locks","/locks/../other"));});
        Reject([&] {DecodeEccControlOwnership(modified("127.0.0.10","127.0.0.1"));});
        Reject([&] {DecodeEccControlOwnership(modified("single-host-cooperating-ecc","board-global"));});
        Reject([&] {DecodeEccControlOwnership(std::string(65537,' '));});
        const auto sourceRow=SourceObservationFixture::Row("C0",std::string(64,'d'));
        const auto sources=SourceObservationFixture::Root(sourceRow);
        const auto sourceDecoded=DecodeEccControlSourceObservation(sources);
        Require(sourceDecoded.sources[0].locallySentBytes==9007199254740993ull && sourceDecoded.sources[0].observedAtUnixMs==std::numeric_limits<std::uint64_t>::max(),"Source observation lost exact counters");
        Reject([&] {DecodeEccControlSourceObservation(SourceObservationFixture::Root(sourceRow+','+sourceRow));});
        Reject([&] {auto b=sources;const std::string token="\"locally_sent_bytes\":\"9007199254740993\"";b.replace(b.find(token),token.size(),"\"locally_sent_bytes\":9007199254740993");DecodeEccControlSourceObservation(b);});
        Reject([&] {auto b=sources;const std::string token="\"source_end_acknowledged\":false";b.replace(b.find(token),token.size(),"\"source_end_acknowledged\":true");DecodeEccControlSourceObservation(b);});
        Reject([&] {auto b=sources;b.replace(b.find("\"busy\":false"),12,"\"busy\":true");DecodeEccControlSourceObservation(b);});
        Reject([&] {DecodeEccControlSourceObservation(std::string(256*1024+1,' '));});
        Require(DecodeEccControlSourceObservation(SourceObservationFixture::Root(SourceObservationFixture::Row("C0",std::string(64,'d'),"unknown"))).sources[0].state=="unknown","Source UNKNOWN was hidden");
        EccControlRequest r; r.operationId="run_001-start"; r.command="start"; r.instanceId=std::string(64,'a'); r.snapshotSha256=std::string(64,'b'); r.clientId="console"; r.clientEpoch=std::string(64,'c');
        {
            auto completion=r;completion.command="complete-data-handoff";completion.failedOperationId="original-handoff";
            Require(DecodeEccControlRequest(EncodeEccControlRequest(completion)).failedOperationId=="original-handoff","Completion lost original handoff binding");
            auto changed=completion;changed.failedOperationId="another-handoff";
            Require(EccControlRequestKey(changed)!=EccControlRequestKey(completion),"Completion request key omitted the original handoff");
            Reject([&]{auto bad=completion;bad.failedOperationId.clear();EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=completion;bad.failedOperationId=bad.operationId;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=completion;bad.coboId="C0";EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=completion;bad.successorEpoch=std::string(64,'d');EncodeEccControlRequest(bad);});
        }
        {
            auto selected=r;selected.command="reprepare-cobo";selected.coboId="C0";
            const auto encoded=EncodeEccControlRequest(selected);Require(DecodeEccControlRequest(encoded).coboId=="C0","Maintenance target was lost");
            auto other=selected;other.coboId="C1";Require(EccControlRequestKey(other)!=EccControlRequestKey(selected),"Maintenance key did not bind the selected board");
            Reject([&] {auto b=selected;b.coboId.clear();EncodeEccControlRequest(b);});
            Reject([&] {auto b=selected;b.coboId=std::string(129,'x');EncodeEccControlRequest(b);});
            Reject([&] {auto b=selected;b.command="start";EncodeEccControlRequest(b);});
            auto configureSelected=selected;configureSelected.command="configure-cobo";
            const auto configureBytes=EncodeEccControlRequest(configureSelected);
            Require(DecodeEccControlRequest(configureBytes).command=="configure-cobo" &&
                    DecodeEccControlRequest(configureBytes).coboId=="C0","Single-CoBo Configure target was lost");
            auto configureOther=configureSelected;configureOther.coboId="C1";
            Require(EccControlRequestKey(configureOther)!=EccControlRequestKey(configureSelected),"Single-CoBo Configure request key omitted the board");
            EccControlMaintenanceTargets targets;targets.instanceId=r.instanceId;targets.snapshotSha256=r.snapshotSha256;
            targets.configurationSha256=std::string(64,'1');targets.artifactSha256=std::string(64,'2');targets.cobos.push_back({"C0","CoBo0"});
            targets.cobos[0].lifecycleState="prepared";targets.cobos[0].operationId="prep-1";targets.cobos[0].operation="prepare";targets.cobos[0].operationPhase="complete";
            const auto decodedTargets=DecodeEccControlMaintenanceTargets(EncodeEccControlMaintenanceTargets(targets));
            Require(decodedTargets.cobos[0].id=="C0" && decodedTargets.cobos[0].lifecycleState=="prepared" &&
                    decodedTargets.cobos[0].operationId=="prep-1" && decodedTargets.cobos[0].operationPhase=="complete","Maintenance metadata lost per-CoBo lifecycle evidence");
            Reject([&] {auto b=targets;b.cobos.clear();EncodeEccControlMaintenanceTargets(b);});
            Reject([&] {auto b=targets;b.cobos.push_back(b.cobos[0]);EncodeEccControlMaintenanceTargets(b);});
            Reject([&] {auto b=targets;b.cobos[0].id=std::string(129,'x');EncodeEccControlMaintenanceTargets(b);});
            Reject([&] {auto b=targets;b.cobos[0].lifecycleState="running-ish";EncodeEccControlMaintenanceTargets(b);});
            Reject([&] {auto b=targets;b.cobos[0].operationPhase="running";b.cobos[0].operationId.clear();EncodeEccControlMaintenanceTargets(b);});
        }
        {
            auto led=r;led.command="set-cobo-led";led.coboId="C0";led.ledAsadId=3;led.ledType=2;led.ledState=3;
            const auto bytes=EncodeEccControlRequest(led);const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.coboId=="C0" && decoded.ledAsadId==3 && decoded.ledType==2 && decoded.ledState==3 &&
                EncodeEccControlRequest(decoded)==bytes,"LED request did not preserve canonical target/state");
            auto changed=led;changed.ledState=2;Require(EccControlRequestKey(changed)!=EccControlRequestKey(led),"LED request key omitted state");
            Reject([&]{auto bad=led;bad.ledAsadId=4;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=led;bad.ledType=4;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=led;bad.ledState=4;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=led;bad.coboId.clear();EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=r;bad.ledState=1;EncodeEccControlRequest(bad);});
            EccControlControllerStatus status;status.transition=12;
            Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(status)).transition==12,"LED transition code is outside status protocol bounds");
        }
        {
            auto blink=r;blink.command="blink-cobo-leds";blink.coboId="C0";
            const auto bytes=EncodeEccControlRequest(blink);const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.coboId=="C0" && decoded.command=="blink-cobo-leds" && EncodeEccControlRequest(decoded)==bytes,
                    "One-shot LED blink request did not preserve its selected target");
            auto changed=blink;changed.coboId="C1";Require(EccControlRequestKey(changed)!=EccControlRequestKey(blink),"LED blink request key omitted the selected board");
            Reject([&]{auto bad=blink;bad.coboId.clear();EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=blink;bad.ledState=1;EncodeEccControlRequest(bad);});
            const auto injected=bytes.substr(0,bytes.size()-2)+",\"asad_id\":0}\n";
            Reject([&]{DecodeEccControlRequest(injected);});
            EccControlControllerStatus status;status.transition=14;
            Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(status)).transition==14,"LED blink transition code is outside status bounds");
        }
        {
            auto flip=r;flip.command="flip-cobo-led";flip.coboId="C0";flip.ledFlipId=10;
            const auto bytes=EncodeEccControlRequest(flip);const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.command=="flip-cobo-led" && decoded.coboId=="C0" && decoded.ledFlipId==10 && EncodeEccControlRequest(decoded)==bytes,
                    "Selected LED flip did not preserve exact board/bit identity");
            auto changed=flip;changed.ledFlipId=11;Require(EccControlRequestKey(changed)!=EccControlRequestKey(flip),"LED flip request key omitted GPIO bit");
            changed=flip;changed.coboId="C1";Require(EccControlRequestKey(changed)!=EccControlRequestKey(flip),"LED flip request key omitted board");
            auto all=flip;all.ledFlipId=16;Require(DecodeEccControlRequest(EncodeEccControlRequest(all)).ledFlipId==16,"All-LED flip sentinel was lost");
            Reject([&]{auto bad=flip;bad.ledFlipId=17;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=flip;bad.coboId.clear();EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=flip;bad.ledState=1;EncodeEccControlRequest(bad);});
            const auto injected=bytes.substr(0,bytes.size()-2)+",\"asad_id\":0}\n";
            Reject([&]{DecodeEccControlRequest(injected);});
            EccControlControllerStatus status;status.transition=15;
            Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(status)).transition==15,"LED flip transition code is outside status bounds");
        }
        {
            auto pulse=r;pulse.command="pulse-cobo-led";pulse.coboId="C0";pulse.ledPulseId=10;pulse.ledPulsePeriodMs=725;
            const auto bytes=EncodeEccControlRequest(pulse);const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.command=="pulse-cobo-led" && decoded.coboId=="C0" && decoded.ledPulseId==10 && decoded.ledPulsePeriodMs==725 &&
                    EncodeEccControlRequest(decoded)==bytes,"LED pulse request did not preserve exact target and period");
            auto changed=pulse;changed.ledPulseId=11;Require(EccControlRequestKey(changed)!=EccControlRequestKey(pulse),"LED pulse request key omitted LED selection");
            changed=pulse;changed.ledPulsePeriodMs=726;Require(EccControlRequestKey(changed)!=EccControlRequestKey(pulse),"LED pulse request key omitted period");
            auto all=pulse;all.ledPulseId=16;all.ledPulsePeriodMs=0;const auto stopped=DecodeEccControlRequest(EncodeEccControlRequest(all));
            Require(stopped.ledPulseId==16 && stopped.ledPulsePeriodMs==0,"All-LED pulse stop sentinel was lost");
            Reject([&]{auto bad=pulse;bad.ledPulseId=17;EncodeEccControlRequest(bad);});
            Reject([&]{auto bad=pulse;bad.ledFlipId=2;EncodeEccControlRequest(bad);});
        }
        {
            auto pulser=r;pulser.command="trigger-asad-pulser";pulser.coboId="C0";pulser.pulserVoltageMv=-1000;
            const auto bytes=EncodeEccControlRequest(pulser);const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.coboId=="C0" && decoded.pulserVoltageMv==-1000 && EncodeEccControlRequest(decoded)==bytes,
                    "Single pulser request did not preserve the signed boundary voltage");
            auto changed=pulser;changed.pulserVoltageMv=1000;
            Require(EccControlRequestKey(changed)!=EccControlRequestKey(pulser),"Pulser request key omitted voltage");
            changed.pulserVoltageMv=1001;Reject([&]{EncodeEccControlRequest(changed);});
            changed.pulserVoltageMv=-1001;Reject([&]{EncodeEccControlRequest(changed);});
            Reject([&]{auto bad=pulser;bad.coboId.clear();EncodeEccControlRequest(bad);});
            auto malformed=bytes;const auto voltage=malformed.find("\"voltage_mv\":-1000");Require(voltage!=std::string::npos,"Pulser request JSON field missing");
            malformed.replace(voltage,std::string("\"voltage_mv\":-1000").size(),"\"voltage_mv\":-1001");Reject([&]{DecodeEccControlRequest(malformed);});
            malformed=bytes;malformed.replace(voltage,std::string("\"voltage_mv\":-1000").size(),"\"voltage_mv\":-0");Reject([&]{DecodeEccControlRequest(malformed);});
        }
        const auto encoded=EncodeEccControlRequest(r); Require(DecodeEccControlRequest(encoded).operationId==r.operationId,"Request identity lost");
        auto handoff=r;handoff.command="handoff-data-control";handoff.successorScopeSha256=std::string(64,'d');
        handoff.successorEpoch=std::string(64,'e');handoff.successorPlanSha256=std::string(64,'f');handoff.successorDataBindingSha256=std::string(64,'1');
        const auto handoffBytes=EncodeEccControlRequest(handoff);
        Require(EncodeEccControlRequest(DecodeEccControlRequest(handoffBytes))==handoffBytes,"ECC handoff canonical request changed");
        for (unsigned int field=0;field<4;++field) {
            auto changed=handoff;
            auto &value=field==0 ? changed.successorScopeSha256 : field==1 ? changed.successorEpoch : field==2 ? changed.successorPlanSha256 : changed.successorDataBindingSha256;
            value=std::string(64,'2');Require(EccControlRequestKey(changed)!=EccControlRequestKey(handoff),"ECC handoff target is not bound to request key");
            value.clear();Reject([&]{EncodeEccControlRequest(changed);});
        }
        Reject([&]{auto changed=handoff;changed.command="start";EncodeEccControlRequest(changed);});
        auto plans=handoff;plans.command="retire-data-plans";
        EccReceiverPlanTarget first;first.agentId="node-0";first.planSha256=std::string(64,'2');first.planFile="/reviewed/node-0.json";
        EccReceiverPlanTarget second=first;second.agentId="node-1";second.planSha256=std::string(64,'3');second.planFile="/reviewed/node-1.json";
        plans.successorReceiverPlans={first,second};const auto plansBytes=EncodeEccControlRequest(plans);
        Require(EncodeEccControlRequest(DecodeEccControlRequest(plansBytes))==plansBytes,"ECC data plan targets lost canonical bytes");
        for (unsigned int field=0;field<3;++field) {
            auto changed=plans;auto &target=changed.successorReceiverPlans[0];
            if(field==0)target.agentId="agent-0";else if(field==1)target.planSha256=std::string(64,'4');else target.planFile="/reviewed/different.json";
            Require(EccControlRequestKey(changed)!=EccControlRequestKey(plans),"Node plan target not bound to parent request key");
        }
        Reject([&]{auto changed=plans;changed.successorReceiverPlans.clear();EncodeEccControlRequest(changed);});
        Reject([&]{auto changed=plans;changed.command="handoff-data-control";EncodeEccControlRequest(changed);});
        Reject([&]{auto changed=plans;changed.successorReceiverPlans[0].agentId="node-1";EncodeEccControlRequest(changed);});
        Reject([&]{auto changed=plans;std::swap(changed.successorReceiverPlans[0],changed.successorReceiverPlans[1]);EncodeEccControlRequest(changed);});
        for (const auto &path:{"relative.json","/reviewed/../other.json","/reviewed/./other.json","/reviewed//other.json","/reviewed/..","/reviewed/."})
            Reject([&]{auto changed=plans;changed.successorReceiverPlans[0].planFile=path;EncodeEccControlRequest(changed);});
        Reject([&]{auto changed=plans;changed.successorReceiverPlans[0].planFile='/'+std::string(4000,'a');changed.successorReceiverPlans[1].planFile='/'+std::string(4000,'b');EncodeEccControlRequest(changed);});
        auto leased=plans;leased.successorPlanFile="/reviewed/successor.json";
        Require(EccControlRequestKey(leased)!=EccControlRequestKey(plans),"Service-host review lease path not bound to parent request");
        Require(EncodeEccControlRequest(DecodeEccControlRequest(EncodeEccControlRequest(leased)))==EncodeEccControlRequest(leased),"Reviewed retirement changed canonical bytes");
        Reject([&]{auto changed=leased;changed.command="handoff-data-control";changed.successorReceiverPlans.clear();EncodeEccControlRequest(changed);});
        Reject([&]{auto changed=leased;changed.successorPlanFile="relative.json";EncodeEccControlRequest(changed);});
        const auto successor="{\"schema\":1,\"kind\":\"getdaq.ecc-successor-plan\",\"plan_file\":\"/reviewed/successor.json\",\"service_id\":\"ecc\",\"controller_id\":\"controller\",\"input_binding_sha256\":"+
            EncodeJsonString(std::string(64,'1'))+",\"snapshot_sha256\":"+EncodeJsonString(std::string(64,'2'))+",\"data_lifecycle_binding_sha256\":"+EncodeJsonString(std::string(64,'3'))+
            ",\"data_controller_scope_sha256\":"+EncodeJsonString(std::string(64,'4'))+",\"audit_root\":\"/reviewed/next-controller\",\"receiver_controller_epoch\":"+EncodeJsonString(std::string(64,'5'))+"}\n";
        const auto targets="{\"schema\":1,\"kind\":\"getdaq.reviewed-data-plan-targets\",\"plans\":[{\"agent_id\":\"node-0\",\"plan_sha256\":"+EncodeJsonString(std::string(64,'6'))+",\"plan_file\":\"/node/reviewed.json\"}]}\n";
        const auto review=ReviewEccPlanTransition(successor,targets);
        Require(EncodeEccSuccessorIdentity(DecodeEccSuccessorIdentity(successor))==successor,"Shared successor identity encoding changed native canonical bytes");
        Require(review.successor.planFile==leased.successorPlanFile && review.targets.size()==1 && review.targets[0].agentId=="node-0" &&
            review.successorBytes==successor && review.targetsBytes==targets && review.successorSha256==GetDAQ::ConfigurationSha256(successor),"Offline transition review changed inputs or identities");
        Reject([&]{ReviewEccPlanTransition(successor,targets+std::string(8192,' '));});
        Reject([&]{ReviewEccPlanTransition(successor+" ",targets);});
        Reject([&]{ReviewEccPlanTransition(successor,"{\"schema\":1,\"kind\":\"getdaq.reviewed-data-plan-targets\",\"plans\":[]}");});
        Reject([&]{auto changed=successor;const std::string path="/reviewed/next-controller";const auto where=changed.find(path);changed.replace(where,path.size(),"D:\\\\next-controller");DecodeEccSuccessorIdentity(changed);});
        const auto golden="{\"schema\":1,\"operation_id\":\"run_001-start\",\"command\":\"start\",\"instance_id\":\""+r.instanceId+"\",\"snapshot_sha256\":\""+r.snapshotSha256+"\",\"client_id\":\"console\",\"client_epoch\":\""+r.clientEpoch+"\"}\n";
        Require(encoded==golden,"Canonical request bytes changed");
        Reject([&] { DecodeEccControlRequest(encoded.substr(0,encoded.size()-2)+",\"raw_frame\":\"data\"}\n"); });
        Reject([&] { DecodeEccControlRequest("{\"schema\":1,\"schema\":1}"); });
        Reject([&] { DecodeEccControlRequest(std::string(8193,' ')); });
        auto bad=r; bad.operationId="../start"; Reject([&] { EncodeEccControlRequest(bad); });
        bad=r; bad.command="catalog"; Reject([&] { EncodeEccControlRequest(bad); });
        bad=r; bad.instanceId=std::string(64,'A'); Reject([&] { EncodeEccControlRequest(bad); });
        auto recovery=r;recovery.command="reconcile-description";recovery.failedOperationId="failed-describe";
        const auto recoveryBytes=EncodeEccControlRequest(recovery);
        Require(DecodeEccControlRequest(recoveryBytes).failedOperationId=="failed-describe","Recovery lost original operation binding");
        auto changedRecovery=recovery;changedRecovery.failedOperationId="another-describe";
        Require(EccControlRequestKey(changedRecovery)!=EccControlRequestKey(recovery),"Original recovery ID absent from request digest");
        Reject([&]{auto b=recovery;b.failedOperationId.clear();EncodeEccControlRequest(b);});
        Reject([&]{auto b=recovery;b.failedOperationId=b.operationId;EncodeEccControlRequest(b);});
        Reject([&]{auto b=r;b.failedOperationId="failed-describe";EncodeEccControlRequest(b);});
        {
            auto catalogRecovery=r;catalogRecovery.command="resume-catalog-replacement";catalogRecovery.failedOperationId="failed-catalog-replace";
            const auto bytes=EncodeEccControlRequest(catalogRecovery);
            const auto decoded=DecodeEccControlRequest(bytes);
            Require(decoded.command==catalogRecovery.command && decoded.failedOperationId==catalogRecovery.failedOperationId,
                    "Catalog recovery request lost its original failed replacement binding");
            auto changed=catalogRecovery;changed.failedOperationId="different-catalog-operation";
            Require(EccControlRequestKey(changed)!=EccControlRequestKey(catalogRecovery),"Catalog recovery key omitted original operation ID");
            Reject([&]{auto b=catalogRecovery;b.failedOperationId.clear();EncodeEccControlRequest(b);});
            Reject([&]{auto b=catalogRecovery;b.failedOperationId=b.operationId;EncodeEccControlRequest(b);});
            Reject([&]{auto b=catalogRecovery;b.catalogReplaceExisting=true;EncodeEccControlRequest(b);});
        }
        EccControlControllerStatus s; s.state=5; s.operationId=r.operationId;
        auto status=EncodeEccControlControllerStatus(s); Require(DecodeEccControlControllerStatus(status).state==5,"Controller status lost");
        auto multiline=s;multiline.message="Ice::ConnectionLostException:\nconnection lost\t(details)";
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(multiline)).message==multiline.message,"Ice multiline diagnostic was not preserved");
        Reject([&]{auto b=r;b.clientId="console\nother";EncodeEccControlRequest(b);});
        Reject([&]{auto b=s;b.message=std::string(1,'\0');EncodeEccControlControllerStatus(b);});
        auto recovering=s;recovering.transition=10;
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(recovering)).transition==10,"Recovery transition rejected");
        auto maintenance=s;maintenance.transition=11;
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(maintenance)).transition==11,"Maintenance transition rejected");
        auto ledTransition=s;ledTransition.transition=12;
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(ledTransition)).transition==12,"LED transition rejected");
        auto pulserTransition=s;pulserTransition.transition=13;
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(pulserTransition)).transition==13,"Pulser transition rejected");
        auto configureCoboTransition=s;configureCoboTransition.transition=17;
        Require(DecodeEccControlControllerStatus(EncodeEccControlControllerStatus(configureCoboTransition)).transition==17,"Single-CoBo Configure transition rejected");
        Reject([&] { auto b=s; b.state=7; EncodeEccControlControllerStatus(b); });
        Reject([&] { auto b=status; b.replace(b.find("\"state\":5"),9,"\"state\":5.0"); DecodeEccControlControllerStatus(b); });
        EccControlOperation op; op.request=r; op.requestKey=EccControlRequestKey(r); op.phase="complete"; op.resultJson=status;
        Require(DecodeEccControlOperation(EncodeEccControlOperation(op)).requestKey==op.requestKey,"Operation binding lost");
        Reject([&] { auto b=op; b.requestKey=std::string(64,'0'); EncodeEccControlOperation(b); });
        Reject([&] { auto b=op; b.resultJson.clear(); EncodeEccControlOperation(b); });
        Reject([&] { auto b=op; b.phase="accepted"; EncodeEccControlOperation(b); });
        auto unknown=op; unknown.phase="unknown"; unknown.resultJson.clear(); Require(DecodeEccControlOperation(EncodeEccControlOperation(unknown)).phase=="unknown","Reopened unfinished intent requires invented result");
        const std::string identity="{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"service_id\":\"ecc\",\"client_id\":\"console\",\"instance_id\":\""+r.instanceId+"\",\"snapshot_sha256\":\""+r.snapshotSha256+
            "\",\"input_binding_sha256\":\""+r.snapshotSha256+"\",\"configuration_sha256\":\""+r.snapshotSha256+"\",\"artifact_sha256\":\""+r.snapshotSha256+"\",\"data_binding_sha256\":\""+r.snapshotSha256+"\"}";
        Require(DecodeEccControlIdentity(identity).clientId==r.clientId,"Identity fields lost");
        EccControlConfiguration configuration;configuration.instanceId=r.instanceId;configuration.snapshotSha256=r.snapshotSha256;
        configuration.inputBindingSha256=configuration.configurationSha256=configuration.artifactSha256=r.snapshotSha256;
        const auto configurationBytes=EncodeEccControlConfiguration(configuration);
        Require(!DecodeEccControlConfiguration(configurationBytes).hasSelection,"Legacy configuration invented repository selection");
        Reject([&] {auto b=configurationBytes;const auto at=b.find("\"hardware_loaded_verified\":false");b.replace(at,32,"\"hardware_loaded_verified\":true");DecodeEccControlConfiguration(b);});
        Reject([&] {DecodeEccControlConfiguration(configurationBytes.substr(0,configurationBytes.size()-2)+",\"raw_frame\":true}\n");});
        configuration.hasSelection=true;configuration.selection.registrationSha256=r.snapshotSha256;
        auto &selected=configuration.selection.registration;selected.repositoryId=selected.publicationSha256=selected.effectiveSha256=selected.snapshotSha256=r.snapshotSha256;
        selected.configurationSha256=selected.artifactSha256=r.snapshotSha256;selected.manifestFile="/repository/version/publication.json";
        Require(DecodeEccControlConfiguration(EncodeEccControlConfiguration(configuration)).hasSelection,"Reviewed selection missing from typed cache");
        Reject([&] {auto c=configuration;c.selection.registration.artifactSha256=std::string(64,'d');EncodeEccControlConfiguration(c);});
        Reject([&] {DecodeEccControlConfiguration(std::string(65537,' '));});
        auto catalogRequest=r; catalogRequest.command="catalog"; catalogRequest.catalogExperiment="Test"; catalogRequest.catalogRunNumber=17;
        Require(EncodeEccControlRequest(DecodeEccControlRequest(EncodeEccControlRequest(catalogRequest)))==EncodeEccControlRequest(catalogRequest),"Catalog experiment/Run canonical contract differs");
        auto otherCatalog=catalogRequest; ++otherCatalog.catalogRunNumber; Require(EccControlRequestKey(otherCatalog)!=EccControlRequestKey(catalogRequest),"Catalog Run absent from key");
        auto replacementCatalog=catalogRequest; replacementCatalog.catalogReplaceExisting=true;
        Require(DecodeEccControlRequest(EncodeEccControlRequest(replacementCatalog)).catalogReplaceExisting &&
            EccControlRequestKey(replacementCatalog)!=EccControlRequestKey(catalogRequest),"Explicit replacement authorization absent from catalog request scope");
        Reject([&] { auto b=r; b.catalogExperiment="Test"; EncodeEccControlRequest(b); });
        Reject([&] { DecodeEccControlIdentity(identity.substr(0,identity.size()-1)+",\"raw\":[]} "); });
        const auto h=std::string(64,'a');
        const std::string system="{\"schema\":1,\"service\":\"getdaq-ecc-server\",\"instance_id\":\""+h+"\",\"snapshot_sha256\":\""+h+"\",\"operation_active\":false,\"operation_id\":\"\",\"client_epoch\":\"\",\"control_needs_reconciliation\":false,\"message\":\"\",\"controller\":"+status+
            ",\"data\":{\"needs_reconciliation\":false,\"controller_scope_sha256\":\""+h+"\",\"controller_epoch\":\""+h+"\",\"message\":\"\",\"nodes\":[{\"agent_id\":\"A\",\"instance_id\":\""+h+"\",\"plan_sha256\":\""+h+
            "\",\"lifecycle\":{\"schema\":1,\"state\":\"stopped\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h+"\",\"session_id\":\""+h+"\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\""+h+
            "\",\"output_directory\":\"/data/session\",\"pid\":42,\"exit_code\":0,\"reaped\":true,\"seal_verified\":true,\"received_bytes\":\"18446744073709551615\",\"frames\":\"9007199254740993\",\"files\":\"128\"}]}}]}}";
        const auto decoded=DecodeEccControlSystemStatus(system); Require(decoded.nodes[0].receivers[0].receivedBytes==std::numeric_limits<std::uint64_t>::max() && decoded.nodes[0].receivers[0].frames==9007199254740993ULL,"Large counters lost precision");
        auto ice=system;ice.insert(ice.find(",\"received_bytes\":"),",\"source_end_confirmed\":true");
        Require(DecodeEccControlSystemStatus(ice).nodes[0].receivers[0].sourceEndConfirmed,"ICE source end lost by portable GUI protocol");
        Reject([&] {auto invalid=ice;const auto at=invalid.find("\"reaped\":true");invalid.replace(at,13,"\"reaped\":false");DecodeEccControlSystemStatus(invalid);});
        auto unused=system;
        const auto counters=unused.find("\"received_bytes\":\"18446744073709551615\",\"frames\":\"9007199254740993\",\"files\":\"128\"");
        const std::string originalCounters="\"received_bytes\":\"18446744073709551615\",\"frames\":\"9007199254740993\",\"files\":\"128\"";
        Require(counters!=std::string::npos,"Find unused-source portable fixture counters");
        unused.replace(counters,originalCounters.size(),"\"received_bytes\":\"0\",\"frames\":\"0\",\"files\":\"0\",\"source_unused_confirmed\":true");
        Require(DecodeEccControlSystemStatus(unused).nodes[0].receivers[0].sourceUnusedConfirmed,"Portable GUI protocol lost unused-source proof");
        Reject([&] {auto invalid=unused;const auto at=invalid.find("\"received_bytes\":\"0\"");invalid.replace(at,20,"\"received_bytes\":\"1\"");DecodeEccControlSystemStatus(invalid);});
        Reject([&] {auto invalid=unused;const auto at=invalid.find("\"reaped\":true");invalid.replace(at,13,"\"reaped\":false");DecodeEccControlSystemStatus(invalid);});
        EccControlCatalogReceipt receipt; receipt.operationId="catalog-1"; receipt.requestKey=receipt.manifestSha256=h; receipt.directory="/srv/archive/Test/run_0017/node"; receipt.linkedFiles=128;
        const auto receiptBytes=EncodeEccControlCatalogReceipt(receipt); Require(DecodeEccControlCatalogReceipt(receiptBytes).directory==receipt.directory,"Catalog receipt codec differs");
        auto archived=system; archived.insert(archived.find(",\"lifecycle\":"),",\"catalog\":"+receiptBytes);
        Require(DecodeEccControlSystemStatus(archived).nodes[0].catalogVerified,"Catalog completion missing from typed whole-system status");
        Reject([&] { auto b=receiptBytes; const auto at=b.find("\"linked_files\":\"128\""); b.replace(at,20,"\"linked_files\":128"); DecodeEccControlCatalogReceipt(b); });
        Reject([&] { auto b=receipt; b.linkedFiles=65537; EncodeEccControlCatalogReceipt(b); });
        Reject([&] { auto b=system; const auto position=b.find("18446744073709551615"); b.replace(position,20,"18446744073709551616"); DecodeEccControlSystemStatus(b); });
        Reject([&] { auto b=system; const auto position=b.find("\"frames\":\"9007199254740993\""); b.replace(position,27,"\"frames\":9007199254740993"); DecodeEccControlSystemStatus(b); });
        const std::string observation="{\"schema\":1,\"kind\":\"getdaq.ecc-receivers\",\"instance_id\":\""+h+"\",\"snapshot_sha256\":\""+h+"\",\"data_binding_sha256\":\""+h+
            "\",\"nodes\":[{\"agent_id\":\"A\",\"plan_sha256\":\""+h+"\",\"instance_id\":\""+h+"\",\"available\":true,\"message\":\"\",\"observation\":{\"schema\":1,\"busy\":false,\"observed_at_unix_ms\":\"18446744073709551615\",\"lifecycle\":{\"schema\":1,\"state\":\"ready\",\"needs_reconciliation\":false,\"plan_sha256\":\""+h+
            "\",\"session_id\":\""+h+"\",\"message\":\"\",\"receivers\":[{\"id\":\"C0\",\"session_id\":\""+h+"\",\"output_directory\":\"/data/session\",\"pid\":42,\"exit_code\":-1,\"reaped\":false,\"seal_verified\":false,\"received_bytes\":\"9007199254740993\",\"frames\":\"1\",\"files\":\"1\"}]},\"receivers\":[{\"id\":\"C0\",\"pid\":42,\"state\":\"running\",\"exit_code\":-1,\"signal\":0,\"message\":\"\"}]}}]}";
        const auto observed=DecodeEccControlReceiverObservation(observation);
        Require(observed.nodes.size()==1 && observed.nodes[0].observedAtUnixMs==UINT64_MAX && observed.nodes[0].receivers[0].state=="running","Portable receiver observation precision/state lost");
        const auto replaceObservation=[&](const std::string &from,const std::string &to) {auto bytes=observation;const auto at=bytes.rfind(from);Require(at!=std::string::npos,"Observation test replacement missing");bytes.replace(at,from.size(),to);return bytes;};
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"pid\":42","\"pid\":43"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"state\":\"running\"","\"state\":\"exited\""));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"busy\":false","\"busy\":true"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"signal\":0","\"signal\":9"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"observed_at_unix_ms\":\"18446744073709551615\"","\"observed_at_unix_ms\":18446744073709551615"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("18446744073709551615","18446744073709551616"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"available\":true","\"available\":false"));});
        Reject([&] {DecodeEccControlReceiverObservation(replaceObservation("\"id\":\"C0\"","\"id\":\"other\""));});
        Reject([&] {DecodeEccControlReceiverObservation(std::string(1024*1024+1,' '));});
        const std::string receiverEvents="{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-events\",\"instance_id\":\""+h+"\",\"snapshot_sha256\":\""+h+"\",\"data_binding_sha256\":\""+h+
            "\",\"events\":{\"agent_id\":\"A\",\"plan_sha256\":\""+h+"\",\"instance_id\":\""+h+"\",\"source_id\":\"C0\",\"session_id\":\""+h+
            "\",\"pid\":42,\"available\":true,\"acquisition_events\":\"9007199254740993\",\"observed_at_unix_ms\":\"18446744073709551615\"}}";
        const auto eventCount=DecodeEccControlReceiverEvents(receiverEvents);
        Require(eventCount.acquisitionEvents==9007199254740993ULL && eventCount.observedAtUnixMs==UINT64_MAX && eventCount.sourceId=="C0",
                "Receiver event codec lost exact counters or source identity");
        const auto replaceReceiverEvents=[&](const std::string &from,const std::string &to) {auto bytes=receiverEvents;const auto at=bytes.find(from);Require(at!=std::string::npos,"Receiver event test replacement missing");bytes.replace(at,from.size(),to);return bytes;};
        Reject([&] {DecodeEccControlReceiverEvents(replaceReceiverEvents("\"9007199254740993\"","9007199254740993"));});
        Reject([&] {DecodeEccControlReceiverEvents(replaceReceiverEvents("\"available\":true","\"available\":false"));});
        Reject([&] {DecodeEccControlReceiverEvents(replaceReceiverEvents("\"kind\":\"getdaq.ecc-receiver-events\"","\"kind\":\"getdaq.unknown\""));});
        auto fileBytes=replaceReceiverEvents("getdaq.ecc-receiver-events","getdaq.ecc-receiver-file-bytes");
        const auto countKey=fileBytes.find("acquisition_events");Require(countKey!=std::string::npos,"Missing original count key");fileBytes.replace(countKey,18,"accepted_file_bytes");
        const auto rawSize=DecodeEccControlReceiverEvents(fileBytes,true);
        Require(rawSize.fileBytes==9007199254740993ULL && rawSize.acquisitionEvents==0,"Raw file size lost exact byte precision");
        Reject([&] {DecodeEccControlReceiverEvents(fileBytes);});
        Reject([&] {DecodeEccControlReceiverEvents(receiverEvents,true);});
        EccControlSystemStatus retained; retained.instanceId=h; retained.snapshotSha256=h;
        retained.controller.state=5; retained.controller.operationId="start";
        EccControlDataNodeStatus retainedNode; retainedNode.agentId="A"; retainedNode.instanceId=h;
        retainedNode.planSha256=h; retainedNode.state="ready"; retainedNode.sessionId=h;
        EccControlReceiverStatus retainedChild; retainedChild.id="C0"; retainedChild.pid=42;
        retainedChild.sessionId=h; retainedChild.outputDirectory="/data/session";
        retainedNode.receivers.push_back(retainedChild); retained.nodes.push_back(retainedNode);
        Require(FailedBoundReceivers(retained,observed,h).empty(),"Running process inferred as failure");
        auto failure=observed; failure.nodes[0].receivers[0].state="exited"; failure.nodes[0].receivers[0].exitCode=7;
        Require(FailedBoundReceivers(retained,failure,h)==std::vector<std::string>{"C0"},"Bound failed exit was ignored");
        failure.nodes[0].receivers[0].exitCode=0;
        Require(FailedBoundReceivers(retained,failure,h).empty(),"Normal exit was inferred as failure");
        failure.nodes[0].receivers[0].exitCode=-1; failure.nodes[0].receivers[0].signal=9;
        Require(FailedBoundReceivers(retained,failure,h)==std::vector<std::string>{"C0"},"Bound signal exit was ignored");
        const auto mismatch=[&](const EccControlSystemStatus &b,const EccControlReceiverObservation &o,const std::string &hash) {
            Require(FailedBoundReceivers(b,o,hash).empty(),"Unbound/stale receiver exit triggered failure");
        };
        {auto o=failure;o.instanceId=std::string(64,'b');mismatch(retained,o,h);}
        {auto o=failure;o.snapshotSha256=std::string(64,'b');mismatch(retained,o,h);}
        mismatch(retained,failure,std::string(64,'b'));
        {auto b=retained;b.operationActive=true;mismatch(b,failure,h);}
        {auto b=retained;b.controller.transition=5;mismatch(b,failure,h);}
        {auto o=failure;o.nodes[0].available=false;mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].busy=true;mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].instanceId=std::string(64,'b');mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].planSha256=std::string(64,'b');mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].lifecycle.sessionId=std::string(64,'b');mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].lifecycle.receivers[0].sessionId=std::string(64,'b');mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].lifecycle.receivers[0].outputDirectory="/other/session";mismatch(retained,o,h);}
        {auto o=failure;o.nodes[0].receivers[0].pid=43;mismatch(retained,o,h);}
        {auto b=retained;b.nodes[0].receivers[0].reaped=true;mismatch(b,failure,h);}
        {auto b=retained;b.nodes[0].state="stopped";mismatch(b,failure,h);}
        auto heldBound=system;
        const auto change=[&](std::string &bytes,const std::string &from,const std::string &to) {
            const auto at=bytes.find(from);Require(at!=std::string::npos,"Receiver cache fixture replacement missing");bytes.replace(at,from.size(),to);
        };
        change(heldBound,"\"state\":\"stopped\"","\"state\":\"ready\"");
        change(heldBound,"\"exit_code\":0","\"exit_code\":-1");change(heldBound,"\"reaped\":true","\"reaped\":false");
        change(heldBound,"\"seal_verified\":true","\"seal_verified\":false");
        auto heldObservation=observation;
        change(heldObservation,"\"state\":\"running\"","\"state\":\"exited\"");
        const auto processCode=heldObservation.rfind("\"exit_code\":-1");Require(processCode!=std::string::npos,"Receiver process exit code missing");
        heldObservation.replace(processCode,14,"\"exit_code\":7");
        const auto cache=[&](const std::string &phase,const std::string &parent,const std::string &seen,const std::string &instance) {
            return "{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-failure\",\"instance_id\":"+EncodeJsonString(instance)+
                ",\"snapshot_sha256\":"+EncodeJsonString(h)+",\"enabled\":true,\"latched\":true,\"operation_id\":\"internal-receiver-fixture\",\"phase\":"+
                EncodeJsonString(phase)+",\"evidence\":{\"bound_status\":"+EncodeJsonString(parent,1024*1024)+",\"observation\":"+EncodeJsonString(seen,1024*1024)+"}}";
        };
        const auto heldCache=cache("complete",heldBound,heldObservation,h);
        Require(DecodeEccControlReceiverFailureStatus(heldCache).failedIds==std::vector<std::string>{"C0"},"Receiver failure typed cache lost original trigger");
        for (const auto &phase:{"held","accepted","failed","unknown"})Require(DecodeEccControlReceiverFailureStatus(cache(phase,heldBound,heldObservation,h)).latched,"Held policy phase lost");
        Require(DecodeEccControlReceiverFailureStatus(cache("reopened-held",heldBound,heldObservation,std::string(64,'b'))).bound.instanceId==h,"Reopened policy erased original instance");
        Reject([&] {DecodeEccControlReceiverFailureStatus(cache("complete",heldBound,heldObservation,std::string(64,'b')));});
        Reject([&] {DecodeEccControlReceiverFailureStatus(cache("watching",heldBound,heldObservation,h));});
        Reject([&] {DecodeEccControlReceiverFailureStatus(cache("complete",heldBound,observation,h));});
        Reject([&] {auto seen=heldObservation;change(seen,"\"state\":\"exited\"","\"state\":\"unavailable\"");DecodeEccControlReceiverFailureStatus(cache("complete",heldBound,seen,h));});
        Reject([&] {auto parent=heldBound;change(parent,"\"pid\":42","\"pid\":43");DecodeEccControlReceiverFailureStatus(cache("complete",parent,heldObservation,h));});
        Reject([&] {auto b=heldCache;change(b,"\"latched\":true","\"latched\":false");DecodeEccControlReceiverFailureStatus(b);});
        Reject([&] {auto b=heldCache;change(b,"\"enabled\":true","\"enabled\":false");DecodeEccControlReceiverFailureStatus(b);});
        Reject([&] {DecodeEccControlReceiverFailureStatus(std::string(1024*1024+1,' '));});
        Reject([&] {auto b=heldCache;change(b,"\"phase\":\"complete\"","\"phase\":\"recovered\"");DecodeEccControlReceiverFailureStatus(b);});
        Reject([&] {auto b=heldCache;change(b,"internal-receiver-fixture","external-control");DecodeEccControlReceiverFailureStatus(b);});
        const auto waiting="{\"schema\":1,\"kind\":\"getdaq.ecc-receiver-failure\",\"instance_id\":"+EncodeJsonString(h)+
            ",\"snapshot_sha256\":"+EncodeJsonString(h)+",\"enabled\":true,\"latched\":false,\"operation_id\":\"\",\"phase\":\"watching\",\"evidence\":null}";
        Require(!DecodeEccControlReceiverFailureStatus(waiting).hasEvidence,"Watching policy invented exit evidence");
        Reject([&] {auto b=waiting;change(b,"\"phase\":\"watching\"","\"phase\":\"complete\"");DecodeEccControlReceiverFailureStatus(b);});
        unsigned int actual=0;
        const std::string alarm="{\"schema\":1,\"kind\":\"getdaq.ecc-alarms\",\"instance_id\":\""+r.instanceId+"\",\"snapshot_sha256\":\""+r.snapshotSha256+
            "\",\"enabled\":true,\"latched\":true,\"operation_id\":\"internal-alarm-test\",\"phase\":\"complete\",\"observation\":{\"controller_state\":5,\"controller_outcome_unknown\":false,\"controller_operation_id\":\"start\",\"stop_requested\":true,\"nodes\":[{\"id\":\"C0\",\"subscription\":2,\"received\":\"18446744073709551615\",\"durable\":\"9007199254740993\",\"lost\":\"0\",\"invalid\":\"0\",\"pending\":1,\"alarm_mask\":1,\"stop_requested\":true,\"queue_overflow\":false,\"audit_failed\":false,\"budget_exceeded\":false}]}}";
        const auto alarmStatus=DecodeEccControlAlarmStatus(alarm); Require(alarmStatus.latched && alarmStatus.nodes.size()==1 && alarmStatus.nodes[0].received==std::numeric_limits<std::uint64_t>::max(),"Alarm large counter/hold lost");
        Reject([&] { auto b=alarm; b.replace(b.find("18446744073709551615"),20,"18446744073709551616"); DecodeEccControlAlarmStatus(b); });
        Reject([&] { auto b=alarm; b.replace(b.find("\"subscription\":2"),16,"\"subscription\":6"); DecodeEccControlAlarmStatus(b); });
        Reject([&] { auto b=alarm; b.replace(b.find("\"latched\":true"),14,"\"latched\":false"); DecodeEccControlAlarmStatus(b); });
        Reject([&] { auto b=alarm; b.replace(b.find("\"received\":\"18446744073709551615\""),33,"\"received\":18446744073709551615"); DecodeEccControlAlarmStatus(b); });
        for (int i=1;i<argc;++i) {
            const std::string root=argv[i]; DecodeEccControlIdentity(Read(root+"/identity.json")); DecodeEccControlSystemStatus(Read(root+"/final-status.json")); ++actual;
            std::ifstream journal(root+"/controller/network/operations.jsonl"); Require(bool(journal),"Actual network journal missing"); std::string row;
            while (std::getline(journal,row)) {
                JsonLimits limits;limits.stringBytes=1024*1024;const auto v=ParseStrictJson(row,limits);
                if (v.At("event").text=="accepted") {
                    const auto request=DecodeEccControlRequest(v.At("request").text);
                    Require(EncodeEccControlRequest(request)==v.At("request").text && EccControlRequestKey(request)==v.At("request_key").text,"Actual accepted request/key differs"); ++actual;
                }
                if (v.At("event").text=="complete" || v.At("event").text=="failed") { DecodeEccControlControllerStatus(v.At("result").text); ++actual; }
            }
        }
        std::cout<<"PASS portable C++11 ECC codec: strict fields, enums, bounds, canonical bytes, key/result binding and unknown history; actual records "<<actual<<"; no Qt/Ice/POSIX backend\n";
    } catch (const std::exception &e) { std::cerr<<"FAIL "<<e.what()<<'\n'; return 1; }
}
