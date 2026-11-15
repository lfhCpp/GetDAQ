#include "nativeruncontrol.h"
#include "nativecobomaintenancepolicy.h"
#include "nativestatusprojection.h"
#include "nativereceiverfinalizationpolicy.h"
#include "nativeplanretirementprojection.h"
#include "runrecorder.h"
#include "scanexecutionsnapshot.h"
#include <QDir>
#include <QFileInfo>
#include <stdexcept>

void NativeRunControl::UpdateRunMetadata(const RunConfig &config) {
    if (!fConnected || fBusy || fActive) return;
    fConfig.runNumber = config.runNumber;
    fConfig.description = config.description;
    fConfig.fields = config.fields;
}
void NativeRunControl::Connect(const RunConfig &config) {
    if (fConnected || fClient) return;
    QString error; if (!config.Validate(&error) || !config.nativeEcc) { emit LogSignal(error); return; }
    try {
        fDeferredProgressState=0;fRecovery.awaitingMetadata=false;fConfig=config; fClock.start(); fStatusRows.clear(); fObservations.Reset();
        fPlanRetirement.Reset();fHaveStatus=false;fStatusBody.clear();fIdentityBody.clear();fIdentityClock.invalidate();fStatusClock.invalidate();fIdentityMatched=false;fScan.recoveryControlHoldOperationId.clear();
        if (!QDir().mkpath(QFileInfo(config.nativeRecordDir).absolutePath())) throw std::runtime_error("Cannot create native control record parent");
        fStore.reset(new EccClientStore(config.nativeRecordDir,config.nativeClient));
        fClient=new EccSystemClient(config.nativeClient,[this](const std::string &bytes,QString *failure) {
            const auto request=GetDAQ::DecodeEccControlRequest(bytes);
            if (fActive) {
                auto operations=fJournal.value("native_operations").toObject(); operations[QString::fromStdString(request.operationId)]=QString::fromStdString(bytes);
                fJournal["native_operations"]=operations;
                if (!Journal("Local intent before network")) { if (failure) *failure="Cannot save native Run intent"; return false; }
            }
            if (!fStore->WriteIntent(bytes,failure,request.command=="retire-data-plans" ? fStatusBody.toStdString() : std::string())) return false;
            if(request.command=="retire-data-plans")fPlanRetirement.request=request;
            fRequest=request; return true;
        },this);
        if(fStore->PlanRetirement(&fPlanRetirement.request,&fPlanRetirement.complete))fClient->RetainPlanRetirement(fPlanRetirement.request);
        connect(fClient,&EccSystemClient::ControlHandoffSignal,this,[this](const QByteArray &body,const QString &failure) {
            if(!fConnected || fShuttingDown)return;
            QString error=failure,path;
            if(error.isEmpty() && !fStore->SavePlanRetirementReceipt(fPlanRetirement.request,fPlanRetirement.receiptStatus,body.toStdString(),&path,&error))
                emit LogSignal(error);
            if(error.isEmpty()) {fPhase="Original plans retired; receipt saved; successor startup remains explicit";emit LogSignal("Retirement receipt saved: "+path);}
            emit PlanTransitionResultSignal(error.isEmpty() ? body : QByteArray(),QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(fPlanRetirement.request)),error);Publish();
        });
        connect(fClient,&EccSystemClient::IdentitySignal,this,[this](bool matched,const QByteArray &body,const QString &failure) {
            if (!failure.isEmpty()) { Fail(failure); return; }
            if (!body.isEmpty()) {fIdentityBody=body;fIdentityClock.restart();fIdentityMatched=matched;}
            if (!matched) emit LogSignal("Native identity is visible; positive control remains gated.");
            fClient->QueryStatus();
            // Read-only fixed-target metadata lets the GUI distinguish a true
            // one-CoBo deployment from a selected board inside a larger system.
            fClient->QueryMaintenanceTargets();
            if (fConfig.nativeClient.configurationStatusEnabled) fClient->QueryConfiguration();
        });
        connect(fClient,&EccSystemClient::StatusSignal,this,&NativeRunControl::Status);
        connect(fClient,&EccSystemClient::AlarmSignal,this,&NativeRunControl::Alarms);
        connect(fClient,&EccSystemClient::ConfigurationSignal,this,&NativeRunControl::Configuration);
        connect(fClient,&EccSystemClient::MaintenanceTargetsSignal,this,[this](const QByteArray &body,const QString &error) {
            if(!fConnected || fShuttingDown)return;
            emit CoBoMaintenanceTargetsSignal(body,error);Publish();
        });
        connect(fClient,&EccSystemClient::OwnershipSignal,this,[this](const QByteArray &body,const QString &error) {
            if (!fConnected || fShuttingDown) return;
            emit OwnershipSignal(body,error,fOwnershipRequestGeneration); Publish();
        });
        connect(fClient,&EccSystemClient::ConfigureReadbacksSignal,this,[this](const QByteArray &body,const QString &error) {
            if (!fConnected || fShuttingDown) return;
            emit ConfigureReadbacksSignal(body,error,fReadbacksRequestGeneration);Publish();
        });
        connect(fClient,&EccSystemClient::ReceiverSignal,this,&NativeRunControl::Receivers);
        connect(fClient,&EccSystemClient::ReceiverEventsSignal,this,&NativeRunControl::FinishScanEventCountBaselineQuery);
        connect(fClient,&EccSystemClient::ScanRuntimeIdentitySignal,this,&NativeRunControl::FinishScanRuntimeIdentityQuery);
        connect(fClient,&EccSystemClient::SourceSignal,this,&NativeRunControl::Sources);
        connect(fClient,&EccSystemClient::ReceiverFileBytesSignal,this,&NativeRunControl::AutomaticFileBytes);
        connect(fClient,&EccSystemClient::ReceiverFailureSignal,this,&NativeRunControl::ReceiverFailure);
        connect(fClient,&EccSystemClient::OperationSignal,this,&NativeRunControl::Operation);
        fConnected=true; fBusy=true; fAwaitingStatus=true; fPhase="Native: checking fixed identity"; fPoll.start(config.pollMs); Publish(); fClient->QueryIdentity();
    } catch (const std::exception &e) { if (fClient) { delete fClient; fClient=nullptr; } fStore.reset(); fConnected=fBusy=false; emit LogSignal(QString::fromUtf8(e.what())); Publish(); }
}
void NativeRunControl::Disconnect() {
    if (fActive || fBusy || fDeferredProgressState || (fHaveStatus && (fSystem.controller.state==5 || fSystem.controller.state==6)) || (fStore && !fStore->Pending().empty())) {
        emit LogSignal("Native control has active/pending evidence; finish or inspect it before disconnecting."); return;
    }
    fPoll.stop(); fDeadline.stop();fScan.ReleaseResources();
    delete fClient; fClient=nullptr; fStore.reset(); fConnected=fHaveStatus=fAwaitingStatus=fDiscardStatus=false; fPhase="Disconnected"; emit StatusSignal({}); Publish();
}
