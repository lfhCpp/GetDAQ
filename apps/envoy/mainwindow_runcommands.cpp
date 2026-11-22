#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "widgetplot2d.h"
#include "diagnostichighlighter.h"
#include "autorunwidget.h"
#include <QAction>
#include <QFileInfo>
#include <QMessageBox>
#include <QUuid>
#include <QPushButton>

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

    if (!ReadConfig()) return;
    const QFileInfo target(fConfig.RunDirectory());
    if (target.isSymLink()) {
        QMessageBox::warning(this, tr("Cannot replace this Run"),
                             tr("The selected Run path is a symbolic link. Preserve it and choose another Run number."));
        return;
    }
    bool replaceCompletedRun = false;
    fConfig.existingRunPolicy.clear();
    if (target.exists()) {
        QMessageBox prompt(QMessageBox::Warning,tr("Run already exists"),
            tr("Experiment: %1\nRun: %2\nGUI records: %3\n\n"
               "Overwrite replaces this Run's old records and archived raw files.\n"
               "Keep and add files keeps the same Run number and directory; existing timestamped raw files remain and new files are added.\n"
               "Raw files are archived by Sentry after Stop. Unarchived incoming files are included; files outside this Run are not deleted." )
                .arg(fConfig.experiment).arg(fConfig.runNumber).arg(target.absoluteFilePath()),QMessageBox::NoButton,this);
        prompt.setObjectName("ExistingRunFileChoice");
        auto *overwrite=prompt.addButton(tr("Overwrite"),QMessageBox::DestructiveRole);overwrite->setObjectName("OverwriteExistingRun");
        auto *keep=prompt.addButton(tr("Keep and add files"),QMessageBox::AcceptRole);keep->setObjectName("KeepAndAddRunFiles");
        auto *cancel=prompt.addButton(QMessageBox::Cancel);cancel->setObjectName("CancelExistingRun");prompt.setDefaultButton(cancel);prompt.setEscapeButton(cancel);prompt.exec();
        if(prompt.clickedButton()!=overwrite && prompt.clickedButton()!=keep)return;
        fConfig.existingRunPolicy=prompt.clickedButton()==overwrite?"overwrite":"append";
        replaceCompletedRun=true;
    }
    ApplyAutoRunSettings(fConfig.autoRun);
    emit UpdateRunMetadataSignal(fConfig);
    fWaveformViewerAction->setEnabled(false);
    fDataToolsAction->setEnabled(false);
    fPlot->Clear(); emit StartSignal(replaceCompletedRun,false,fConfig);
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
