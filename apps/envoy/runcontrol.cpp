#include "runcontrol.h"
#include "runrecorder.h"
#include "nativeruncontrol.h"
#include <QDateTime>
#include "diagnostics.h"

RunControl::RunControl(QObject *parent) : QObject(parent) {
    qRegisterMetaType<AutoRunSettings>();fAutoClock.start();
    fAutoTimer=new QTimer(this);fAutoTimer->setInterval(200);
    connect(fAutoTimer,&QTimer::timeout,this,&RunControl::AutomaticTick);
    connect(this,&RunControl::LogSignal,this,[this](const QString &message){emit LogEntrySignal(message,fLogSeverity<0 ? int(Diagnostics::Classify(message)) : fLogSeverity);});
}

void RunControl::ConnectSlot(RunConfig config)
{
    if (fNative) { emit LogSignal("Native controller already exists; disconnect before changing deployment."); return; }
    if (fConnected || fBusy || fActive) return;
    QString error;
    if (!config.Validate(&error)) { emit LogSignal(error); return; }
    fLocalFinalizationFailed=false;
    fCatalogResultUncertain=false;
    fConfig=config;ConfigureAutoRunSlot(config.autoRun);
    if (config.nativeEcc) {
        fNative=new NativeRunControl(this);
        connect(fNative,&NativeRunControl::StatusSignal,this,&RunControl::StatusSignal);
        NativeRunControl *native=fNative;
        connect(native,&NativeRunControl::ControlSignal,this,[this,native](const QString &phase,bool connected,bool busy,bool active) {
            emit ControlSignal(phase,connected,busy,active);
            if (!connected && !busy && !active && fNative==native) { fNative=nullptr; native->deleteLater(); }
        });
        connect(fNative,&NativeRunControl::LogSignal,this,[this,native](const QString &message){ReportLog(message,native->CurrentLogSeverity()<0 ? Diagnostics::Classify(message) : native->CurrentLogSeverity());});
        connect(fNative,&NativeRunControl::FailureSignal,this,[this](const QString &error){CancelAutoRun(error);});
        connect(fNative,&NativeRunControl::AutoRunObservationErrorSignal,this,[this](const QString &error){CancelAutoRun(error);ReportLog(error,Diagnostics::Error);});
        connect(fNative,&NativeRunControl::ExternalOperationSignal,this,[this](int state){if(state!=5 && state!=6)CancelAutoRun("Automatic Run canceled: external/MCP manual control changed the acquisition state.");});
        connect(fNative,&NativeRunControl::RunFinishedSignal,this,[this](int next){fConfig.runNumber=next;fAutoRun.Completed(fAutoClock.elapsed());emit RunFinishedSignal(next);});
        connect(fNative,&NativeRunControl::ConfigurationSignal,this,&RunControl::ConfigurationSignal);
        connect(fNative,&NativeRunControl::OwnershipSignal,this,&RunControl::OwnershipSignal);
        connect(fNative,&NativeRunControl::ConfigureReadbacksSignal,this,&RunControl::ConfigureReadbacksSignal);
        connect(fNative,&NativeRunControl::PlanTransitionStateSignal,this,&RunControl::PlanTransitionStateSignal);
        connect(fNative,&NativeRunControl::PlanTransitionResultSignal,this,&RunControl::PlanTransitionResultSignal);
        connect(fNative,&NativeRunControl::PlanRetirementHoldSignal,this,&RunControl::PlanRetirementHoldSignal);
        connect(fNative,&NativeRunControl::PlanCompletionStateSignal,this,&RunControl::PlanCompletionStateSignal);
        connect(fNative,&NativeRunControl::CoBoMaintenanceStateSignal,this,&RunControl::CoBoMaintenanceStateSignal);
        connect(fNative,&NativeRunControl::CoBoMaintenanceTargetsSignal,this,&RunControl::CoBoMaintenanceTargetsSignal);
        connect(fNative,&NativeRunControl::CoBoMaintenanceResultSignal,this,&RunControl::CoBoMaintenanceResultSignal);
        connect(fNative,&NativeRunControl::ScanRuntimeIdentityBoundSignal,this,&RunControl::ScanRuntimeIdentityBoundSignal);
        connect(fNative,&NativeRunControl::ScanEventCountBaselineBoundSignal,this,&RunControl::ScanEventCountBaselineBoundSignal);
        connect(fNative,&NativeRunControl::ScanPointRunStatusSignal,this,&RunControl::ScanPointRunStatusSignal);
        connect(fNative,&NativeRunControl::CatalogRecoveryStateSignal,this,&RunControl::CatalogRecoveryStateSignal);
        connect(fNative,&NativeRunControl::ShutdownFinishedSignal,this,&RunControl::ShutdownFinishedSignal);
        fNative->Connect(config); return;
    }
    fConfig = config;
    fReplacementOperationId.clear();
    fMutant = -1;
    fClock.start();
    if (!fPoll) {
        fPoll = new QTimer(this);
        fDeadline = new QTimer(this);
        fDeadline->setSingleShot(true);
        connect(fPoll, &QTimer::timeout, this, &RunControl::PollSlot);
        connect(fDeadline, &QTimer::timeout, this, [this]() { Fail("Transition deadline exceeded in " + fPhase + ". Query states before recovery."); });
        fFileTask = new QFutureWatcher<QString>(this);
        connect(fFileTask, &QFutureWatcher<QString>::finished, this, &RunControl::FileFinishedSlot);
    }
    for (const NodeConfig &node : config.nodes) {
        NodeStatus status; status.id = node.id; fStatus.insert(node.id, status);
        EccClient *ecc = new EccClient(node, config, this);
        fEcc.insert(node.id, ecc);
        connect(ecc, &EccClient::ReplySignal, this, &RunControl::EccReplySlot);
        if (node.mutant) fMutant = node.id;
        else {
            SentryClient *sentry = new SentryClient(node, config, this);
            fSentry.insert(node.id, sentry);
            connect(sentry, &SentryClient::ReplySignal, this, &RunControl::SentryReplySlot);
            connect(sentry, &SentryClient::ReplacementFinalizedSignal, this, &RunControl::ReplacementFinalizedSlot);
        }
    }
    fConnected = true;
    fPhase = "Monitoring";
    fPoll->start(config.pollMs);
    emit LogSignal(config.simulation ? "SIMULATION: no requests sent to hardware." : "Hardware control connected; querying actual node states.");
    PollSlot();
}

void RunControl::UpdateRunMetadataSlot(RunConfig config)
{
    if (fNative) {
        fNative->UpdateRunMetadata(config);
        return;
    }
    if (!fConnected || fBusy || fActive) return;
    fConfig.runNumber = config.runNumber;
    fConfig.autoRun=config.autoRun;
    fConfig.description = config.description;
    fConfig.fields = config.fields;
    for (auto *client : fSentry) client->SetRunMetadata(config);
}

void RunControl::ClearClients()
{
    for (auto client : fEcc) { client->Cancel(); client->deleteLater(); }
    for (auto client : fSentry) { client->Cancel(); client->deleteLater(); }
    fEcc.clear(); fSentry.clear(); fStatus.clear();
}

void RunControl::DisconnectSlot()
{
    CancelAutoRun("Automatic Run canceled: manual disconnect");
    if (fNative) { fNative->Disconnect(); return; }
    if (fBusy) { emit LogSignal("Wait for the current operation before disconnecting."); return; }
    if (fActive) {
        if ((!fLocalFinalizationFailed && !fCatalogResultUncertain) || !AllAt(All(),4)) {
            emit LogSignal("Stop and confirm every ECC Ready before disconnecting an unfinished Run."); return;
        }
        if (!Journal(fCatalogResultUncertain ? "Catalog inspection required (disconnected)" : "Finalization pending (disconnected)")) {
            emit LogSignal("Cannot preserve the pending Run journal; disconnect canceled."); return;
        }
        emit LogSignal("Disconnected from stopped ECC. Incomplete Run records retained; reopen run.yml and Recover journal to resume finalization. Background services remain running.");
        fActive=false;fLocalFinalizationFailed=false;fCatalogResultUncertain=false;
    }
    for (const NodeStatus &s : fStatus) if (s.state == 5 || s.state == 6) { emit LogSignal("Observed running/paused hardware; stop it before disconnecting."); return; }
    if (fPoll) fPoll->stop();
    if (fDeadline) fDeadline->stop();
    ClearClients();
    fConnected = false; fPhase = "Disconnected";
    Publish();
}

void RunControl::ShutdownSlot()
{
    CancelAutoRun("Automatic Run canceled: application shutdown");
    if (fNative) { fNative->Shutdown(); return; }
    fShuttingDown = true;
    ++fSequence;
    if (fPoll) fPoll->stop();
    if (fDeadline) fDeadline->stop();
    if (fActive) Journal("Interrupted");
    ClearClients();
    // 文件任务持有配置副本，等它退出再关闭线程，避免析构仍在运行的线程。
    if (fFileTask && fFileTask->isRunning()) return;
    emit ShutdownFinishedSignal();
}

bool RunControl::Journal(const QString &phase)
{
    fJournal["phase"] = phase;
    fJournal["updated_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString error;
    if (!RunRecorder::WriteJournal(fConfig, fJournal, &error)) { emit LogSignal("Journal write failed: " + error); return false; }
    return true;
}

