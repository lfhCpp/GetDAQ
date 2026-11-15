#ifndef RUNCONTROL_H
#define RUNCONTROL_H
#include "runconfig.h"
#include "eccclient.h"
#include "sentryclient.h"
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QJsonObject>
#include <QSet>
#include <QTimer>
class NativeRunControl;

// 控制对象整体移入 QThread；全部状态只在这个线程更新。
class RunControl : public QObject
{
    Q_OBJECT
public:
    explicit RunControl(QObject *parent = nullptr);
public slots:
    void ConnectSlot(RunConfig config);
    void ConfigureAutoRunSlot(AutoRunSettings settings);
    void SetAutoRunEnabledSlot(bool enabled);
    void SetOfflineToolsOpenSlot(bool open);
    void CancelAutomaticRunSlot(QString reason);
    void UpdateRunMetadataSlot(RunConfig config);
    void DisconnectSlot();
    void ProgressSlot(int id = -1);
    void RegressSlot(int id = -1);
    void StartRunSlot(bool replaceCompletedRun = false);
    void StopRunSlot();
    void FinalizeSlot();
    void RecoverSlot();
    void ResumeCatalogReplacementSlot(QString failedOperationId,QString operationId);
    void ShutdownSlot();
    void StopProductionAfterFailureSlot();
    void AbortFailedReceiversSlot();
    void RetireFailedReceiversSlot();
    void RetireFailedSourcesSlot();
    void ReconcileAcquisitionWithSourcesSlot();
    void UnprepareAcquisitionAfterFailureSlot();
    void ReconcileDescriptionSlot();
    void ReconcilePreparationSlot();
    void ReconcilePreparationWithAlarmSlot();
    void ReleaseStoppedAlarmSlot();
    void ReconcileAcquisitionSlot();
    void ReconcileAcquisitionWithAlarmSlot();
    void UnprepareAfterFailureSlot();
    void BreakupAfterFailureSlot();
    void PauseSlot();
    void ResumeSlot();
    void InspectConfigurationSlot();
    void InspectOwnershipSlot(quint64 generation);
    void InspectConfigureReadbacksSlot(quint64 generation);
    void RetireDataPlansSlot(QByteArray successorBytes,QByteArray targetsBytes);
    void InspectPlanRetirementReceiptSlot();
    void CompletePlanRetirementSlot(QString originalOperationId);
    void InspectPlanRetirementCompletionSlot();
    void InspectCoBoMaintenanceTargetsSlot();
    void ReprepareCoBoSlot(QString coboId,QString instanceId,QString snapshotSha256);
    void ConfigureCoBoSlot(QString coboId,QString instanceId,QString snapshotSha256);
    void SetCoBoLedSlot(QString coboId,unsigned int asadId,unsigned int ledType,unsigned int ledState,QString instanceId,QString snapshotSha256);
    void BlinkCoBoLedsSlot(QString coboId,QString instanceId,QString snapshotSha256);
    void FlipCoBoLedSlot(QString coboId,unsigned int ledId,QString instanceId,QString snapshotSha256);
    void PulseCoBoLedSlot(QString coboId,unsigned int ledId,unsigned int periodMs,QString instanceId,QString snapshotSha256);
    void TriggerAsAdPulserSlot(QString coboId,int voltageMv,QString instanceId,QString snapshotSha256);
    void QueryScanRuntimeIdentitySlot(QString journalPath,QString planDirectory,
                                      QByteArray snapshotBytes,unsigned int pointIndex);
    void QueryScanEventCountBaselineSlot(QString journalPath);
    void ExecuteScanPointLifecycleOperationSlot(QString journalPath,QString command);
    void ObservePendingScanLifecycleOperationSlot(QString journalPath,QString operationId,
                                                  QString expectedPlanSha256,
                                                  QByteArray snapshotBytes);
    void StartScanPointSlot(QString journalPath);
signals:
    void StatusSignal(QVector<NodeStatus> status);
    void ControlSignal(QString phase, bool connected, bool busy, bool active);
    void LogSignal(QString message);
    void LogEntrySignal(QString message,int severity);
    void AutoRunStatusSignal(QString text,bool armed);
    void AutoRunEnabledSignal(bool enabled);
    void RunFinishedSignal(int nextRun);
    void ShutdownFinishedSignal();
    void ConfigurationSignal(QByteArray body,QString error);
    void OwnershipSignal(QByteArray body,QString error,quint64 generation);
    void ConfigureReadbacksSignal(QByteArray body,QString error,quint64 generation);
    void PlanTransitionStateSignal(bool canSubmit,bool canInspect,QByteArray status,QByteArray identity,QString reason);
    void PlanTransitionResultSignal(QByteArray receipt,QByteArray request,QString error);
    void CoBoMaintenanceStateSignal(bool canSubmit,QByteArray status,QByteArray identity,QString reason);
    void CoBoMaintenanceTargetsSignal(QByteArray body,QString error);
    void CoBoMaintenanceResultSignal(QByteArray operation,QString error);
    void ScanRuntimeIdentityBoundSignal(QByteArray journalBytes,QByteArray responseBytes,QString error);
    void ScanEventCountBaselineBoundSignal(QByteArray journalBytes,QByteArray responseBytes,QString error);
    void ScanPointRunStatusSignal(QString message,bool active);
    void CatalogRecoveryStateSignal(bool canResume,QString failedOperationId,QString reason);
    void PlanRetirementHoldSignal(bool held,QString snapshot);
    void PlanCompletionStateSignal(bool canComplete,bool canObserve,QByteArray originalRequest,QByteArray originalStatus,QByteArray completionRequest,QString completionPhase,QString reason);
private slots:
    void PollSlot();
    void EccReplySlot(int id, QString operation, int state, int transition, QString error);
    void SentryReplySlot(int id, bool catalog, NodeStatus status, QString error);
    void ReplacementFinalizedSlot(int id, QString error);
    void FileFinishedSlot();
private:
    bool fOfflineToolsOpen=false;
    AutoRunPolicy fAutoRun;
    QElapsedTimer fAutoClock;
    QTimer *fAutoTimer=nullptr;
    bool fAutomaticDispatch=false;
    int fLogSeverity=-1;
    AutoRunObservation AutoRunView() const;
    void AutomaticTick();
    void CancelAutoRun(const QString &reason);
    void ReportLog(const QString &message,int severity);
    struct Step { QVector<int> ids; QString operation; int target; };
    RunConfig fConfig;
    QMap<int, EccClient *> fEcc;
    QMap<int, SentryClient *> fSentry;
    QMap<int, NodeStatus> fStatus;
    QVector<Step> fPlan;
    QSet<int> fAwaitAck;
    QSet<int> fAwaitState;
    QSet<int> fAwaitCatalog;
    QSet<int> fAwaitReplacementCleanup;
    QTimer *fPoll = nullptr;
    QTimer *fDeadline = nullptr;
    QFutureWatcher<QString> *fFileTask = nullptr;
    QElapsedTimer fClock;
    QElapsedTimer fRunClock;
    QJsonObject fJournal;
    QString fPhase = "Disconnected";
    QString fOperation;
    int fStep = -1;
    bool fConnected = false;
    bool fBusy = false;
    bool fActive = false;
    bool fLocalFinalizationFailed = false;
    bool fCatalogResultUncertain = false;
    bool fShuttingDown = false;
    int fSequence = 0;
    int fMutant = -1;
    double fDuration = 0;
    QString fReplacementOperationId;
    NativeRunControl *fNative = nullptr;

    QVector<int> Cobos() const;
    QVector<int> All() const;
    bool AllAt(const QVector<int> &ids, int state) const;
    bool Fresh(int id) const;
    bool Available();
    void Publish();
    void Begin(const QString &operation, const QVector<Step> &plan);
    void NextStep();
    void FinishPlan();
    void Fail(const QString &error);
    bool Journal(const QString &phase);
    void StartCatalog();
    void StartFiles();
    void StartReplacementCleanup();
    void CompleteRun();
    void ClearClients();
};
#endif
