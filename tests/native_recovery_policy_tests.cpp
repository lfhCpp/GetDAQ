#include "native_recovery_policy_tests_p.h"
#include "getdaq_test_support_p.h"
#include "plan_transition_fixture.h"

#include <QtTest>
#include <string>
#include <vector>

#include "nativealarmrecoverypolicy.h"
#include "nativecatalogrecoverypolicy.h"
#include "nativecobomaintenancepolicy.h"
#include "nativerecoveryevidencepolicy.h"
#include "nativereceiverfinalizationpolicy.h"
#include "nativerecoverysnapshotpolicy.h"
#include "nativeplanretirementprojection.h"

namespace GetDAQTestInternal {

void RunNativeReceiverFinalizationPolicyRequiresCompleteBoundEvidence()
{
    RunConfig config;
    NodeConfig active;active.mutant=false;config.nodes.push_back(active);
    NodeConfig mutant;mutant.mutant=true;config.nodes.push_back(mutant);
    GetDAQ::EccControlSystemStatus status;status.controller.state=0;
    GetDAQ::EccControlDataNodeStatus node;node.state="stopped";
    GetDAQ::EccControlReceiverStatus receiver;receiver.id="C0";receiver.reaped=true;
    receiver.sealVerified=true;receiver.exitCode=0;receiver.files=4;
    node.receivers.push_back(receiver);
    QVERIFY(!GetDAQ::NativeReceiverFinalizationError(status,config,false).isEmpty());
    QVERIFY(GetDAQ::HasNormalNativeRetirementEvidence(status)==false);

    status.nodes.push_back(node);
    QVERIFY(GetDAQ::NativeReceiverFinalizationError(status,config,false).isEmpty());
    QVERIFY(!GetDAQ::NativeReceiverFinalizationError(status,config,true).isEmpty());
    status.nodes[0].catalogVerified=true;status.nodes[0].catalog.linkedFiles=4;
    QVERIFY(GetDAQ::NativeReceiverFinalizationError(status,config,true).isEmpty());
    QVERIFY(GetDAQ::HasNormalNativeRetirementEvidence(status)==false);
    status.controller.state=1;
    QVERIFY(GetDAQ::HasNormalNativeRetirementEvidence(status));

    status.nodes[0].needsReconciliation=true;
    QVERIFY(!GetDAQ::NativeReceiverFinalizationError(status,config,false).isEmpty());
    QVERIFY(!GetDAQ::HasNormalNativeRetirementEvidence(status));
}

void RunNativeRecoverySnapshotPolicyRequiresExactDeploymentIdentity()
{
    RunConfig saved=GetDAQTestSupport::Config("/tmp/recovery-policy");
    saved.nativeEcc=true;
    RunConfig current=saved;
    QVERIFY(GetDAQ::MatchesNativeRecoverySnapshot(saved,current));

    current.runNumber++;
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
    current=saved;current.nativeClient.clientEpoch+="-changed";
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
    current=saved;current.nativeClient.inputBindingSha256+="-changed";
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
    current=saved;current.nodes[0].nativeSenderId+="-changed";
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
    current=saved;current.nodes[0].nativeAgentId+="-changed";
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
    current=saved;current.nativeEcc=false;
    QVERIFY(!GetDAQ::MatchesNativeRecoverySnapshot(saved,current));
}

void RunNativeRecoveryEvidencePolicyBindsInstanceAndReceiverSessions()
{
    GetDAQ::EccControlIdentity identity;identity.instanceId="instance-1";
    GetDAQ::EccControlSystemStatus ready,current;
    ready.snapshotSha256=current.snapshotSha256=std::string(64,'a');
    current.instanceId="instance-1";current.controller.state=4;
    GetDAQ::EccControlDataNodeStatus readyNode,currentNode;
    GetDAQ::EccControlReceiverStatus readyReceiver,currentReceiver;
    readyReceiver.id=currentReceiver.id="C0";
    readyReceiver.sessionId=currentReceiver.sessionId="session-1";
    readyReceiver.pid=currentReceiver.pid=4242;
    readyReceiver.outputDirectory=currentReceiver.outputDirectory="/data/run";
    readyNode.receivers.push_back(readyReceiver);currentNode.receivers.push_back(currentReceiver);
    ready.nodes.push_back(readyNode);current.nodes.push_back(currentNode);
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).isEmpty());

    current.nodes[0].receivers[0].sessionId="other-session";
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("session/PID/dataset"));
    current.nodes[0].receivers[0]=currentReceiver;current.nodes[0].receivers[0].pid++;
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("session/PID/dataset"));
    current.nodes[0].receivers[0]=currentReceiver;current.nodes[0].receivers[0].outputDirectory="/data/other";
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("session/PID/dataset"));
    current.nodes[0].receivers[0]=currentReceiver;current.instanceId="other-instance";
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("instance/snapshot"));
    current.instanceId="instance-1";current.controller.state=7;
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("coverage/state"));
    current.controller.state=4;current.nodes[0].receivers.clear();
    QVERIFY(GetDAQ::NativeRecoveryEvidenceError(identity,ready,current).contains("session/PID/dataset"));
}

void RunNativeCatalogRecoveryPolicyRequiresExactLivePreparedReplacement()
{
    GetDAQ::EccControlSystemStatus status;status.controller.state=3;
    const std::string saved="failed-catalog-operation",requested=saved,operation="resume-catalog-operation";
    QVERIFY(GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,"other-failure",operation,true,true,true,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,saved,true,true,true,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,false,true,true,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,false,true,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,false,true,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,false,true));
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,false));
    status.controller.transition=true;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.controller.transition=false;status.controller.error=1;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.controller.error=0;status.controller.outcomeUnknown=true;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.controller.outcomeUnknown=false;status.controller.needsReconciliation=true;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.controller.needsReconciliation=false;status.controlNeedsReconciliation=true;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.controlNeedsReconciliation=false;status.operationActive=true;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
    status.operationActive=false;status.controller.state=4;
    QVERIFY(!GetDAQ::CanResumeNativeCatalogReplacement(status,saved,requested,operation,true,true,true,true,true));
}

void RunNativeAlarmRecoveryPolicyBindsOriginalOperationAndOutcome()
{
    GetDAQ::EccControlSystemStatus system;system.instanceId="instance-1";system.snapshotSha256=std::string(64,'a');
    system.controller.operationId="controller-operation";
    GetDAQ::EccControlAlarmStatus alarm;alarm.enabled=true;alarm.latched=true;alarm.phase="complete";
    alarm.operationId="alarm-operation";alarm.controllerOperationId="controller-operation";
    alarm.instanceId="instance-1";alarm.snapshotSha256=system.snapshotSha256;alarm.controllerOutcomeUnknown=true;
    const auto unknown=GetDAQ::NativeAlarmRecoveryMode::AcknowledgeUnknownStop;
    const auto stopped=GetDAQ::NativeAlarmRecoveryMode::ReleaseKnownStoppedRun;
    QVERIFY(GetDAQ::MatchesNativeAlarmRecovery(alarm,system,unknown));
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));

    alarm.controllerOutcomeUnknown=false;
    alarm.operationId="controller-operation";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,unknown));
    QVERIFY(GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.phase="held-no-run";
    QVERIFY(GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,unknown));
    alarm.phase="failed";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.phase="complete";alarm.operationId.clear();alarm.controllerOperationId="controller-operation";
    QVERIFY(GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.controllerOperationId="other-controller-operation";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.operationId="other-alarm-operation";alarm.controllerOperationId="controller-operation";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.operationId="alarm-operation";alarm.controllerOutcomeUnknown=true;
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    QVERIFY(GetDAQ::MatchesNativeAlarmRecovery(alarm,system,unknown));
    alarm.controllerOperationId="other-controller-operation";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,unknown));
    alarm.controllerOperationId="controller-operation";
    alarm.controllerOutcomeUnknown=false;alarm.instanceId="other-instance";
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.instanceId="instance-1";alarm.snapshotSha256=std::string(64,'b');
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
    alarm.snapshotSha256=system.snapshotSha256;alarm.latched=false;
    QVERIFY(!GetDAQ::MatchesNativeAlarmRecovery(alarm,system,stopped));
}

void RunNativeCoBoMaintenancePolicySharesPreparedIdentityGate()
{
    GetDAQ::NativeCoBoMaintenanceContext context;
    context.available=true;context.controllerState=3;context.instanceId="instance-1";
    context.snapshotSha256=std::string(64,'a');
    const auto allowed=[&]{return GetDAQ::CanSubmitNativeCoBoMaintenance(context,"instance-1",std::string(64,'a'));};
    QVERIFY(allowed());
    context.available=false;QVERIFY(!allowed());context.available=true;
    context.activeRun=true;QVERIFY(!allowed());context.activeRun=false;
    context.controllerState=4;QVERIFY(!allowed());context.controllerState=3;
    context.controllerError=true;QVERIFY(!allowed());context.controllerError=false;
    QVERIFY(!GetDAQ::CanSubmitNativeCoBoMaintenance(context,"other-instance",std::string(64,'a')));
    QVERIFY(!GetDAQ::CanSubmitNativeCoBoMaintenance(context,"instance-1",std::string(64,'b')));
    context.instanceId.clear();QVERIFY(!allowed());context.instanceId="instance-1";
    context.snapshotSha256.clear();QVERIFY(!allowed());
    context.instanceId.clear();
    QVERIFY(GetDAQ::CanSubmitNativeCoBoMaintenance(context,"",std::string()));

    const std::vector<std::string> commands={"reprepare-cobo","configure-cobo","set-cobo-led",
        "trigger-asad-pulser","blink-cobo-leds","flip-cobo-led","pulse-cobo-led"};
    for(const auto &command:commands)QVERIFY(GetDAQ::IsNativeCoBoMaintenanceCommand(command));
    for(const std::string &command:{std::string("start"),std::string("catalog"),std::string("retire-data-plans")})
        QVERIFY(!GetDAQ::IsNativeCoBoMaintenanceCommand(command));
}

void RunNativePlanRetirementProjectionPreservesOriginalInstanceGates()
{
    GetDAQ::NativePlanRetirementProjectionInput input;
    input.request=PlanTransitionFixture::Request("original-retirement");
    input.system.instanceId=input.request.instanceId;
    input.availableForControl=true;input.normalRetirementEvidence=true;input.connected=true;
    input.haveStatus=true;input.retirementComplete=true;input.clientAvailable=true;
    input.storeAvailable=true;input.storeCanComplete=true;input.clientCanComplete=true;
    input.clientBusyForObservation=false;
    input.hasRetirementContext=true;input.hasCompletion=true;
    input.originalStatus="original-status";input.completionRequest="completion-request";
    input.completionPhase="failed";input.phase="ready";
    auto view=GetDAQ::ProjectNativePlanRetirement(input);
    QVERIFY(view.canSubmit);QVERIFY(view.canInspect);QVERIFY(view.retirementHeld);
    QVERIFY(view.canComplete);QVERIFY(view.canObserveCompletion);
    QCOMPARE(view.originalRequest,GetDAQ::EncodeEccControlRequest(input.request));
    QCOMPARE(view.originalStatus,std::string("original-status"));
    QCOMPARE(view.completionRequest,std::string("completion-request"));
    QCOMPARE(view.completionPhase,std::string("failed"));QCOMPARE(view.reason,std::string("ready"));

    input.system.instanceId="other-instance";
    view=GetDAQ::ProjectNativePlanRetirement(input);
    QVERIFY(view.canSubmit);QVERIFY(!view.canInspect);QVERIFY(!view.canComplete);
    QVERIFY(!view.canObserveCompletion);
    QCOMPARE(view.reason,std::string("Completion requires the original live ECC instance"));
    input.system.instanceId="instance-1";input.clientBusy=true;input.clientBusyForObservation=true;
    view=GetDAQ::ProjectNativePlanRetirement(input);
    QVERIFY(!view.canInspect);QVERIFY(!view.canObserveCompletion);QVERIFY(!view.canComplete);
    input.system.instanceId=input.request.instanceId;
    input.clientBusy=false;input.clientBusyForObservation=false;input.hasRetirementContext=false;
    view=GetDAQ::ProjectNativePlanRetirement(input);
    QCOMPARE(view.reason,std::string("Saved pre-retirement status is missing; observation only"));
    input.request.command="complete-data-handoff";
    view=GetDAQ::ProjectNativePlanRetirement(input);
    QVERIFY(!view.retirementHeld);QVERIFY(view.originalRequest.empty());
}

} // namespace GetDAQTestInternal
