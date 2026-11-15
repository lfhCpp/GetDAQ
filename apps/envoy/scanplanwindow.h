#ifndef GETDAQ_SCANPLANWINDOW_H
#define GETDAQ_SCANPLANWINDOW_H

#include <QDialog>
#include <QByteArray>
#include <QProcess>

#include <memory>

class QCloseEvent;
class QComboBox;
class QLineEdit;
class QSpinBox;
class QLabel;
class QPushButton;
class QWidget;
class QVBoxLayout;
class ScanPointBindingSession;

// Low-rate scan planning/control metadata UI. It delegates deterministic plan
// creation to the sibling CLI and can request read-only ECC identity/baseline
// observations; it never handles DAQ payloads.
class ScanPlanWindow : public QDialog
{
    Q_OBJECT

public:
    explicit ScanPlanWindow(QWidget *parent = nullptr);
    ~ScanPlanWindow() override;
    void SetReviewStorageDirectory(const QString &directory);
    void SetNativeCatalogEnabled(bool enabled);

signals:
    void QueryScanRuntimeIdentityRequested(QString journalPath, QString planDirectory,
                                           QByteArray snapshotBytes, unsigned int pointIndex);
    void QueryScanEventCountBaselineRequested(QString journalPath);
    void ExecuteScanPointLifecycleOperationRequested(QString journalPath, QString command);
    void ObservePendingScanLifecycleOperationRequested(QString journalPath,
                                                       QString operationId,
                                                       QString expectedPlanSha256,
                                                       QByteArray snapshotBytes);
    void StartScanPointRequested(QString journalPath);

public slots:
    void RuntimeIdentityQueryFinished(const QByteArray &journalBytes,
                                      const QByteArray &responseBytes,
                                      const QString &error);
    void EventBaselineQueryFinished(const QByteArray &journalBytes,
                                   const QByteArray &responseBytes,
                                   const QString &error);
    void ScanPointRunStatus(QString message,bool active);

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void ChooseArtifact();
    void ChooseOutputParent();
    void OpenExistingPlan();
    void ReconcilePendingOperation();
    void QueryCurrentRuntimeIdentity();
    void CaptureCurrentEventBaseline();
    void AdvanceCurrentScanPoint();
    void StartCurrentScanPoint();
    void BindCompletedRun();
    void SummarizeCompletedRuns();
    void CreateExecutionPolicy();
    void UpdateMode();
    void CreatePlan();
    void RecordPlanReview();
    QString ResolveCatalogExecutable();
    QString ResolveConfigExecutable() const;
    void UpdatePolicyControls();
    void ProcessFinished(int exitCode, QProcess::ExitStatus status);
    void ProcessError(QProcess::ProcessError error);
    void HandleCatalogProcessFinished(int exitCode, QProcess::ExitStatus status,
                                      const QByteArray &outputBytes,
                                      const QString &output, const QString &error);
    void HandlePolicyProcessFinished(int exitCode, QProcess::ExitStatus status,
                                     const QString &output, const QString &error);

private:
    void BuildUi();
    void BuildPlanForm(QVBoxLayout *layout);
    void BuildExecutionPolicyControls(QVBoxLayout *layout);
    void BuildRuntimeControls(QVBoxLayout *layout, QWidget *page);
    void BuildJournalControls(QVBoxLayout *layout);

    QComboBox *fKind = nullptr;
    QLineEdit *fArtifact = nullptr;
    QLineEdit *fCoBo = nullptr;
    QLineEdit *fOutputParent = nullptr;
    QLineEdit *fFolder = nullptr;
    QString fPlanOutputDirectory;
    QSpinBox *fStart = nullptr;
    QSpinBox *fStop = nullptr;
    QSpinBox *fStep = nullptr;
    QSpinBox *fAsAd = nullptr;
    QSpinBox *fAget = nullptr;
    QSpinBox *fFirstChannel = nullptr;
    QSpinBox *fLastChannel = nullptr;
    QLabel *fRangeLabel = nullptr;
    QWidget *fChipControls = nullptr;
    QWidget *fChannelControls = nullptr;
    QWidget *fPolicyControls = nullptr;
    QWidget *fEventPolicyControls = nullptr;
    QLabel *fThresholdPolicyNotice = nullptr;
    QSpinBox *fPolicyEvents = nullptr;
    QSpinBox *fPolicyMaximumDuration = nullptr;
    QLabel *fStatus = nullptr;
    QLabel *fCurrentPointState = nullptr;
    QPushButton *fCreate = nullptr;
    QPushButton *fOpenExisting = nullptr;
    QPushButton *fReview = nullptr;
    QPushButton *fRecovery = nullptr;
    QPushButton *fBindRun = nullptr;
    QPushButton *fSummarizeRuns = nullptr;
    QPushButton *fCreatePolicy = nullptr;
    QPushButton *fQueryRuntimeIdentity = nullptr;
    QPushButton *fCaptureEventBaseline = nullptr;
    QPushButton *fStartScanPoint = nullptr;
    QPushButton *fAdvanceScanPoint = nullptr;
    QProcess *fProcess = nullptr;
    std::unique_ptr<ScanPointBindingSession> fScanBindingSession;
    QString fCatalogOperation;
    QString fPendingScanBindingOperationId;
    QString fPendingScanBindingPath;
    bool fPolicyOperation = false;
    bool fPolicyChecking = false;
    QString fPolicyOutputPath;
    QString fCatalogExecutable;
    QString fPlanSha256;
    QString fPlanKind;
    QString fReviewStorageDirectory;
    QString fPendingRuntimeJournalPath;
    int fPlanPointCount = 0;
    bool fRuntimeQueryPending = false;
    bool fNativeCatalogEnabled = false;
    bool fScanPointBindingOperation = false;
    bool fAwaitingEventBaseline = false;
    bool fAdoptingRuntimeIdentity = false;
    bool fRuntimeIdentityVerified = false;
    QString fVerifiedRuntimeState;
    QString fPendingRuntimeState;
};

#endif
