#include "eccsystemclient.h"
#include "eccsystemclient_p.h"
#include <stdexcept>

using namespace EccSystemClientInternal;

bool EccSystemClient::CanSubmit(const QString &command) const
{
    if(command=="complete-data-handoff") {
        const auto &status=fLatestStatus;
        return fPlanRetired && !fFault && !fCredentialFault && !fRemoteBusy && !Busy() && !Pending() &&
            fStatusAvailable && fStatusClock.isValid() && fStatusClock.elapsed()<=fOptions.statusMaxAgeMs &&
            fIdentity.instanceId==fRetirementRequest.instanceId && status.instanceId==fRetirementRequest.instanceId &&
            status.snapshotSha256==fRetirementRequest.snapshotSha256 && status.clientEpoch==fRetirementRequest.clientEpoch &&
            (status.controller.state==1 || status.controller.state==2) && !status.controller.outcomeUnknown &&
            !status.controller.error && !status.controller.transition;
    }
    if(fPlanRetired)return false;
    if((command=="reprepare-cobo" || command=="configure-cobo" || command=="set-cobo-led" || command=="trigger-asad-pulser" || command=="blink-cobo-leds" || command=="flip-cobo-led" || command=="pulse-cobo-led") && fControllerState!=3)return false;
    const bool cleanup=command=="abort-failed-receivers" || command=="retire-failed-receivers" || command=="retire-failed-sources" || command=="unprepare-acquisition-after-failure" || command=="unprepare-after-failure" || command=="breakup-after-failure";
    const bool catalogResume=command=="resume-catalog-replacement";
    const bool recovery=(command=="reconcile-description" || command=="release-stopped-alarm" || catalogResume) || command=="reconcile-preparation" || command=="reconcile-preparation-with-alarm" || ((command=="reconcile-acquisition" || command=="reconcile-acquisition-with-sources") || command=="reconcile-acquisition-with-alarm");
    const bool productionStop = command == "stop-production-after-failure" || cleanup || recovery;
    const bool retainedIdentity = productionStop && !fIdentity.instanceId.empty() && !fFault;
    const bool fresh = fStatusAvailable && fStatusClock.isValid() && fStatusClock.elapsed() <= fOptions.statusMaxAgeMs;
    return (fMatched || retainedIdentity) && !fFault && (!fControlFault || productionStop) && (productionStop || (fresh && (!fOptions.alarmStatusEnabled || fAlarmAvailable))) &&
        (catalogResume
            ? (fresh && !fControlFault && fLatestStatus.instanceId==fIdentity.instanceId &&
               fLatestStatus.clientEpoch==Utf8(fOptions.clientEpoch) && !fLatestStatus.operationActive &&
               fLatestStatus.controller.state==3 && !fLatestStatus.controller.transition &&
               !fLatestStatus.controller.error && !fLatestStatus.controller.outcomeUnknown &&
               !fLatestStatus.controller.needsReconciliation && !fLatestStatus.controlNeedsReconciliation)
            : (!(cleanup || recovery) || (fresh && fControlFault))) && !fRemoteBusy && !Busy() && !Pending();
}

bool EccSystemClient::Submit(const QString &command, const QString &operationId,const QString &catalogExperiment,unsigned int catalogRun,const QString &failedOperationId,const QString &alarmOperationId,bool catalogReplaceExisting)
{
    if(command=="complete-data-handoff")return false; // Only the retained, typed retirement-completion entry may submit this command.
    if (!CanSubmit(command)) return false;
    GetDAQ::EccControlRequest request; request.operationId = Utf8(operationId); request.command = Utf8(command);
    request.instanceId = fIdentity.instanceId; request.snapshotSha256 = fIdentity.snapshotSha256;
    request.clientId = Utf8(fOptions.clientId); request.clientEpoch = Utf8(fOptions.clientEpoch);
    request.catalogExperiment=Utf8(catalogExperiment); request.catalogRunNumber=catalogRun;request.failedOperationId=Utf8(failedOperationId);request.alarmOperationId=Utf8(alarmOperationId);
    request.catalogReplaceExisting=catalogReplaceExisting;
    return SubmitRequest(request);
}

bool EccSystemClient::SubmitPlanRetirement(const QByteArray &successorBytes,const QByteArray &targetsBytes,const QString &operationId)
{
    if(!CanSubmit("retire-data-plans"))return false;
    try {
        const auto review=GetDAQ::ReviewEccPlanTransition(successorBytes.toStdString(),targetsBytes.toStdString());
        GetDAQ::EccControlRequest request;request.command="retire-data-plans";request.operationId=Utf8(operationId);request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;
        request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        request.successorPlanFile=review.successor.planFile;request.successorPlanSha256=review.successorSha256;request.successorScopeSha256=review.successor.scopeSha256;
        request.successorEpoch=review.successor.epoch;request.successorDataBindingSha256=review.successor.dataBindingSha256;request.successorReceiverPlans=review.targets;
        return SubmitRequest(request);
    } catch(const std::exception &e) {emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

void EccSystemClient::RetainPlanRetirement(const GetDAQ::EccControlRequest &request)
{
    GetDAQ::ValidateEccControlRequest(request);
    if(request.command!="retire-data-plans" || request.clientId!=Utf8(fOptions.clientId) || request.clientEpoch!=Utf8(fOptions.clientEpoch) || request.snapshotSha256!=Utf8(fOptions.snapshotSha256))
        throw std::invalid_argument("Retirement hold differs from this original client policy");
    if(fPlanRetired && GetDAQ::EncodeEccControlRequest(fRetirementRequest)!=GetDAQ::EncodeEccControlRequest(request))
        throw std::invalid_argument("Another retirement request is already retained");
    fRetirementRequest=request;fPlanRetired=true;LatchControlFault();
}

bool EccSystemClient::SubmitPlanRetirementCompletion(const GetDAQ::EccControlRequest &original,const QString &operationId)
{
    if(!CanSubmit("complete-data-handoff"))return false;
    try {
        if(GetDAQ::EncodeEccControlRequest(original)!=GetDAQ::EncodeEccControlRequest(fRetirementRequest))
            throw std::invalid_argument("Completion must reference the retained original retirement");
        GetDAQ::EccControlRequest request;request.command="complete-data-handoff";request.operationId=Utf8(operationId);
        request.failedOperationId=original.operationId;request.instanceId=original.instanceId;request.snapshotSha256=original.snapshotSha256;
        request.clientId=original.clientId;request.clientEpoch=original.clientEpoch;
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::QueryControlHandoff(const GetDAQ::EccControlRequest &request,const GetDAQ::EccControlSystemStatus &original)
{
    if(fHandoffBusy || fCredentialFault || fIdentity.instanceId.empty())return false;
    try {
        if(request.instanceId!=fIdentity.instanceId || request.snapshotSha256!=fIdentity.snapshotSha256 || request.clientEpoch!=Utf8(fOptions.clientEpoch) || request.clientId!=Utf8(fOptions.clientId))
            throw std::invalid_argument("Receipt query requires the original live ECC instance and client");
        const auto &current=fLatestStatus;
        if(!fStatusAvailable || !fStatusClock.isValid() || fStatusClock.elapsed()>fOptions.statusMaxAgeMs || current.operationActive ||
            current.instanceId!=request.instanceId || current.clientEpoch!=request.clientEpoch || !current.controlNeedsReconciliation || !current.dataNeedsReconciliation ||
            (current.controller.state!=1 && current.controller.state!=2) || current.controller.outcomeUnknown || current.controller.error || current.controller.transition)
            throw std::invalid_argument("Refresh the original live stopped/fenced ECC status; a saved normal cycle cannot override a current hardware fault");
        const auto headers=Headers();fHandoffBusy=true;
        fHttp->Request(Url("/v1/control-handoff"),{},{},fOptions.timeoutMs,[this,request,original](const QByteArray &body,const QString &failure) {
            fHandoffBusy=false;QString error=failure;
            if(error.isEmpty())try {GetDAQ::ValidateEccPlanRetirementReceipt(body.toStdString(),request,original);}catch(const std::exception &e){error=Failure(e);}
            emit ControlHandoffSignal(error.isEmpty() ? body : QByteArray(),error);
        },headers);return true;
    } catch(const std::exception &e) {fHandoffBusy=false;emit ControlHandoffSignal({},Failure(e));return false;}
}
