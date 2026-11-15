#include "eccsystemclient.h"
#include "eccsystemclient_p.h"
#include <stdexcept>

using namespace EccSystemClientInternal;
bool EccSystemClient::QueryConfiguration()
{
    if (fConfigurationBusy || fCredentialFault || fIdentity.instanceId.empty() || fFault) return false;
    QMap<QByteArray,QByteArray> headers;
    try {headers=Headers();}catch (const std::exception &e) {emit ConfigurationSignal({},Failure(e));return false;}
    fConfigurationBusy=true;
    fHttp->Request(Url("/v1/configuration"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fConfigurationBusy=false;QString error=failure;
        if (error.isEmpty()) try {
            const auto c=GetDAQ::DecodeEccControlConfiguration(body.toStdString());
            if (c.instanceId!=fIdentity.instanceId || c.snapshotSha256!=fIdentity.snapshotSha256 || c.inputBindingSha256!=fIdentity.inputBindingSha256 ||
                c.configurationSha256!=fIdentity.configurationSha256 || c.artifactSha256!=fIdentity.artifactSha256)
                throw std::runtime_error("ECC configuration cache differs from fixed identity");
            const auto canonical=QByteArray::fromStdString(GetDAQ::EncodeEccControlConfiguration(c));
            if (!fConfigurationCanonical.isEmpty() && fConfigurationCanonical!=canonical) throw std::runtime_error("ECC startup selection changed within the same bound instance");
            fConfigurationCanonical=canonical;
        }catch (const std::exception &e) {fFault=true;fMatched=false;error=Failure(e);}
        emit ConfigurationSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryConfigureReadbacks()
{
    if (fReadbacksBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray,QByteArray> headers;
    try {headers=Headers();}catch(const std::exception &e){emit ConfigureReadbacksSignal({},Failure(e));return false;}
    fReadbacksBusy=true;
    fHttp->Request(Url("/v1/configure-readbacks"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fReadbacksBusy=false;QString error=failure;
        if(error.isEmpty())try {
            const auto evidence=GetDAQ::DecodeEccControlConfigureReadbacks(body.toStdString());
            if(evidence.instanceId!=fIdentity.instanceId || evidence.snapshotSha256!=fIdentity.snapshotSha256 ||
                evidence.configurationSha256!=fIdentity.configurationSha256 || evidence.artifactSha256!=fIdentity.artifactSha256)
                throw std::runtime_error("Configure readback cache differs from the fixed ECC identity; no takeover");
            if(evidence.controller.outcomeUnknown || evidence.controller.needsReconciliation) LatchControlFault();
        }catch(const std::exception &e){fFault=true;fMatched=false;error=Failure(e);}
        emit ConfigureReadbacksSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryOwnership()
{
    if (fOwnershipBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray,QByteArray> headers;
    try { headers=Headers(); } catch (const std::exception &e) { emit OwnershipSignal({},Failure(e)); return false; }
    fOwnershipBusy=true;
    fHttp->Request(Url("/v1/ownership"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fOwnershipBusy=false; QString error=failure;
        if (error.isEmpty()) try {
            const auto ownership=GetDAQ::DecodeEccControlOwnership(body.toStdString());
            if (ownership.instanceId!=fIdentity.instanceId || ownership.snapshotSha256!=fIdentity.snapshotSha256)
                throw std::runtime_error("ECC ownership cache differs from fixed identity; no takeover");
            QByteArray binding(1,ownership.enabled ? '1' : '0');binding+='\0';
            binding+=QByteArray::fromStdString(ownership.directory);binding+='\0';
            for (const auto &resource:ownership.resources) { binding+=QByteArray::fromStdString(resource);binding+='\0'; }
            if (!fOwnershipBinding.isEmpty() && fOwnershipBinding!=binding)
                throw std::runtime_error("ECC reservation resources changed within the same bound instance");
            fOwnershipBinding=binding;
            if (ownership.faultLatched) LatchControlFault();
        } catch (const std::exception &e) { fFault=true;fMatched=false;error=Failure(e); }
        emit OwnershipSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryReceivers()
{
    if (!fOptions.receiverStatusEnabled || fReceiverBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray,QByteArray> headers;
    try { headers=Headers(); } catch (const std::exception &e) { emit ReceiverSignal({},Failure(e)); return false; }
    fReceiverBusy=true;
    fHttp->Request(Url("/v1/receivers"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fReceiverBusy=false; QString error=failure;
        if (error.isEmpty()) try {
            const auto receivers=GetDAQ::DecodeEccControlReceiverObservation(body.toStdString());
            if (receivers.instanceId!=fIdentity.instanceId || receivers.snapshotSha256!=fIdentity.snapshotSha256 || receivers.dataBindingSha256!=fIdentity.dataBindingSha256)
                throw std::runtime_error("ECC receiver observation instance/snapshot/data binding differs");
        } catch (const std::exception &e) { error=Failure(e); }
        emit ReceiverSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryReceiverEvents(const QString &senderId,bool fileBytes)
{
    if (!fOptions.receiverStatusEnabled || fReceiverEventsBusy || fReceiverBusy || fSourceBusy || fReceiverFailureBusy ||
        fScanIdentityBusy || fCredentialFault || fIdentity.instanceId.empty() || senderId.isEmpty()) return false;
    const auto report=[this,fileBytes,senderId](const QByteArray &body,const QString &error){if(fileBytes)emit ReceiverFileBytesSignal(senderId,body,error);else emit ReceiverEventsSignal(senderId,body,error);};
    QMap<QByteArray,QByteArray> headers;
    const auto sourceId=senderId.toUtf8();
    if (sourceId.size()>128) { report({},tr("Receiver source ID exceeds its limit")); return false; }
    try { headers=Headers(); } catch (const std::exception &e) { report({},Failure(e)); return false; }
    fReceiverEventsBusy=true;
    const auto encoded=QString::fromLatin1(sourceId.toHex());
    fHttp->Request(Url(QString(fileBytes ? "/v1/receiver-file-bytes/" : "/v1/receiver-events/")+encoded),{},{},fOptions.timeoutMs,[this,senderId,fileBytes,report](const QByteArray &body,const QString &failure) {
        fReceiverEventsBusy=false; QString error=failure;
        if (error.isEmpty()) try {
            const auto events=GetDAQ::DecodeEccControlReceiverEvents(body.toStdString(),fileBytes);
            if (events.instanceId!=fIdentity.instanceId || events.snapshotSha256!=fIdentity.snapshotSha256 ||
                events.dataBindingSha256!=fIdentity.dataBindingSha256 || QString::fromUtf8(events.sourceId.data(),int(events.sourceId.size()))!=senderId)
                throw std::runtime_error("ECC receiver event instance/snapshot/data binding/source differs");
        } catch (const std::exception &e) { error=Failure(e); }
        report(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryScanRuntimeIdentity(
    const GetDAQ::ScanRuntimePointIdentityRequest &request,
    const GetDAQ::ScanRuntimeIdentityExpectation &expected)
{
    if (fScanIdentityBusy || fCredentialFault || !fMatched || fIdentity.instanceId.empty() ||
        Busy() || Pending() || fRemoteBusy || fReceiverBusy || fSourceBusy || fReceiverFailureBusy)
        return false;
    QByteArray payload;
    QMap<QByteArray,QByteArray> headers;
    try {
        if (expected.executionSnapshotSha256 != request.executionSnapshotSha256 ||
            expected.pointIndex != request.pointIndex ||
            expected.configurationSha256 != request.configurationSha256 ||
            expected.artifactSha256 != request.artifactSha256 ||
            expected.serviceId != Utf8(fOptions.serviceId) ||
            expected.clientId != Utf8(fOptions.clientId) ||
            expected.instanceId != fIdentity.instanceId ||
            expected.snapshotSha256 != Utf8(fOptions.snapshotSha256) ||
            expected.inputBindingSha256 != Utf8(fOptions.inputBindingSha256))
            throw std::runtime_error("Scan runtime identity expectation differs from request or fixed ECC client");
        payload=QByteArray::fromStdString(GetDAQ::EncodeScanRuntimePointIdentityRequest(request));
        headers=Headers();
    } catch (const std::exception &e) {
        emit ScanRuntimeIdentitySignal({},Failure(e));
        return false;
    }
    fScanIdentityBusy=true;
    fHttp->Request(Url("/v1/scan-runtime-identity"),payload,"application/json",
        fOptions.timeoutMs,[this,expected,request](const QByteArray &body,const QString &failure) {
            fScanIdentityBusy=false;
            QString error=failure;
            if(error.isEmpty()) try {
                const auto response=GetDAQ::DecodeScanRuntimePointIdentityResponse(body.toStdString());
                if(response.mode!=request.mode ||
                   response.observedControllerState!=request.expectedControllerState ||
                   GetDAQ::EncodeScanRuntimeIdentityExpectation(response.expectation)!=
                   GetDAQ::EncodeScanRuntimeIdentityExpectation(expected))
                    throw std::runtime_error("ECC scan runtime identity response differs from the complete frozen deployment expectation");
            } catch(const std::exception &e) { fFault=true;fMatched=false;error=Failure(e); }
            emit ScanRuntimeIdentitySignal(error.isEmpty()?body:QByteArray(),error);
        },headers,8*1024*1024);
    return true;
}
bool EccSystemClient::QuerySources()
{
    if(!fOptions.sourceStatusEnabled || fSourceBusy || fCredentialFault || fIdentity.instanceId.empty())return false;
    QMap<QByteArray,QByteArray> headers;
    try {headers=Headers();}catch(const std::exception &e) {emit SourceSignal({},Failure(e));return false;}
    fSourceBusy=true;
    fHttp->Request(Url("/v1/sources"),{},{},fOptions.timeoutMs,[this](const QByteArray &body,const QString &failure) {
        fSourceBusy=false;QString error=failure;
        if(error.isEmpty())try {
            const auto sources=GetDAQ::DecodeEccControlSourceObservation(body.toStdString());
            if(sources.instanceId!=fIdentity.instanceId || sources.snapshotSha256!=fIdentity.snapshotSha256 || sources.configurationSha256!=fIdentity.configurationSha256 || sources.artifactSha256!=fIdentity.artifactSha256)
                throw std::runtime_error("ECC source observation fixed identity differs");
        }catch(const std::exception &e) {error=Failure(e);}
        emit SourceSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
bool EccSystemClient::QueryReceiverFailure()
{
    if (!fOptions.receiverFailureStatusEnabled || fReceiverFailureBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray,QByteArray> headers;
    try { headers=Headers(); } catch (const std::exception &e) { emit ReceiverFailureSignal({},Failure(e)); return false; }
    fReceiverFailureBusy=true;const auto generation=fRecoveryGeneration;
    fHttp->Request(Url("/v1/receiver-failure"),{},{},fOptions.timeoutMs,[this,generation](const QByteArray &body,const QString &failure) {
        fReceiverFailureBusy=false;
        if(generation!=fRecoveryGeneration){emit ReceiverFailureSignal({},"Pre-recovery receiver cache discarded; refresh required");return;}
        QString error=failure;
        if (error.isEmpty()) try {
            const auto policy=GetDAQ::DecodeEccControlReceiverFailureStatus(body.toStdString());
            if (policy.instanceId!=fIdentity.instanceId || policy.snapshotSha256!=fIdentity.snapshotSha256 ||
                (policy.hasEvidence && policy.observation.dataBindingSha256!=fIdentity.dataBindingSha256))
                throw std::runtime_error("ECC receiver failure cache identity/data binding differs; no takeover");
            if (policy.latched) LatchControlFault();
        } catch (const std::exception &e) { error=Failure(e); }
        emit ReceiverFailureSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
