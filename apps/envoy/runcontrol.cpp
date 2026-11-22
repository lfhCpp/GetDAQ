#include "runcontrol.h"
#include "runrecorder.h"
#include <QDateTime>
#include <QUuid>
#include <QDir>
#include <QStandardPaths>
#include "diagnostics.h"

RunControl::RunControl(QObject *parent) : QObject(parent) {
    qRegisterMetaType<AutoRunSettings>();fAutoClock.start();
    fAutoTimer=new QTimer(this);fAutoTimer->setInterval(200);
    connect(fAutoTimer,&QTimer::timeout,this,&RunControl::AutomaticTick);
    connect(this,&RunControl::LogSignal,this,[this](const QString &message){emit LogEntrySignal(message,fLogSeverity<0 ? int(Diagnostics::Classify(message)) : fLogSeverity);});
}

void RunControl::ConnectSlot(RunConfig config)
{
    
    if (fConnected || fBusy || fActive) return;
    QString error;
    if (!config.Validate(&error)) { emit LogSignal(error); return; }
    fProjectConfigurationChecked=false;fRecordingDegraded=false;fCandidateConfigurationSnapshot.clear();fWarningAction={};fWarningToken.clear();fAcceptedWarnings.clear();
    fLocalFinalizationFailed=false;
    fCatalogResultUncertain=false;
    fConfig=config;ConfigureAutoRunSlot(config.autoRun);

    fConfig = config;
    if(!fConfig.configurationSnapshot.isEmpty())fConfig.configurationBackupWarning="The source snapshot is available, but this connection has not confirmed which configuration the existing hardware loaded. Configure through this GUI to establish that association.";
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
    emit LogSignal("Hardware control connected; querying actual node states.");
    PollSlot();
}

void RunControl::UpdateRunMetadataSlot(RunConfig config)
{
    
    if (!fConnected || fBusy || fActive) return;
    fConfig.runNumber = config.runNumber;
    fConfig.existingRunPolicy=config.existingRunPolicy;
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
    
    QStringList warnings;if(fActive || fBusy)warnings<<"Local work/Run records are unfinished. Accepted remote commands and raw data are retained.";
    for(const auto &status:fStatus)if(status.state==5 || status.state==6 || !status.eccOnline){warnings<<"Remote acquisition may continue. Disconnect only detaches this GUI; it does not stop hardware.";break;}
    if(!ConfirmWarnings("Disconnect",warnings,[this]{DisconnectSlot();}))return;
    if(fActive || fBusy)Journal("Detached; remote state and incomplete records retained");
    ++fSequence;fWarningAction={};fDeferredAction={};fBusy=false;fActive=false;fStep=-1;
    fAwaitAck.clear();fAwaitState.clear();fAwaitCatalog.clear();fAwaitReplacementCleanup.clear();
 
    if (fPoll) fPoll->stop();
    if (fDeadline) fDeadline->stop();
    ClearClients();
    fConnected = false; fPhase = "Disconnected";
    Publish();
}

void RunControl::ShutdownSlot()
{
    CancelAutoRun("Automatic Run canceled: application shutdown");
    
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
    if(fJournal.value("untracked").toBool())return true;
    fJournal["phase"] = phase;
    fJournal["updated_utc"] = QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
    QString error;
    if (!RunRecorder::WriteJournal(fConfig, fJournal, &error)) {
        fJournal["recording_degraded"]=true;
        if(!fRecordingDegraded){fRecordingDegraded=true;emit LogSignal("Warning: local journal unavailable; hardware controls remain available: "+error);}
        return true;
    }
    return true;
}


bool RunControl::ConfirmWarnings(const QString &operation,const QStringList &warnings,std::function<void()> action){
 if(warnings.isEmpty())return true;
 const auto key=operation+'\n'+warnings.join('\n');
 if(fAcceptedWarnings.contains(key)){ReportLog("Operator accepted "+operation+": "+warnings.join("; "),Diagnostics::Warning);return true;}
 if(fAutomaticDispatch){CancelAutoRun("Automatic action needs operator confirmation: "+warnings.join("; "));return false;}
 fWarningKey=key;fWarningAction=std::move(action);fWarningToken=QUuid::createUuid().toString();emit OperationWarningSignal(fWarningToken,operation+"\n\n"+warnings.join("\n\n"));return false;
}
void RunControl::AcceptOperationWarningSlot(QString token){
 if(token!=fWarningToken || !fWarningAction)return;
 auto action=std::move(fWarningAction);fWarningAction={};fAcceptedWarnings.insert(fWarningKey);fWarningToken.clear();action();if(!fWarningAction)fAcceptedWarnings.clear();
}

void RunControl::CancelOperationWarningSlot(QString token){if(token==fWarningToken){fWarningToken.clear();fWarningAction={};fAcceptedWarnings.clear();}}
