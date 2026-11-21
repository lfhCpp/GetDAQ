#include <QTimer>
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "autorunwidget.h"
#include "ipwindow.h"
#include "applicationinstance.h"
#include "consolepresentation.h"

#include <QDialog>
#include <QFileDialog>
#include <QTableWidget>

bool MainWindow::ReadConfig()
{
    RunConfig config = fConfig;
    config.autoRun=fAutoRunWidget->Settings();
    config.experiment = ui->Experiment->text().trimmed(); config.runNumber = ui->RunNumber->value();
    config.description = ui->Description->text(); config.fields.clear();
    for (int row = 0; row < ui->Fields->rowCount(); ++row) {
        const QString name = ui->Fields->item(row, 0) ? ui->Fields->item(row, 0)->text().trimmed() : QString();
        if (name.isEmpty() || config.fields.contains(name)) { ShowLogSlot("Field names must be nonempty and unique."); return false; }
        config.fields[name] = ui->Fields->item(row, 1) ? ui->Fields->item(row, 1)->text() : QString();
    }
    QString error;
    if (!config.Validate(&error)) { ShowLogSlot(error); return false; }
    fConfig = config; return true;
}

bool MainWindow::LoadConfig(const QString &path)
{
    RunConfig config; QString error;
    if (!config.Load(path, &error)) { ShowLogSlot("Load failed: " + error); return false; }
    if(!AdoptConfiguration(config))return false;
    ShowLogSlot("Loaded " + path); return true;
}

void MainWindow::on_LoadConfig_clicked()
{
    const QString path = QFileDialog::getOpenFileName(this, tr("Open envoy YAML"), QString(), "YAML (*.yml *.yaml)");
    if (!path.isEmpty() && LoadConfig(path))RememberStartupConfiguration();
}

void MainWindow::on_SaveConfig_clicked()
{
    if (!ReadConfig()) return;
    const QString path = QFileDialog::getSaveFileName(this, tr("Save configuration"), fConfig.filePath, "YAML (*.yml *.yaml)");
    if (path.isEmpty()) return;
    QString error;
    if (!fConfig.Save(path, &error)) ShowLogSlot("Save failed: " + error);
    else { auto saved=fConfig;saved.filePath=path;if(AdoptConfiguration(saved))RememberStartupConfiguration();ui->ConfigPath->clear();ShowLogSlot("Saved " + path); }
}

void MainWindow::on_Nodes_clicked()
{
    if (!ReadConfig()) return;
    if(fConnected || fBusy){ShowLogSlot(tr("Disconnect before changing ECC connection settings."));return;}
    IPWindow window(fConfig, this, fLastNodeStatus);
    if (window.exec() == QDialog::Accepted){if(AdoptConfiguration(window.GetConfig())){RememberStartupConfiguration();ShowLogSlot(tr("Nodes / Paths saved: %1").arg(fConfig.filePath));if(window.property("BackendServicesReady").toBool())QTimer::singleShot(0,this,[this]{if(!fConnected && !fBusy)on_Connect_clicked();});}}
}

void MainWindow::on_AddField_clicked()
{
    int row = ui->Fields->rowCount(); ui->Fields->insertRow(row);
    ui->Fields->setRowHeight(row,ConsolePresentation::ParameterRowHeight);
    ui->Fields->setItem(row, 0, new QTableWidgetItem(QString("Parameter %1").arg(row + 1)));
    ui->Fields->setItem(row, 1, new QTableWidgetItem());
}

void MainWindow::on_RemoveField_clicked() { if (ui->Fields->currentRow() >= 0) ui->Fields->removeRow(ui->Fields->currentRow()); }

bool MainWindow::AdoptConfiguration(const RunConfig &config) {
    if(fApplicationInstance && !fApplicationInstance->Acquire(config.filePath)) {
        ShowLogSlot(fApplicationInstance->Error().isEmpty()?tr("This configuration is already open; its window was activated. Current configuration retained."):fApplicationInstance->Error());return false;
    }
    if(fConfig.filePath!=config.filePath)fNotifications.clear();
    SetConfig(config);
    if(!config.filePath.isEmpty())setWindowTitle(tr("GetDAQ — %1 / %2").arg(QFileInfo(config.filePath).absoluteDir().dirName(),QFileInfo(config.filePath).fileName()));
    return true;
}

