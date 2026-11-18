#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "diagnostichighlighter.h"
#include "servicelogpanel.h"
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFileDialog>
#include <QMessageBox>
#include <QSaveFile>

void MainWindow::ShowLogSlot(QString message) {ShowLogEntry(message,Diagnostics::Classify(message));}
void MainWindow::ShowLogEntry(QString message,int severity)
{
    if(severity>=Diagnostics::Warning) {
        bool found=false;for(const auto &entry:fNotifications)if(entry.message==message){found=true;break;}
        if(!found){Notification entry;entry.message=message;entry.severity=severity;fNotifications.append(entry);if(fNotifications.size()>200)fNotifications.removeFirst();}
    }
    const QString line = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz ") + message;
    ui->Log->appendPlainText(line);
    if(fServiceLogs)fServiceLogs->RecordEvent(line);
    auto block=ui->Log->document()->lastBlock();
    for(int remaining=line.count('\n')+1;remaining>0 && block.isValid();--remaining,block=block.previous()) {
        block.setUserData(new DiagnosticBlockData(severity));fLogHighlighter->rehighlightBlock(block);
    }
    fLogStore.Append((line + "\n").toUtf8());
}
void MainWindow::StartConnectionLog()
{
    if (fLogStore.HasConnectionLog()) EndConnectionLog(tr("Previous connection closed"));
    if (!fLogStore.OpenConnectionLog()) {
        ShowLogSlot(tr("Cannot open connection log %1: %2").arg(fLogStore.ConnectionLogPath(),fLogStore.ErrorString()));
        return;
    }
    ShowLogSlot(tr("Connection session log: %1").arg(fLogStore.ConnectionLogPath()));
}
void MainWindow::EndConnectionLog(const QString &message)
{
    if (!fLogStore.HasConnectionLog()) return;
    ShowLogSlot(message);
    fLogStore.CloseConnectionLog();
}
