#include "controlhttp.h"
#include "planio.h"
#include "configurationsha256.h"
#include "systemexperimentconfiguration.h"
#include <functional>
#include <fstream>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>
namespace {
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;
void Require(bool yes,const std::string &message) { if (!yes) throw std::runtime_error(message); }
void Reject(const std::function<void()> &action) {
    try { action(); } catch (const std::exception &) { return; }
    throw std::runtime_error("Expected strict validation failure");
}
void Codec() {
    std::uint32_t parsedAddress=0;
    Require(ParseCanonicalIPv4("192.0.2.10",parsedAddress) && parsedAddress==0xc000020au,
        "Shared canonical IPv4 parser returned the wrong host-order address");
    for (const auto &invalid:std::vector<std::string>{"", ".1.2.3", "1.2.3", "1.2.3.4.",
            "01.2.3.4", "1.2.3.256", "1.2.-1.4", "1.2.3.4:5", " 1.2.3.4"})
        Require(!ParseCanonicalIPv4(invalid,parsedAddress),"Shared IPv4 parser accepted a noncanonical address: "+invalid);
    DataSupervisorOptions options; options.auditRoot="/srv/getdaq/control"; options.deploymentId="test"; options.receiverExecutable="/opt/getdaq/getdaq-receiver";
    options.configurationSha256=std::string(64,'a'); options.artifactSha256=std::string(64,'b');
    ReceiverSourcePlan source; source.id="C0"; source.receiverAddress="192.0.2.10:23001"; source.bindAddress="0.0.0.0"; source.outputRoot="/srv/getdaq/data";
    source.dataPort=23001; source.statusPort=23002; options.sources.push_back(source);
    const auto plan=EncodeOptions(options,std::string(64,'c')); const auto decoded=DecodeSupervisorPlan(plan);
    Require(EncodeOptions(decoded.options,decoded.executableSha256)==plan,"Plan canonical round trip differs");
    Require(decoded.options.sources[0].fileNaming=="native" && plan.find("file_naming")==std::string::npos,"Default file naming changed the legacy canonical plan");
    Require(decoded.options.sources[0].storageMode=="frame-storage" && plan.find("storage_mode")==std::string::npos,
        "Default frame storage changed the legacy canonical plan");
    auto getOptions=options;getOptions.sources[0].fileNaming="get";
    const auto getPlan=EncodeOptions(getOptions,std::string(64,'c'));
    Require(getPlan!=plan && DecodeSupervisorPlan(getPlan).options.sources[0].fileNaming=="get","GET naming absent from plan scope");
    Require(EncodeOptions(DecodeSupervisorPlan(getPlan).options,std::string(64,'c'))==getPlan,"GET plan round trip differs");
    auto zbufOptions=options;zbufOptions.sources[0].inputProtocol="ZBUF";const auto zbufPlan=EncodeOptions(zbufOptions,std::string(64,'c'));
    Require(zbufPlan!=plan && DecodeSupervisorPlan(zbufPlan).options.sources[0].inputProtocol=="ZBUF","ZBUF protocol absent from fixed plan");
    Require(EncodeOptions(DecodeSupervisorPlan(zbufPlan).options,std::string(64,'c'))==zbufPlan,"ZBUF canonical plan differs");
    Require(plan.find("input_protocol")==std::string::npos,"Default TCP changed canonical plan bytes");
    auto byteOptions=options;byteOptions.sources[0].storageMode="byte-storage";
    const auto bytePlan=EncodeOptions(byteOptions,std::string(64,'c'));
    const auto decodedBytePlan=DecodeSupervisorPlan(bytePlan);
    Require(bytePlan!=plan && bytePlan.find("\"storage_mode\":\"byte-storage\"")!=std::string::npos &&
        decodedBytePlan.options.sources[0].storageMode=="byte-storage" &&
        EncodeOptions(decodedBytePlan.options,decodedBytePlan.executableSha256)==bytePlan,
        "Byte-storage mode is missing from canonical plan identity or round trip");
    for(const auto &invalid:std::vector<std::string>{"BYTE-STORAGE","raw",""}) {
        auto v=ParseStrictJson(bytePlan);v.members["sources"].items[0].members["storage_mode"].text=invalid;
        Reject([&] {DecodeSupervisorPlan(v);});
    }
    {
        auto invalid=byteOptions;invalid.sources[0].inputProtocol="FDT";
        Reject([&] {DecodeSupervisorPlan(EncodeOptions(invalid,std::string(64,'c')));});
    }
    {
        auto invalid=byteOptions;invalid.sources[0].exportPort=23003;
        Reject([&] {DecodeSupervisorPlan(EncodeOptions(invalid,std::string(64,'c')));});
    }
    auto reserveOptions=options;reserveOptions.sources[0].minFreeMiB=256;const auto reservePlan=EncodeOptions(reserveOptions,std::string(64,'c'));
    Require(reservePlan!=plan && reservePlan.find("\"min_free_mib\":256")!=std::string::npos,"Disk reserve absent from fixed plan identity");
    Require(DecodeSupervisorPlan(reservePlan).options.sources[0].minFreeMiB==256 &&
        EncodeOptions(DecodeSupervisorPlan(reservePlan).options,std::string(64,'c'))==reservePlan,"Disk reserve plan round trip differs");
    Require(DecodeSupervisorPlan(plan).options.sources[0].minFreeMiB==0,"Legacy plan did not retain disabled disk reserve default");
    {auto v=ParseStrictJson(reservePlan);v.members["sources"].items[0].members["min_free_mib"].text="1048577";Reject([&] {DecodeSupervisorPlan(v);});}
    auto fdtOptions=options;fdtOptions.sources[0].inputProtocol="FDT";
    const auto fdtPlan=EncodeOptions(fdtOptions,std::string(64,'c'));
    Require(EncodeOptions(DecodeSupervisorPlan(fdtPlan).options,std::string(64,'c'))==fdtPlan && fdtPlan!=plan,"FDT cycle mode absent from canonical plan");
    auto iceOptions=options;iceOptions.sources[0].inputProtocol="ICE";iceOptions.iceReceiverExecutable="/tmp/getdaq-ice-receiver";iceOptions.iceReceiverSha256=std::string(64,'d');
    const auto icePlan=EncodeOptions(iceOptions,std::string(64,'c'));
    Require(EncodeOptions(DecodeSupervisorPlan(icePlan).options,std::string(64,'c'))==icePlan && icePlan!=plan,"ICE executable/source binding missing");
    {auto v=ParseStrictJson(icePlan);v.members.erase("ice_executable_sha256");Reject([&] {DecodeSupervisorPlan(v);});}
    {auto v=ParseStrictJson(icePlan);v.members.erase("ice_executable");Reject([&] {DecodeSupervisorPlan(v);});}
    for(const auto &invalid:std::vector<std::string>{"zbuf","fdt","ICE",""}) {
        auto v=ParseStrictJson(zbufPlan);v.members["sources"].items[0].members["input_protocol"].text=invalid;
        Reject([&] {DecodeSupervisorPlan(v);});
    }
    {auto v=ParseStrictJson(zbufPlan);auto &protocol=v.members["sources"].items[0].members["input_protocol"];protocol.type=JsonValue::Boolean;protocol.text="true";Reject([&] {DecodeSupervisorPlan(v);});}
    for(const auto &invalid:std::vector<std::string>{"GET","other",""}) {
        auto v=ParseStrictJson(getPlan);v.members["sources"].items[0].members["file_naming"].text=invalid;
        Reject([&] {DecodeSupervisorPlan(v);});
    }
    {auto v=ParseStrictJson(getPlan);auto &naming=v.members["sources"].items[0].members["file_naming"];
     naming.type=JsonValue::Number;naming.text="1";Reject([&] {DecodeSupervisorPlan(v);});}
    Reject([&] { DecodeSupervisorPlan(plan.substr(0,plan.find_last_of('}'))+",\"shell\":\"sh\"}"); });
    Reject([&] { auto v=ParseStrictJson(plan); v.members["finish_timeout_ms"].text="4294967296"; DecodeSupervisorPlan(v); });
    Reject([&] { auto v=ParseStrictJson(plan); v.members["schema"].text="01"; DecodeSupervisorPlan(v); });
    for (const auto &address:std::vector<std::string>{"192.000.2.10:23001","192.0.2:23001","224.0.0.1:23001",
            "239.1.2.3:23001","240.0.0.1:23001","255.255.255.255:23001","0.0.0.0:23001"}) {
        auto v=ParseStrictJson(plan);v.members["sources"].items[0].members["receiver_address"].text=address;
        Reject([&] {DecodeSupervisorPlan(v);});
    }
    SupervisorControlRequest request; request.operationId="start-1"; request.planSha256=ConfigurationSha256(plan); request.instanceId=std::string(64,'d');
    request.controllerId="controller"; request.controllerSnapshotSha256=std::string(64,'e'); request.controllerEpoch=std::string(64,'f');
    request.configurationSha256=options.configurationSha256; request.artifactSha256=options.artifactSha256; request.parentAuditReference="controller-run:0001";
    request.command=SupervisorCommand::Start;
    const auto bytes=EncodeSupervisorControlRequest(request);
    Require(SupervisorControlRequestKey(request)==SupervisorControlRequestKey(DecodeSupervisorControlRequest(bytes)),"Request round trip differs");
    const auto original=SupervisorControlRequestKey(request);
    Require(bytes.find("successor_controller_")==std::string::npos,"Legacy request acquired handoff fields");
    {
        auto stagedOptions=options;stagedOptions.auditRoot="/"+std::string(3200,'r');stagedOptions.sources[0].outputRoot="/"+std::string(3200,'o');
        const auto stagedPlan=EncodeOptions(stagedOptions,std::string(64,'c'));
        auto staged=request;staged.command=SupervisorCommand::StageSuccessorPlan;staged.successorPlanFile="/tmp/staged-next-plan.json";
        staged.successorPlanSha256=ConfigurationSha256(stagedPlan);staged.successorPlanJson=stagedPlan;staged.parentAuditReference=std::string(3000,'p');
        const auto stagedBytes=EncodeSupervisorControlRequest(staged);
        Require(stagedBytes.size()>8192 && stagedBytes.size()<=2U*1024U*1024U &&
            EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(stagedBytes))==stagedBytes &&
            SupervisorCommandMutates(staged.command) && SupervisorControlRequestMaxBytes(SupervisorCommand::Start)==8192,
            "Remote successor staging must carry a bounded canonical plan without widening ordinary control requests");
        auto tampered=staged;tampered.successorPlanSha256=std::string(64,'9');
        Reject([&]{EncodeSupervisorControlRequest(tampered);});
        tampered=staged;tampered.successorPlanJson+=" ";
        Reject([&]{EncodeSupervisorControlRequest(tampered);});
    }
    auto handoff=request;handoff.command=SupervisorCommand::HandoffControl;
    handoff.successorControllerSnapshotSha256=std::string(64,'1');handoff.successorControllerEpoch=std::string(64,'2');
    const auto handoffBytes=EncodeSupervisorControlRequest(handoff);
    Require(SupervisorCommandMutates(handoff.command) &&
        EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(handoffBytes))==handoffBytes,"Handoff lost its protected canonical intent");
    auto otherSuccessor=handoff;otherSuccessor.successorControllerEpoch=std::string(64,'3');
    Require(SupervisorControlRequestKey(otherSuccessor)!=SupervisorControlRequestKey(handoff),"Successor epoch absent from handoff binding");
    otherSuccessor=handoff;otherSuccessor.successorControllerSnapshotSha256=std::string(64,'4');
    Require(SupervisorControlRequestKey(otherSuccessor)!=SupervisorControlRequestKey(handoff),"Successor snapshot absent from handoff binding");
    Reject([&]{auto invalid=handoff;invalid.successorControllerEpoch.clear();EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=handoff;invalid.successorControllerEpoch=request.controllerEpoch;invalid.successorControllerSnapshotSha256=request.controllerSnapshotSha256;EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=handoff;invalid.command=SupervisorCommand::Start;EncodeSupervisorControlRequest(invalid);});
    auto planRetirement=handoff;planRetirement.command=SupervisorCommand::RetirePlan;planRetirement.expectedSessionId=std::string(64,'a');
    planRetirement.successorPlanFile="/private/reviewed/next-plan.json";planRetirement.successorPlanSha256=std::string(64,'5');
    const auto planRetirementBytes=EncodeSupervisorControlRequest(planRetirement);
    Require(EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(planRetirementBytes))==planRetirementBytes && SupervisorCommandMutates(planRetirement.command),"Plan retirement lacks canonical protected request");
    for (unsigned int field=0;field<3;++field) {
        auto different=planRetirement;
        if(field==0)different.successorPlanFile="/private/reviewed/other-plan.json";
        if(field==1)different.successorPlanSha256=std::string(64,'6');
        if(field==2)different.expectedSessionId=std::string(64,'7');
        Require(SupervisorControlRequestKey(different)!=SupervisorControlRequestKey(planRetirement),"Plan retirement input/cycle absent from request key");
    }
    Reject([&]{auto invalid=planRetirement;invalid.successorPlanFile="relative.json";EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=planRetirement;invalid.expectedSessionId.clear();EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=planRetirement;invalid.parentFailureOperationId="failed-run";EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=planRetirement;invalid.command=SupervisorCommand::HandoffControl;EncodeSupervisorControlRequest(invalid);});

    auto completion=planRetirement;completion.command=SupervisorCommand::CompletePlanRetirement;
    completion.failedRetirementOperationId=planRetirement.operationId;completion.operationId="explicit-completion";
    const auto completionBytes=EncodeSupervisorControlRequest(completion);
    Require(EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(completionBytes))==completionBytes &&
        EncodeSupervisorControlRequest(OriginalPlanRetirementRequest(completion))==planRetirementBytes,
        "Completion lost exact original target/owner/cycle binding");
    Require(SupervisorControlRequestKey(completion)!=SupervisorControlRequestKey(planRetirement),"Completion aliases original request");
    Reject([&]{auto invalid=completion;invalid.failedRetirementOperationId.clear();EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=completion;invalid.operationId=invalid.failedRetirementOperationId;EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=completion;invalid.command=SupervisorCommand::RetirePlan;EncodeSupervisorControlRequest(invalid);});
    auto differentCompletion=completion;differentCompletion.failedRetirementOperationId="another-failed-retirement";
    Require(SupervisorControlRequestKey(differentCompletion)!=SupervisorControlRequestKey(completion),"Original failure reference absent from completion key");

    auto handoffCompletion=handoff;handoffCompletion.command=SupervisorCommand::CompleteControlHandoff;
    handoffCompletion.failedHandoffOperationId=handoff.operationId;handoffCompletion.operationId="explicit-handoff-completion";
    const auto handoffCompletionBytes=EncodeSupervisorControlRequest(handoffCompletion);
    Require(SupervisorCommandMutates(handoffCompletion.command) &&
        EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(handoffCompletionBytes))==handoffCompletionBytes &&
        EncodeSupervisorControlRequest(OriginalControlHandoffRequest(handoffCompletion))==handoffBytes,
        "Handoff completion lost exact original request/successor binding");
    Require(SupervisorControlRequestKey(handoffCompletion)!=SupervisorControlRequestKey(handoff),"Handoff completion aliases original request");
    Reject([&]{auto invalid=handoffCompletion;invalid.failedHandoffOperationId.clear();EncodeSupervisorControlRequest(invalid);});
    Reject([&]{auto invalid=handoffCompletion;invalid.operationId=invalid.failedHandoffOperationId;EncodeSupervisorControlRequest(invalid);});
    auto changedHandoffCompletion=handoffCompletion;changedHandoffCompletion.successorControllerEpoch=std::string(64,'3');
    Require(SupervisorControlRequestKey(changedHandoffCompletion)!=SupervisorControlRequestKey(handoffCompletion) &&
        EncodeSupervisorControlRequest(OriginalControlHandoffRequest(changedHandoffCompletion))!=handoffBytes,
        "Changed handoff completion target did not remain bound to a distinct exact original request");
    auto otherFailedHandoff=handoffCompletion;otherFailedHandoff.failedHandoffOperationId="another-failed-handoff";
    Require(SupervisorControlRequestKey(otherFailedHandoff)!=SupervisorControlRequestKey(handoffCompletion),"Failed handoff reference absent from completion key");

    auto abort=request;abort.command=SupervisorCommand::AbortFailedSession;abort.expectedSessionId=std::string(64,'a');
    Require(EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(EncodeSupervisorControlRequest(abort)))==EncodeSupervisorControlRequest(abort),"Failed-session abort canonical request differs");
    Require(SupervisorCommandMutates(abort.command),"Failed-session abort must retain owner/operation protection");
    auto retire=abort;retire.command=SupervisorCommand::RetireFailedSession;retire.parentFailureOperationId="original-failed-run";
    const auto retirementBytes=EncodeSupervisorControlRequest(retire);
    Require(EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(retirementBytes))==retirementBytes,"Retirement canonical request differs");
    Require(SupervisorCommandMutates(retire.command) && SupervisorControlRequestKey(retire)!=SupervisorControlRequestKey(abort),"Retirement must have a distinct protected intent");
    auto otherRetire=retire;otherRetire.parentFailureOperationId="another-failed-run";
    Require(SupervisorControlRequestKey(otherRetire)!=SupervisorControlRequestKey(retire),"Retirement parent absent from request binding");
    Reject([&] {auto bad=retire;bad.expectedSessionId.clear();EncodeSupervisorControlRequest(bad);});
    auto otherAbort=abort;otherAbort.expectedSessionId=std::string(64,'b');Require(SupervisorControlRequestKey(otherAbort)!=SupervisorControlRequestKey(abort),"Expected session absent from abort request key");
    auto parentAbort=abort;parentAbort.parentFailureOperationId="parent-failed-start";
    Require(SupervisorControlRequestKey(parentAbort)!=SupervisorControlRequestKey(abort),"Parent failure absent from request key");
    Require(DecodeSupervisorControlRequest(EncodeSupervisorControlRequest(parentAbort)).parentFailureOperationId==parentAbort.parentFailureOperationId,"Parent failure reference lost");
    Reject([&] {auto invalid=parentAbort;invalid.command=SupervisorCommand::Start;EncodeSupervisorControlRequest(invalid);});
    Reject([&] {auto invalid=abort;invalid.expectedSessionId.clear();EncodeSupervisorControlRequest(invalid);});
    Reject([&] {auto invalid=request;invalid.expectedSessionId=std::string(64,'a');EncodeSupervisorControlRequest(invalid);});
    auto catalog=request; catalog.command=SupervisorCommand::Catalog; catalog.catalogExperiment="Test"; catalog.catalogRunNumber=17; catalog.catalogSessionId=std::string(64,'a');
    Require(EncodeSupervisorControlRequest(DecodeSupervisorControlRequest(EncodeSupervisorControlRequest(catalog)))==EncodeSupervisorControlRequest(catalog),"Catalog request canonical round trip differs");
    auto replacementCatalog=catalog;replacementCatalog.catalogReplaceExisting=true;
    Require(DecodeSupervisorControlRequest(EncodeSupervisorControlRequest(replacementCatalog)).catalogReplaceExisting &&
        SupervisorControlRequestKey(replacementCatalog)!=SupervisorControlRequestKey(catalog),"Explicit replacement authorization absent from supervisor catalog intent");
    auto replacementPrepare=replacementCatalog;replacementPrepare.catalogPhase="prepare";replacementPrepare.catalogReplacementId="stable-replacement";
    auto replacementPublish=replacementPrepare;replacementPublish.operationId="replacement-publish";replacementPublish.catalogPhase="publish";
    auto replacementFinalize=replacementPrepare;replacementFinalize.operationId="replacement-finalize";replacementFinalize.catalogPhase="finalize";
    const auto prepareBytes=EncodeSupervisorControlRequest(replacementPrepare),publishBytes=EncodeSupervisorControlRequest(replacementPublish),
        finalizeBytes=EncodeSupervisorControlRequest(replacementFinalize);
    const auto decodedPrepare=DecodeSupervisorControlRequest(prepareBytes),decodedPublish=DecodeSupervisorControlRequest(publishBytes),
        decodedFinalize=DecodeSupervisorControlRequest(finalizeBytes);
    Require(decodedPrepare.catalogPhase=="prepare" && decodedPublish.catalogPhase=="publish" &&
        decodedFinalize.catalogPhase=="finalize" &&
        decodedPrepare.catalogReplacementId==decodedPublish.catalogReplacementId &&
        decodedPublish.catalogReplacementId==decodedFinalize.catalogReplacementId &&
        SupervisorControlRequestKey(decodedPrepare)!=SupervisorControlRequestKey(decodedPublish) &&
        SupervisorControlRequestKey(decodedPublish)!=SupervisorControlRequestKey(decodedFinalize),
        "Replacement prepare/publish/finalize lost stable identity or phase-specific request binding");
    Reject([&] { auto invalid=replacementPrepare;invalid.catalogReplacementId.clear();EncodeSupervisorControlRequest(invalid); });
    Reject([&] { auto invalid=replacementPrepare;invalid.catalogReplaceExisting=false;EncodeSupervisorControlRequest(invalid); });
    auto changedCatalog=catalog; ++changedCatalog.catalogRunNumber; Require(SupervisorControlRequestKey(changedCatalog)!=SupervisorControlRequestKey(catalog),"Catalog Run absent from request scope");
    changedCatalog=catalog; changedCatalog.catalogSessionId=std::string(64,'b'); Require(SupervisorControlRequestKey(changedCatalog)!=SupervisorControlRequestKey(catalog),"Catalog session absent from request scope");
    Reject([&] { auto invalid=request; invalid.catalogExperiment="Test"; EncodeSupervisorControlRequest(invalid); });
    auto unused=request;unused.command=SupervisorCommand::Finish;unused.unusedSourceIds={"C0","C1"};
    Require(DecodeSupervisorControlRequest(EncodeSupervisorControlRequest(unused)).unusedSourceIds==unused.unusedSourceIds,"Unused source intent lost in canonical request");
    auto used=unused;used.unusedSourceIds.clear();Require(SupervisorControlRequestKey(used)!=SupervisorControlRequestKey(unused),"Unused source IDs absent from request identity");
    Reject([&] {auto invalid=unused;invalid.unusedSourceIds={"C0","C0"};EncodeSupervisorControlRequest(invalid);});
    Reject([&] {auto invalid=unused;invalid.unusedSourceIds={"C1","C0"};EncodeSupervisorControlRequest(invalid);});
    Reject([&] {auto invalid=unused;invalid.command=SupervisorCommand::Start;EncodeSupervisorControlRequest(invalid);});
    for (std::string *value:{&request.instanceId,&request.planSha256,&request.controllerSnapshotSha256,&request.controllerEpoch,&request.configurationSha256,&request.artifactSha256}) {
        const auto saved=*value; (*value)[0]=(*value)[0]=='0' ? '1' : '0'; Require(SupervisorControlRequestKey(request)!=original,"Digest scope absent from request key"); *value=saved;
    }
    Reject([&] { DecodeSupervisorControlRequest(bytes.substr(0,bytes.find_last_of('}'))+",\"executable\":\"/bin/sh\"}"); });
    Reject([&] { DecodeSupervisorControlRequest(bytes.substr(0,bytes.find_last_of('}'))+",\"operation_id\":\"different\"}"); });
    Reject([&] { SupervisorOperationId("../outside"); });
    DataSupervisorStatus status; status.state=DataSupervisorStatus::Stopped; status.planSha256=request.planSha256;
    ManagedReceiverStatus receiver; receiver.id="C0"; receiver.receivedBytes=9007199254740993ULL; receiver.frames=UINT64_MAX; status.receivers.push_back(receiver);
    const auto encoded=ParseStrictJson(EncodeSupervisorStatus(status)); const auto &result=encoded.At("receivers").items.at(0);
    Require(Unsigned(result.At("received_bytes"),true)==9007199254740993ULL && Unsigned(result.At("frames"),true)==UINT64_MAX,"64-bit status lost precision");
    Require(EncodeSupervisorStatus(DecodeSupervisorStatus(encoded))==EncodeSupervisorStatus(status),"Status canonical round trip differs");
    auto retiredStatus=status;retiredStatus.state=DataSupervisorStatus::Retired;
    retiredStatus.sessionId=std::string(64,'a');retiredStatus.receivers[0].sessionId=retiredStatus.sessionId;
    retiredStatus.receivers[0].pid=123;retiredStatus.receivers[0].exitCode=137;retiredStatus.receivers[0].reaped=true;
    Require(DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(retiredStatus))).state==DataSupervisorStatus::Retired,"Retired state omitted");
    Reject([&] {auto bad=retiredStatus;bad.receivers[0].reaped=false;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(bad)));});
    Reject([&] {auto bad=retiredStatus;bad.needsReconciliation=true;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(bad)));});
    auto iceStatus=status;auto &iceReceiver=iceStatus.receivers[0];iceReceiver.sourceEndConfirmed=true;iceReceiver.reaped=true;iceReceiver.sealVerified=true;iceReceiver.exitCode=0;
    Require(DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(iceStatus))).receivers[0].sourceEndConfirmed,"ICE source end proof omitted");
    Reject([&] {auto invalid=iceStatus;invalid.receivers[0].reaped=false;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(invalid)));});
    Reject([&] {auto invalid=iceStatus;invalid.receivers[0].sourceUnusedConfirmed=true;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(invalid)));});
    auto unusedStatus=status;auto &empty=unusedStatus.receivers[0];empty.sourceUnusedConfirmed=true;empty.reaped=true;empty.sealVerified=true;empty.exitCode=0;empty.receivedBytes=0;empty.frames=0;empty.files=0;
    Require(DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(unusedStatus))).receivers[0].sourceUnusedConfirmed,"Unused status proof was omitted");
    Reject([&] {auto invalid=unusedStatus;invalid.receivers[0].receivedBytes=1;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(invalid)));});
    Reject([&] {auto invalid=unusedStatus;invalid.receivers[0].reaped=false;DecodeSupervisorStatus(ParseStrictJson(EncodeSupervisorStatus(invalid)));});
    Reject([&] { auto invalid=encoded; invalid.members["receivers"].items[0].members["frames"].type=JsonValue::Number; DecodeSupervisorStatus(invalid); });
    DataSupervisorObservation observation; observation.lifecycle=status; observation.observedAtUnixMs=UINT64_MAX;
    ReceiverProcessObservation process; process.id="C0"; process.state="not-started"; observation.receivers.push_back(process);
    const auto observed=ParseStrictJson(EncodeSupervisorObservation(observation));
    Require(EncodeSupervisorObservation(DecodeSupervisorObservation(observed))==EncodeSupervisorObservation(observation),"Observation exact 64-bit round trip differs");
    Reject([&] {auto v=observed;v.members["busy"].type=JsonValue::String;v.members["busy"].text="false";DecodeSupervisorObservation(v);});
    Reject([&] {auto v=observed;v.members["observed_at_unix_ms"].type=JsonValue::Number;DecodeSupervisorObservation(v);});
    Reject([&] {auto v=observed;v.members["busy"].text="true";DecodeSupervisorObservation(v);});
    Reject([&] {auto v=observed;v.members["receivers"].items.clear();DecodeSupervisorObservation(v);});
    for (const auto &field:{"id","state","exit_code","signal"}) {
        auto v=observed;auto &row=v.members["receivers"].items[0];
        row.members[field].text=std::string(field)=="id" ? "other" : std::string(field)=="state" ? "ready" : "999";
        Reject([&] {DecodeSupervisorObservation(v);});
    }
    {auto v=observed;v.members["receivers"].items[0].members["state"].text="exited";Reject([&] {DecodeSupervisorObservation(v);});}
    {auto v=observed;v.members["receivers"].items[0].members["state"].text="unavailable";Reject([&] {DecodeSupervisorObservation(v);});}
    std::cout<<"PASS strict plan/request scope and exact 64-bit status\n";
}
void Credential(const std::string &root) {
    CreateDirectory(root); const auto path=root+"/token"; const auto bytes=RandomSession()+"\n"; WriteNew(path,bytes);
    SupervisorCredential credential(path);
    Require(credential.Authorize("Bearer "+bytes.substr(0,64)) && !credential.Authorize("Bearer "+std::string(64,'0')) && !credential.Authorize(""),"Credential comparison failed");
    Require(!::chmod(path.c_str(),0644),"Credential permission fixture"); Reject([&] { credential.Check(); });
    Require(!::chmod(path.c_str(),0600),"Credential permission restoration"); Reject([&] { credential.Check(); });
    SupervisorCredential reopened(path);
    Require(!::rename(path.c_str(),(path+".old").c_str()),"Credential inode fixture"); WriteNew(path,bytes); Reject([&] { reopened.Check(); });
    Require(!::unlink(path.c_str()) && !::rename((path+".old").c_str(),path.c_str()),"Credential inode restoration"); Reject([&] { reopened.Check(); });
    const auto symbolic=root+"/link"; Require(!::symlink(path.c_str(),symbolic.c_str()),"Credential symlink fixture"); Reject([&] { SupervisorCredential linked(symbolic); });
    std::cout<<"PASS credential permission/inode/symlink checks and latched restoration\n";
}
}
int main(int argc,char **argv) {
    try {
        if (argc==2 && std::string(argv[1])=="codec") { Codec(); return 0; }
        if (argc==3 && std::string(argv[1])=="credential") { Credential(argv[2]); return 0; }
        if (argc<7 || argc>8 || std::string(argv[1])!="http") throw std::invalid_argument("Use codec, credential NEW_DIR, or http IPv4 PORT TOKEN_FILE METHOD [ID/REQUEST_FILE]");
        SupervisorHttpOptions options; options.address=argv[2]; options.port=unsigned(std::stoul(argv[3])); options.credentialFile=argv[4]; options.timeoutMs=1000;
        const std::string method=argv[5];
        // 固定最后参数是测试截止时间，便于注入迟缓/断线；不接受任意HTTP目标路径。
        options.timeoutMs=unsigned(std::stoul(argv[argc-1])); SupervisorHttpClient timed(options); SupervisorHttpResponse response;
        if (method=="identity" && argc==7) response=timed.Identity();
        else if (method=="status" && argc==7) response=timed.Status();
        else if (method=="receivers" && argc==7) response=timed.Receivers();
        else if (method=="operation" && argc==8) response=timed.Operation(argv[6]);
        else if (method=="submit" && argc==8) response=timed.Submit(DecodeSupervisorControlRequest(ReadFile(argv[6],8192)));
        else throw std::invalid_argument("Invalid HTTP fixture method/argument count");
        std::cout<<"{\"code\":"<<response.code<<",\"body\":"<<EncodeJsonString(response.body)<<"}\n"; return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 2; }
}
