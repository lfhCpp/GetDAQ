#ifndef NATIVERUNCONTROL_H
#define NATIVERUNCONTROL_H
#include "runconfig.h"
#include "eccclientstore.h"
#include "nativestatusobservations.h"
#include "../../common/experiment/scanexecutionsnapshot.h"
#include "../../common/control/scanruntimeidentity.h"
#include <QElapsedTimer>
#include <QTimer>
#include <QJsonObject>
namespace GetDAQ {
class ScanExecutionJournalStore;
class ScanRuntimeIdentityCoordinator;
class ScanEventCountBaselineCoordinator;
class ScanOperationRunner;
class ScanEventCountMonitor;
}
class NativeRunControl : public QObject
{
    Q_OBJECT
public:
    explicit NativeRunControl(QObject *parent = nullptr);
    ~NativeRunControl() override;
    void SetAutomaticSettings(const AutoRunSettings &settings) {fConfig.autoRun=settings;}
    AutoRunObservation AutomationObservation() const;
    void RequestAutomaticFileBytes();
    int CurrentLogSeverity() const {return fLogSeverity;}
    void Connect(const RunConfig &config);
    void UpdateRunMetadata(const RunConfig &config);
    void Disconnect();
    void Shutdown();
    void Progress(int id);
    void Regress(int id);
    void StartRun(bool replaceCompletedRun=false);
    void StopRun();
    void Finalize();
    void Recover();
    void ResumeCatalogReplacement(const QString &failedOperationId,const QString &operationId);
    void Pause();
    void Resume();
    void StopProductionAfterFailure();
    void ReconcileDescription();
    void ReconcilePreparation();
    void ReconcilePreparationWithAlarm();
    void ReleaseStoppedAlarm();
    void ReconcileAcquisition();
    void ReconcileAcquisitionWithAlarm();
    void UnprepareAfterFailure();
    void BreakupAfterFailure();
    void AbortFailedReceivers();
    void RetireFailedReceivers();
    void RetireFailedSources();
    void ReconcileAcquisitionWithSources();
    void UnprepareAcquisitionAfterFailure();
    void InspectConfiguration();
    void InspectOwnership(quint64 generation);
    void InspectConfigureReadbacks(quint64 generation);
    void RetireDataPlans(const QByteArray &successorBytes,const QByteArray &targetsBytes);
    void InspectPlanRetirementReceipt();
    void CompletePlanRetirement(const QString &originalOperationId);
    void InspectPlanRetirementCompletion();
    void InspectCoBoMaintenanceTargets();
    void ReprepareCoBo(const QString &coboId,const QString &instanceId,const QString &snapshotSha256);
    void ConfigureCoBo(const QString &coboId,const QString &instanceId,const QString &snapshotSha256);
    void SetCoBoLed(const QString &coboId,unsigned int asadId,unsigned int ledType,unsigned int ledState,const QString &instanceId,const QString &snapshotSha256);
    void BlinkCoBoLeds(const QString &coboId,const QString &instanceId,const QString &snapshotSha256);
    void FlipCoBoLed(const QString &coboId,unsigned int ledId,const QString &instanceId,const QString &snapshotSha256);
    void PulseCoBoLed(const QString &coboId,unsigned int ledId,unsigned int periodMs,const QString &instanceId,const QString &snapshotSha256);
    void TriggerAsAdPulser(const QString &coboId,int voltageMv,const QString &instanceId,const QString &snapshotSha256);
    void QueryScanRuntimeIdentity(const QString &journalPath,const QString &planDirectory,
                                  const QByteArray &snapshotBytes,unsigned int pointIndex);
    void QueryScanEventCountBaseline(const QString &journalPath);
    void ExecuteScanPointLifecycleOperation(const QString &journalPath,const QString &command);
    void ObservePendingScanLifecycleOperation(const QString &journalPath,
                                             const QString &operationId,
                                             const QString &expectedPlanSha256,
                                             const QByteArray &snapshotBytes);
    void StartScanPoint(const QString &journalPath);
signals:
    void StatusSignal(QVector<NodeStatus> status);
    void ControlSignal(QString phase, bool connected, bool busy, bool active);
    void LogSignal(QString message);
    void FailureSignal(QString message);
    void AutoRunObservationErrorSignal(QString message);
    void ExternalOperationSignal(int state);
    void RunFinishedSignal(int nextRun);
    void ShutdownFinishedSignal();
    void ConfigurationSignal(QByteArray body,QString error);
    void OwnershipSignal(QByteArray body,QString error,quint64 generation);
    void ConfigureReadbacksSignal(QByteArray body,QString error,quint64 generation);
    void PlanTransitionStateSignal(bool canSubmit,bool canInspect,QByteArray status,QByteArray identity,QString reason);
    void PlanTransitionResultSignal(QByteArray receipt,QByteArray request,QString error);
    void PlanRetirementHoldSignal(bool held,QString snapshot);
    void PlanCompletionStateSignal(bool canComplete,bool canObserve,QByteArray originalRequest,QByteArray originalStatus,QByteArray completionRequest,QString completionPhase,QString reason);
    void CoBoMaintenanceStateSignal(bool canSubmit,QByteArray status,QByteArray identity,QString reason);
    void CoBoMaintenanceTargetsSignal(QByteArray body,QString error);
    void CoBoMaintenanceResultSignal(QByteArray operation,QString error);
    void ScanRuntimeIdentityBoundSignal(QByteArray journalBytes,QByteArray responseBytes,QString error);
    void ScanEventCountBaselineBoundSignal(QByteArray journalBytes,QByteArray responseBytes,QString error);
    void ScanPointRunStatusSignal(QString message,bool active);
    void CatalogRecoveryStateSignal(bool canResume,QString failedOperationId,QString reason);
private:
    QMap<QString,GetDAQ::EccControlReceiverEvents> fAutoFileBytes;
    QMap<QString,qint64> fAutoFileTimes;
    int fAutoFileIndex=0,fLogSeverity=-1;
    std::string fAutoObservedOperation,fAutoOwnedOperation;
    void AutomaticFileBytes(const QString &source,const QByteArray &body,const QString &error);
    struct ScanState {
        std::unique_ptr<GetDAQ::ScanExecutionJournalStore> identityStore;
        std::unique_ptr<GetDAQ::ScanRuntimeIdentityCoordinator> identityCoordinator;
        std::unique_ptr<GetDAQ::ScanExecutionJournalStore> eventBaselineStore;
        std::unique_ptr<GetDAQ::ScanEventCountBaselineCoordinator> eventBaselineCoordinator;
        std::unique_ptr<GetDAQ::ScanOperationRunner> operationRunner;
        std::unique_ptr<GetDAQ::ScanExecutionJournalStore> runStore;
        std::unique_ptr<GetDAQ::ScanEventCountMonitor> eventMonitor;
        std::unique_ptr<GetDAQ::ScanExecutionSnapshot> runtimeSnapshot;
        std::unique_ptr<GetDAQ::ScanRuntimeIdentityExpectation> runtimeExpectation;
        QString runtimeJournalPath;
        unsigned int identityPointIndex=0;
        unsigned int eventsPerPoint=0, maximumDurationMs=0;
        bool thresholdManualOnly=false;
        QByteArray identityResponseBody;
        bool runMonitoring=false, stopForSuccess=false;
        bool pointFinalizationPending=false;
        bool lifecycleRecovery=false;
        QString recoveryControlHoldOperationId;
        QString runOperationId;
        QString lifecycleCommand;
        QString stopReason;
        QByteArray runOperationResponse;
        unsigned int awaitingControllerState=0;
        QElapsedTimer runClock;
        ScanState();
        void ReleaseResources();
        ~ScanState();
    };
    struct RecoveryState {
        bool awaitingMetadata=false;
    };
    struct PlanRetirementState {
        GetDAQ::EccControlRequest request;
        GetDAQ::EccControlSystemStatus receiptStatus;
        bool complete=false;
        bool receiptAfterStatus=false;
        void Reset();
    };

    RunConfig fConfig;
    std::unique_ptr<EccClientStore> fStore;
    ScanState fScan;
    RecoveryState fRecovery;
    PlanRetirementState fPlanRetirement;
    EccSystemClient *fClient = nullptr;
    quint64 fOwnershipRequestGeneration = 0;
    quint64 fReadbacksRequestGeneration = 0;
    QTimer fPoll, fDeadline;
    QElapsedTimer fClock, fRunClock, fIdentityClock, fStatusClock;
    GetDAQ::EccControlSystemStatus fSystem;
    QByteArray fStatusBody, fIdentityBody;
    GetDAQ::EccControlRequest fRequest;
    QString fPhase = "Disconnected", fCommand, fOperationId;
    int fTarget = -1;
    bool fConnected = false, fBusy = false, fActive = false, fHaveStatus = false, fShuttingDown = false;
    bool fIdentityMatched = false;
    bool fAwaitingStatus = false, fDiscardStatus = false;
    unsigned int fDeferredProgressState = 0;
    std::string fDeferredProgressOperation;
    QElapsedTimer fDeferredProgressClock;
    QByteArray fConfigurationBody;
    QVector<NodeStatus> fStatusRows;
    NativeStatusObservations fObservations;
    QJsonObject fJournal;
    double fDuration = 0;
    void Poll();
    void Status(const QByteArray &body, const QString &error);
    void Alarms(const QByteArray &body, const QString &error);
    void ContinueFromStatus();
    void ContinueRunFinalizationFromStatus();
    bool ConfirmScanFinalizationFromStatus(bool catalogued);
    void QueryStatusAfterOperation();
    void Operation(const QString &id, const QString &phase, const QByteArray &body, const QString &error);
    bool Send(const QString &command, int target);
    bool Available() const;
    bool CanSubmitCoBoMaintenance(const QString &instanceId,const QString &snapshotSha256) const;
    bool Journal(const QString &phase);
    void FinishRun();
    void Fail(const QString &message);
    void Publish();
    void Configuration(const QByteArray &body,const QString &error);
    void Receivers(const QByteArray &body,const QString &error);
    void Sources(const QByteArray &body,const QString &error);
    void ReceiverFailure(const QByteArray &body,const QString &error);
    void EmitStatus();
    void FinishScanRuntimeIdentityQuery(const QByteArray &responseBytes,const QString &error);
    void FinishScanEventCountBaselineQuery(const QString &senderId,const QByteArray &responseBytes,const QString &error);
    void FinishScanRunEventQuery(const QString &senderId,const QByteArray &responseBytes,const QString &error);
    void SubmitScanStop(bool successfulPoint,const QString &reason);
    void ConfirmScanOperationControllerState();
    void ConfirmScanLifecycleControllerState();
    void ConfirmScanRunControllerState(unsigned int expected);
    bool HandleCatalogReplacementResult(const QString &id,const QString &phase,
        const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result);
    bool HandleScanOperationResult(const QString &id,const QString &phase,const QByteArray &body,
        const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result);
    bool HandleCoBoMaintenanceResult(const QString &phase,const QByteArray &body,
        const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result);
    bool HandlePlanRetirementResult(const QString &phase,const GetDAQ::EccControlOperation &operation);
    bool HandleRecoveryOperationResult(const QString &phase,const QByteArray &body,
        const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result);
    void HandleLifecycleOperationResult(const QString &phase,
        const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result);
    void RecordScanFinalizationUnknown(const QString &reason);
    void RecordUnresolvedScanOperation(const QString &id,const QString &error);
    void HandleScanOperationDeadline();
};
#endif
