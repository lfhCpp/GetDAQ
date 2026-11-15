#ifndef GETDAQ_FAILEDHANDOFFWINDOW_H
#define GETDAQ_FAILEDHANDOFFWINDOW_H

#include <QDialog>
#include <QByteArray>
#include <QProcess>
#include <QString>

namespace Ui { class FailedHandoffWindow; }

class FailedHandoffWindow : public QDialog
{
    Q_OBJECT
public:
    explicit FailedHandoffWindow(QWidget *parent=nullptr, const QString &queryProgram=QString());
    ~FailedHandoffWindow() override;
    bool LoadRequestFile(const QString &path,QString *error=nullptr);

private slots:
    void GenerateRequest();
    void SubmitOnce();
    void ObserveSavedOperation();
    void LoadSavedRequest();
    void BrowseOriginalRequest();
    void BrowseCredential();
    void BrowseOutput();
    void ProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    Ui::FailedHandoffWindow *ui;
    QProcess *fProcess=nullptr;
    QString fQueryProgram;
    QString fAction;
    QByteArray fProcessOutput;
    QByteArray fCompletionRequest;
    bool fGenerated=false;
    bool fSubmitConsumed=false;
    bool fBusy=false;
    bool fQueryAvailable=false;
    void InvalidateRequest();
    void UpdateControls();
    QStringList CommonArguments() const;
    bool ValidateInputs(QString *error) const;
    bool ValidateCompletion(const QByteArray &bytes, QString *error) const;
    void StartQuery(const QString &mode, const QStringList &extraArguments);
};

#endif
