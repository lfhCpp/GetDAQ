#ifndef GETDAQ_HARDWARESETUPWINDOW_H
#define GETDAQ_HARDWARESETUPWINDOW_H
#include "hardwaredeployment.h"
#include <QDialog>
#include <QProcess>
#include <QElapsedTimer>
#include <QJsonArray>
class EarlyWarningRecovery;
class QLineEdit;class QSpinBox;class QTableWidget;class QPlainTextEdit;class QPushButton;class QCheckBox;
class HardwareSetupWindow : public QDialog {
    Q_OBJECT
public:
    explicit HardwareSetupWindow(const RunConfig &base,QWidget *parent=nullptr);
    void StartAcceptedWarningRecovery();
    void PrepareParallelDraft(){NewParallelInstance();}
    RunConfig Configuration() const {return fFiles.console;}
    void EnableProjectDraftMode();
    HardwareDeploymentSetup DraftSetup();
    void SetProjectDraft(const HardwareDeploymentSetup &setup);
    void SetReceivingHosts(const QJsonArray &nodes,const QString &defaultNode);
protected:
    void reject() override;
private:
    QLineEdit *fExperiment,*fProfile,*fPrograms,*fRoot,*fData,*fCallback;
    QSpinBox *fEccPort,*fNodePort,*fCallbackPort;
    QTableWidget *fBoards;
    QPlainTextEdit *fLog;
    QPushButton *fLoad,*fGenerate,*fLaunch,*fUse;
    QCheckBox *fStopped,*fDelayWarning;
    QWidget *fInputs;
    QVector<HardwareBoardSetup> fLoaded;
    RunConfig fBase;
    HardwareDeploymentFiles fFiles;
    QProcess *fProcess;
    QString fStep;
    bool fBusy=false,fLaunched=false,fRecoveryMode=false,fLaunchAfterCheck=false,fProjectDraft=false;
    QJsonObject fRecoveryProof;
    QElapsedTimer fRecoveryClock;
    void StopOriginalForWarningRecovery();
    QString fCheckedDigest;
    QString fSavedDirectory,fSavedInputsDigest;
    QJsonArray fReceivingHosts;
    QString fDefaultReceivingHost;
    qint64 fSupervisorPid=0,fEccPid=0;
    int fIdentityAttempts=0;
    void LoadBoards();
    void PopulateBoards();
    void ReadRows();
    void NewParallelInstance();
    void AddCoBo();
    void RemoveBoard();
    void RestoreSetup(const QString &directory);
    void EditAdvanced();
    void Generate();
    void Launch();
    void StartOrCheckServices();
    void VerifyServices();
    void Execute(const QString &program,const QStringList &arguments,const QString &step);
    void Finished(int code,QProcess::ExitStatus status);
    void Fail(const QString &message);
    void Log(const QString &message);
    void Busy(bool busy);
    QString InputsDigest(bool includeConfirmation=true) const;
};
#endif
