#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include "runconfig.h"
#include "consolelogstore.h"
#include <QMainWindow>
#include <QThread>
#include <QElapsedTimer>
#include <QPointer>
class RunControl;
class ServiceLogPanel;
class AutoRunWidget;
class DiagnosticHighlighter;
class WidgetPlot2D;
class QAction;
class QMenu;
class QLabel;
class QTableWidget;
class OfflineDataToolsWindow;
class ApplicationInstance;
class QTimer;
namespace Ui { class MainWindow; }

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    void SetApplicationInstance(ApplicationInstance *instance){fApplicationInstance=instance;}
    bool LoadConfig(const QString &path);
    void SetConfig(const RunConfig &config);
signals:
    void ConnectSignal(RunConfig config);
    void AcceptOperationWarningSignal(QString token);
    void CancelOperationWarningSignal(QString token);
    void ConfigureAutoRunSignal(AutoRunSettings settings);
    void SetAutoRunEnabledSignal(bool enabled);
    void CancelAutomaticRunSignal(QString reason);
    void UpdateRunMetadataSignal(RunConfig config);
    void DisconnectSignal();
    void ProgressSignal(int id);
    void RegressSignal(int id);
    void StartSignal(bool replaceCompletedRun, bool acceptExistingIncoming = false);
    void OfflineToolsStateSignal(bool open);
    void StopSignal();
    void FinalizeSignal();
    void RecoverSignal();
    void ShutdownSignal();
    void PauseSignal();
    void ResumeSignal();
protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
private slots:
    void on_Connect_clicked();
    void on_Disconnect_clicked();
    void on_Progress_clicked();
    void on_Regress_clicked();
    void on_StartRun_clicked();
    void on_StopRun_clicked();
    void on_Finalize_clicked();
    void on_Recover_clicked();
    void NewConfiguration();
    void on_LoadConfig_clicked();
    void on_SaveConfig_clicked();
    void ExportVisibleLog();
    void on_Nodes_clicked();
    void on_AddField_clicked();
    void on_RemoveField_clicked();
    void on_NodeProgress_clicked();
    void on_NodeRegress_clicked();
    void ShowStatusSlot(QVector<NodeStatus> status);
    void ShowControlSlot(QString phase, bool connected, bool busy, bool active);
    void ShowLogSlot(QString message);
    void ShowLogEntry(QString message,int severity);
private:
    AutoRunWidget *fAutoRunWidget=nullptr;
    bool fAutomaticArmed=false;
    DiagnosticHighlighter *fLogHighlighter=nullptr;
    void ApplyAutoRunSettings(const AutoRunSettings &settings);
    void OpenWaveformViewer();
    void OpenDataTools();
    void RefreshOfflineToolsGate();
    bool RequireOfflineToolsClosed();
    QAction *fWaveformViewerAction=nullptr;
    QPointer<QMainWindow> fWaveformViewer;
    QAction *fDataToolsAction=nullptr;
    bool fDataToolsAvailable=false;
    bool fDataToolsClosing=false;
    QPointer<OfflineDataToolsWindow> fDataTools;
    void InitializeConsole();
    void ArrangeConsole();
    void UpdateStatusPresentation(const QVector<NodeStatus> &status,const QMap<int,QString> &lifecycles={});
    void InitializePar();
    void InitializeMenus();
    void InitializeRecoveryAndSafetyMenus(QMenu *configurationMenu,QMenu *runMenu,QMenu *recoveryMenu);
    void InitializeRunControl();
    void ResizeStatusTableToContents();
    bool ReadConfig();
    void StartConnectionLog();
    void EndConnectionLog(const QString &message);
    int SelectedId() const;
    Ui::MainWindow *ui;
    ServiceLogPanel *fServiceLogs=nullptr;
    QString fOnlineSourcesPath;
    QString OnlineSourcesFile();
    RunConfig fConfig;
    QVector<NodeStatus> fLastNodeStatus;
    QThread fControlThread;
    RunControl *fRunControl=nullptr;
    WidgetPlot2D *fPlot;
    ConsoleLogStore fLogStore;
    ApplicationInstance *fApplicationInstance=nullptr;
    bool AdoptConfiguration(const RunConfig &config);
    int fObservedEccState=-1;
    struct Notification {QString message;int severity;bool acknowledged=false;};
    QVector<Notification> fNotifications;
    void OpenStatusManagement();
    void RememberStartupConfiguration();
    bool fConnected = false;
    bool fBusy = false;
    bool fActive = false;
    bool fRemoteRunning = false;
    bool fClosing = false;
    bool fCanClose = false;
    QElapsedTimer fDisplayClock;
    QAction *fPause=nullptr,*fResume=nullptr;
};
#endif
