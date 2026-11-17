#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgetplot2d.h"
#include "diagnostichighlighter.h"
#include "autorunwidget.h"
#include <QAction>
#include <QFileInfo>
#include <QMessageBox>
#include <QUuid>

void MainWindow::on_Connect_clicked()
{
    if (!ReadConfig()) return;

    ShowLogSlot(tr("Connection requested."));
    ui->Connect->setEnabled(false);
    emit ConnectSignal(fConfig);
}
void MainWindow::on_Disconnect_clicked() { emit DisconnectSignal(); }
void MainWindow::on_Progress_clicked() { emit ProgressSignal(-1); }
void MainWindow::on_Regress_clicked() { emit RegressSignal(-1); }
void MainWindow::on_StartRun_clicked()
{
    if(!RequireOfflineToolsClosed())return;
    if (!ReadConfig()) return;
    const QFileInfo target(fConfig.RunDirectory());
    if (target.isSymLink()) {
        QMessageBox::warning(this, tr("Cannot replace this Run"),
                             tr("The selected Run path is a symbolic link. Preserve it and choose another Run number."));
        return;
    }
    bool replaceCompletedRun = false;
    if (target.exists()) {
        const QString prompt = tr("Run %1 / %2 already exists. Start a new acquisition using this same Run number?\n\n"
                 "The previous local records will first be moved to a separate backup directory. "
                 "Interrupted or warning Run records are retained and will not be deleted automatically. "
                 "Existing raw data is retained until the receiver confirms replacement; an archive error remains a warning.\n\n"
                 "Unarchived files already in the incoming directory will remain there and may be archived together with this same-number Run.\n\n"
                 "Continue only when the previous acquisition is stopped and no other controller is using this Run.");
        const QString formattedPrompt=prompt.arg(fConfig.experiment).arg(fConfig.runNumber);
        if (QMessageBox::question(this, tr("Confirm same Run number replacement"), formattedPrompt,
                                  QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
            return;
        replaceCompletedRun = true;
    }
    ApplyAutoRunSettings(fConfig.autoRun);
    emit UpdateRunMetadataSignal(fConfig);
    fWaveformViewerAction->setEnabled(false);
    fDataToolsAction->setEnabled(false);
    fPlot->Clear(); emit StartSignal(replaceCompletedRun);
}
void MainWindow::on_StopRun_clicked() {
    if(!fActive && fAutomaticArmed)emit CancelAutomaticRunSignal(tr("Automatic Run canceled: manual Run stop during the interval."));
    else emit StopSignal();
}
void MainWindow::on_Finalize_clicked() { emit FinalizeSignal(); }
void MainWindow::on_Recover_clicked() { emit RecoverSignal(); }
void MainWindow::ApplyAutoRunSettings(const AutoRunSettings &settings) {
    if(fActive || fAutomaticArmed) {ShowLogEntry("Automatic Run settings are locked until the Run or automatic sequence finishes. Use Run stop to cancel.",Diagnostics::Warning);return;}
    QString error;if(!settings.Validate(&error)){ShowLogEntry(error,Diagnostics::Error);return;}
    fConfig.autoRun=settings;fAutoRunWidget->SetSettings(settings);emit ConfigureAutoRunSignal(settings);
}
