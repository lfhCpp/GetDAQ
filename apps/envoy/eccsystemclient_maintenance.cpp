#include "eccsystemclient.h"
#include "eccsystemclient_p.h"
#include <stdexcept>

using namespace EccSystemClientInternal;

bool EccSystemClient::QueryMaintenanceTargets()
{
    if(fMaintenanceBusy || fCredentialFault || fIdentity.instanceId.empty())return false;
    QMap<QByteArray,QByteArray> headers;
    try {headers=Headers();}catch(const std::exception &e){emit MaintenanceTargetsSignal({},Failure(e));return false;}
    fMaintenanceBusy=true;
    fHttp->Request(Url("/v1/cobo-maintenance-targets"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fMaintenanceBusy=false;QString error=failure;
        if(error.isEmpty())try {
            const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(body.toStdString());
            if(targets.instanceId!=fIdentity.instanceId || targets.snapshotSha256!=fIdentity.snapshotSha256 ||
               targets.configurationSha256!=fIdentity.configurationSha256 || targets.artifactSha256!=fIdentity.artifactSha256)
                throw std::runtime_error("CoBo maintenance target identity/configuration differs");
            auto fixed=targets;
            for(auto &target:fixed.cobos) {target.lifecycleState="unknown";target.operationId.clear();target.operation.clear();target.operationPhase="idle";}
            const auto canonical=QByteArray::fromStdString(GetDAQ::EncodeEccControlMaintenanceTargets(fixed));
            if(!fMaintenanceCanonical.isEmpty() && fMaintenanceCanonical!=canonical)throw std::runtime_error("Fixed CoBo maintenance targets changed");
            fMaintenanceCanonical=canonical;
        }catch(const std::exception &e){fFault=true;error=Failure(e);}
        emit MaintenanceTargetsSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}

bool EccSystemClient::SubmitCoBoMaintenance(const QString &coboId,const QString &operationId)
{
    if(!CanSubmit("reprepare-cobo") || fMaintenanceCanonical.isEmpty())return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="reprepare-cobo";request.coboId=Utf8(coboId);request.operationId=Utf8(operationId);
        request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitCoBoConfigure(const QString &coboId,const QString &operationId)
{
    if(!CanSubmit("configure-cobo") || fMaintenanceCanonical.isEmpty())return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="configure-cobo";request.coboId=Utf8(coboId);request.operationId=Utf8(operationId);
        request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitCoBoLed(const QString &coboId,unsigned int asadId,unsigned int ledType,unsigned int ledState,const QString &operationId)
{
    if(!CanSubmit("set-cobo-led") || fMaintenanceCanonical.isEmpty())return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="set-cobo-led";request.coboId=Utf8(coboId);request.ledAsadId=asadId;request.ledType=ledType;request.ledState=ledState;
        request.operationId=Utf8(operationId);request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;
        request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitCoBoLedBlink(const QString &coboId,const QString &operationId)
{
    if(!CanSubmit("blink-cobo-leds") || fMaintenanceCanonical.isEmpty())return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="blink-cobo-leds";request.coboId=Utf8(coboId);request.operationId=Utf8(operationId);
        request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitCoBoLedFlip(const QString &coboId,unsigned int ledId,const QString &operationId)
{
    if(!CanSubmit("flip-cobo-led") || fMaintenanceCanonical.isEmpty() || ledId>16)return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="flip-cobo-led";request.coboId=Utf8(coboId);request.ledFlipId=ledId;request.operationId=Utf8(operationId);
        request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitCoBoLedPulse(const QString &coboId,unsigned int ledId,unsigned int periodMs,const QString &operationId)
{
    if(!CanSubmit("pulse-cobo-led") || fMaintenanceCanonical.isEmpty() || ledId>16)return false;
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="pulse-cobo-led";request.coboId=Utf8(coboId);request.ledPulseId=ledId;request.ledPulsePeriodMs=periodMs;request.operationId=Utf8(operationId);
        request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}

bool EccSystemClient::SubmitAsAdPulserTrigger(const QString &coboId,int voltageMv,const QString &operationId)
{
    if(!CanSubmit("trigger-asad-pulser") || fMaintenanceCanonical.isEmpty())return false;
    try {
        if(voltageMv < -1000 || voltageMv > 1000)throw std::invalid_argument("Pulser voltage must be within -1000..1000 mV");
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fMaintenanceCanonical.toStdString());
        bool selected=false;for(const auto &target:targets.cobos)if(target.id==Utf8(coboId))selected=true;
        if(!selected)throw std::runtime_error("Selected CoBo is not in the fixed active maintenance set");
        GetDAQ::EccControlRequest request;request.command="trigger-asad-pulser";request.coboId=Utf8(coboId);request.pulserVoltageMv=voltageMv;
        request.operationId=Utf8(operationId);request.instanceId=fIdentity.instanceId;request.snapshotSha256=fIdentity.snapshotSha256;
        request.clientId=Utf8(fOptions.clientId);request.clientEpoch=Utf8(fOptions.clientEpoch);
        return SubmitRequest(request);
    }catch(const std::exception &e){emit OperationSignal(operationId,"not-sent",{},Failure(e));return false;}
}
