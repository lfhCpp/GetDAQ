#include "nativeruncontrol.h"
#include "nativecobomaintenancepolicy.h"
#include <QUuid>

bool NativeRunControl::CanSubmitCoBoMaintenance(const QString &instance,const QString &snapshot) const {
    GetDAQ::NativeCoBoMaintenanceContext context;
    context.available=Available();context.activeRun=fActive;
    context.controllerState=fSystem.controller.state;context.controllerError=fSystem.controller.error!=0;
    context.instanceId=fSystem.instanceId;context.snapshotSha256=fSystem.snapshotSha256;
    return GetDAQ::CanSubmitNativeCoBoMaintenance(context,instance.toStdString(),snapshot.toStdString());
}

void NativeRunControl::InspectCoBoMaintenanceTargets() {
    if(!fConnected || fShuttingDown || !fClient || !fClient->QueryMaintenanceTargets()) {
        emit CoBoMaintenanceTargetsSignal({},"Connect to the bound native ECC and wait for current metadata");return;
    }
    Publish();
}
void NativeRunControl::ReprepareCoBo(const QString &node,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"Maintenance requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    fCommand="reprepare-cobo";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-maintenance";
    fBusy=true;fPhase="Preparing selected CoBo only";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoMaintenance(node,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"Maintenance submission was not confirmed; preserve and inspect any saved intent");
        Fail("CoBo maintenance was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::ConfigureCoBo(const QString &node,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"Configure requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    fCommand="configure-cobo";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-configure";
    fBusy=true;fPhase="Configuring selected CoBo only; system remains Prepared";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoConfigure(node,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"Configure submission was not confirmed; preserve and inspect any saved intent");
        Fail("Selected CoBo Configure was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::SetCoBoLed(const QString &node,unsigned int asad,unsigned int type,unsigned int state,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"LED control requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    fCommand="set-cobo-led";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-led";
    fBusy=true;fPhase="Submitting one selected CoBo LED state";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoLed(node,asad,type,state,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"LED command submission was not confirmed; preserve and inspect any saved intent");Fail("CoBo LED command was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::BlinkCoBoLeds(const QString &node,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"LED blink requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    fCommand="blink-cobo-leds";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-led-blink";
    fBusy=true;fPhase="Submitting one selected CoBo sequential LED blink";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoLedBlink(node,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"LED blink submission was not confirmed; preserve and inspect any saved intent");Fail("CoBo LED blink was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::FlipCoBoLed(const QString &node,unsigned int ledId,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"LED flip requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    fCommand="flip-cobo-led";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-led-flip";
    fBusy=true;fPhase="Submitting one non-idempotent CoBo LED flip";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoLedFlip(node,ledId,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"LED flip submission was not confirmed; preserve and inspect any saved intent");Fail("CoBo LED flip was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::PulseCoBoLed(const QString &node,unsigned int ledId,unsigned int periodMs,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"LED pulse requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    if(ledId>16) {emit CoBoMaintenanceResultSignal({},"LED pulse selection must be 0..15 or 16 for all LEDs");return;}
    fCommand="pulse-cobo-led";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-cobo-led-pulse";
    fBusy=true;fPhase="Submitting one selected CoBo LED pulse command";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitCoBoLedPulse(node,ledId,periodMs,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"LED pulse submission was not confirmed; preserve and inspect any saved intent");Fail("CoBo LED pulse was not confirmed submitted; inspect the original intent");
    }
}
void NativeRunControl::TriggerAsAdPulser(const QString &node,int voltage,const QString &instance,const QString &snapshot) {
    if(!CanSubmitCoBoMaintenance(instance,snapshot)) {
        emit CoBoMaintenanceResultSignal({},"Pulser trigger requires the reviewed original instance in healthy Prepared with no active Run");return;
    }
    if(voltage < -1000 || voltage > 1000) {emit CoBoMaintenanceResultSignal({},"Pulser voltage must be within -1000..1000 mV");return;}
    fCommand="trigger-asad-pulser";fTarget=-1;fOperationId="gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-pulser-trigger";
    fBusy=true;fPhase="Submitting one selected CoBo pulser trigger";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->SubmitAsAdPulserTrigger(node,voltage,fOperationId)) {
        emit CoBoMaintenanceResultSignal({},"Pulser trigger submission was not confirmed; preserve and inspect any saved intent");Fail("Pulser trigger was not confirmed submitted; inspect the original intent");
    }
}

bool NativeRunControl::HandleCoBoMaintenanceResult(const QString &phase,const QByteArray &body,
    const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result) {
    if(GetDAQ::IsNativeCoBoMaintenanceCommand(fCommand.toStdString())) {
        if(phase=="complete" && (result.state!=3 || result.transition || result.error || result.outcomeUnknown || result.needsReconciliation)) {
            emit CoBoMaintenanceResultSignal({},"Maintenance did not retain known Prepared");Fail("CoBo maintenance result requires inspection");return true;
        }
        emit CoBoMaintenanceResultSignal(body,phase=="complete" ? QString() : QString::fromStdString(operation.message));
        const bool led=fCommand=="set-cobo-led",blink=fCommand=="blink-cobo-leds",flip=fCommand=="flip-cobo-led",pulse=fCommand=="pulse-cobo-led",pulser=fCommand=="trigger-asad-pulser";
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase=phase=="complete" ? (led ? "LED command acknowledged; physical lamp state unverified" : blink ? "LED blink acknowledged; physical lamp state unverified" : flip ? "LED flip acknowledged; physical lamp state unverified" : pulse ? "LED pulse acknowledged; physical lamp state unverified" : pulser ? "Pulser command acknowledged; physical pulse unverified" : operation.request.command=="configure-cobo" ? "Selected CoBo Configure complete; system remains Prepared" : "Selected CoBo prepared; explicit Configure required") : "CoBo maintenance failed; original evidence retained";
        QueryStatusAfterOperation();Publish();return true;
    }
    return false;
}
