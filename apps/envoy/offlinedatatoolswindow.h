#ifndef GETDAQ_OFFLINE_DATA_TOOLS_WINDOW_H
#define GETDAQ_OFFLINE_DATA_TOOLS_WINDOW_H
#include <QWidget>
#include <QStringList>
class QComboBox;
class QListWidget;
class QLineEdit;
class QSpinBox;
class QPlainTextEdit;
class QLabel;
class QPushButton;
class QProcess;

// Runs the existing offline programs as children; never opens a DAQ connection.
class OfflineDataToolsWindow : public QWidget {
    Q_OBJECT
public:
    explicit OfflineDataToolsWindow(QWidget *parent=nullptr,const QString &programDirectory=QString());
    ~OfflineDataToolsWindow() override;
    static QString FindProgram(const QString &name,const QString &directory=QString());
    static bool HasAvailableTool();
signals:
    void ClosePendingSignal();
protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
private:
    void UpdateMode();
    void UpdateActions();
    bool PrepareCommand(QStringList *arguments,QString *error) const;
    void Start();
    void Stop();
    void DrainOutput();
    void Finished(int exitCode,bool normal);
    void Append(const QString &text);
    QComboBox *fMode;
    QListWidget *fInputs;
    QLineEdit *fOutput;
    QSpinBox *fCount;
    QPlainTextEdit *fLog;
    QLabel *fStatus;
    QWidget *fSettings;
    QPushButton *fStart,*fStop,*fOutputBrowse,*fRemove;
    QProcess *fProcess;
    QStringList fPrograms;
    bool fClosing=false;
    bool fCancelled=false;
    qint64 fDisplayedBytes=0;
};
#endif
