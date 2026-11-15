#ifndef GETDAQ_LOCAL_SERVICES_WINDOW_H
#define GETDAQ_LOCAL_SERVICES_WINDOW_H
#include <QDialog>
#include <QProcess>
#include "runconfig.h"
class LocalDeploymentServices;
class QLabel;
class QComboBox;class QLineEdit;class QSpinBox;class QPushButton;class QPlainTextEdit;class QCheckBox;
// The check process belongs to this window; launched DAQ services never do.
class LocalServicesWindow:public QDialog {
    Q_OBJECT
public:
    explicit LocalServicesWindow(const RunConfig &config,QWidget *parent=nullptr);
    RunConfig Configuration() const {return fRunConfig;}
protected:
    void reject() override;
private:
    RunConfig fRunConfig;
    LocalDeploymentServices *fBackend=nullptr;
    QWidget *fBackendActions=nullptr;
    QComboBox *fInstances;
    QLabel *fSummary;
    void SelectBackend(const RunConfig &config);
    void CreateBackendInstance();
    void BackendAction(int action);
    QComboBox *fMode;
    QLineEdit *fPrograms,*fConfig,*fOutput,*fBind,*fLogs;
    QSpinBox *fDataPort,*fStatusPort;
    QWidget *fConfigRow,*fReceiverFields,*fSettings;
    QPushButton *fStart;
    QPlainTextEdit *fLog;
    QCheckBox *fStandalone;
    QProcess *fCheck;
    QString fProgram;
    QStringList fArguments;
    bool fBusy=false;
    quint64 fGeneration=0;
    void UpdateMode();
    void Start();
    void Detached();
    void Failure(const QString &message);
};
#endif
