#ifndef GETDAQ_DISTRIBUTED_DEPLOYMENT_WINDOW_H
#define GETDAQ_DISTRIBUTED_DEPLOYMENT_WINDOW_H
#include "runconfig.h"
#include "deploymentcontroller.h"
#include <QDialog>
#include <functional>
class QLineEdit;class QSpinBox;class QTableWidget;class QPlainTextEdit;class QComboBox;
class QCheckBox;
class QLabel;class QPushButton;
class DistributedDeploymentWindow:public QDialog {
    Q_OBJECT
public:
    explicit DistributedDeploymentWindow(const RunConfig &base,QWidget *parent=nullptr);
    RunConfig Configuration() const{return fSelected;}
    void OpenProject(const QString &directory);
    void OpenSyncDialog();
    bool Busy() const{return fBusy;}
    void RequestClose();
protected:
    void reject() override;
private slots:
    void ApplyLocalProjectFolder();
private:
    RunConfig fBase,fSelected;
    QJsonObject fMaster,fProject,fPreparation,fActivation;
    QMap<QString,QByteArray> fFiles;
    QLineEdit *fSource,*fRoot,*fAddress,*fInvitation,*fLocalDataRoot;
    QSpinBox *fPort;
    QTableWidget *fHosts,*fGroups;
    QComboBox *fDailyGroups;
    QCheckBox *fAdvanced,*fSync;
    QPlainTextEdit *fLog;
    QWidget *fControls;
    bool fBusy=false;
    bool fConfigurationWarningAccepted=false;
    bool fCloseAfterWait=false;
    QLabel *fWaitStatus;
    QLabel *fPathSummary;
    QPushButton *fStopWaiting;
    QString fWaitStage;
    QElapsedTimer fWaitClock;
    std::shared_ptr<std::atomic_bool> fCanceled;
    void SaveProject();
    void ChooseProjectDirectory();
    void MoveProjectDirectory(const QString &directory);
    void RefreshSourceFiles();
    void RefreshTables();
    void UpdatePathSummary();
    void Async(std::function<QJsonObject()> task,std::function<void(QJsonObject)> done);
    Deployment::Logger Logger();
    void StartMaster();
    void PairHost();
    void LocalNode();
    void EditGroup(bool existing);
    void RemoveGroup();
    void Publish();
    void Activate();
    void Observe();
    void StopSelected(bool repair);
    void ReviewTerminatedGroup();
    void UseSelected();
    void StartAndUse();
    void StopProjectServices();
    QString SelectedGroup() const;
    QSet<QString> SelectedGroups() const;
};
#endif
