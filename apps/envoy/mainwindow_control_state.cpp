#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "consolepresentation.h"
#include "autorunwidget.h"
#include <QAction>
#include <QPushButton>
#include <QStyle>

void MainWindow::ShowControlSlot(QString phase, bool connected, bool busy, bool active)
{
    if (connected && !fConnected && !fLogStore.HasConnectionLog()) StartConnectionLog();
    else if (!connected && fConnected) EndConnectionLog(tr("Connection closed"));
    if ((phase=="Running") && !fDisplayClock.isValid()) {fDisplayClock.start();ui->Elapsed->setText(ConsolePresentation::ElapsedText(0));}
    if ((phase!="Running") && fDisplayClock.isValid()) fDisplayClock.invalidate();
    fConnected = connected; fBusy = busy; fActive = active;
    for(auto *controls:findChildren<AutoRunWidget *>())controls->SetLocked(active || fAutomaticArmed);
    ui->Phase->setText(phase);
    const bool phaseFailed = phase.contains(QStringLiteral("error"), Qt::CaseInsensitive) ||
        phase.contains(QStringLiteral("failed"), Qt::CaseInsensitive) ||
        phase.contains(QStringLiteral("inspection"), Qt::CaseInsensitive);
    if(ui->ConnectionStatusBadge->property("connected").toBool()!=connected) {
        ui->ConnectionStatusBadge->setProperty("connected",connected);
        ui->ConnectionStatusBadge->style()->unpolish(ui->ConnectionStatusBadge);
        ui->ConnectionStatusBadge->style()->polish(ui->ConnectionStatusBadge);
    }
    const QString phaseState = phaseFailed ? QStringLiteral("error") : busy ? QStringLiteral("busy") : active ? QStringLiteral("running") : QStringLiteral("idle");
    for (QWidget *widget : QList<QWidget *>{ui->ConnectionStatusBadge, ui->Phase}) {
        if (widget->property("state").toString() == phaseState) continue;
        widget->setProperty("state", phaseState);
        widget->style()->unpolish(widget);
        widget->style()->polish(widget);
    }
    ui->Mode->setText("HARDWARE / SOAP");
    const bool edit = !connected && !busy && !active;
    const bool runMetadataEditable = !active;
    ui->Experiment->setEnabled(edit);
    for (QWidget *widget : QList<QWidget *>{ui->RunNumber, ui->Description, ui->Fields, ui->AddField, ui->RemoveField})
        widget->setEnabled(runMetadataEditable);
    for (QWidget *widget : QList<QWidget *>{ui->LoadConfig, ui->SaveConfig, ui->Nodes}) widget->setEnabled(edit);
    ui->Connect->setEnabled(edit);
    const bool stoppedPending=(phase=="Stopped; finalization pending" || phase=="Stopped; catalog inspection required");
    ui->Disconnect->setEnabled(connected && !busy && (!active || stoppedPending) && !fRemoteRunning);
    ui->Disconnect->setToolTip(stoppedPending
        ? tr("Disconnect from stopped ECC and retain the unfinished Run journal. Background services keep running.")
        : tr("Disconnect from ECC without stopping background services."));
    for (QPushButton *button : {ui->Progress, ui->Regress, ui->NodeProgress, ui->NodeRegress, ui->Recover}) button->setEnabled(connected && !busy && !active);
    ui->StartRun->setEnabled(connected && !busy && !active);
    ui->StopRun->setEnabled(connected && !busy && (active || fAutomaticArmed));
    ui->Finalize->setEnabled(connected && !busy && active);
    fPause->setEnabled(connected && !busy && active && phase=="Running");
    fResume->setEnabled(connected && !busy && active && phase=="Paused");
    RefreshOfflineToolsGate();
}
