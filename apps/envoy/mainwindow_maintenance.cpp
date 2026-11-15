#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "cobomaintenancewindow.h"
#include "eccprotocol.h"

#include <QPushButton>
#include <QStyle>
#include <QTableWidget>

int MainWindow::SelectedId() const
{
    const int row = ui->StatusTable->currentRow();
    return row < 0 ? -1 : ui->StatusTable->item(row, 0)->data(Qt::UserRole).toInt();
}
QString MainWindow::SelectedMaintenanceTarget() const
{
    const int row=ui->StatusTable->currentRow();
    if(row<0 || row>=fConfig.nodes.size()) return QString();
    const NodeConfig &node=fConfig.nodes[row];
    try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(fCoBoTargets.toStdString());
        QString idMatch,nameMatch;
        int idMatches=0,nameMatches=0;
        for(const auto &target:targets.cobos) {
            const QString id=QString::fromStdString(target.id);
            if(id==QString::number(node.id)) {idMatch=id;++idMatches;}
            if(QString::fromStdString(target.nodeName)==node.name) {nameMatch=id;++nameMatches;}
        }
        if(idMatches==1) return idMatch;
        if(idMatches>1) return QString();
        return nameMatches==1 ? nameMatch : QString();
    } catch (...) { return QString(); }
}
void MainWindow::OpenCoBoMaintenance(const QString &preferredTarget)
{
    if(!fCoBoMaintenance) {
        fCoBoMaintenance=new CoBoMaintenanceWindow(this);fCoBoMaintenance->setObjectName("NativeCoBoMaintenanceWindow");
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::InspectSignal,this,&MainWindow::InspectCoBoMaintenanceTargetsSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::SubmitSignal,this,&MainWindow::ReprepareCoBoSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::ConfigureSignal,this,&MainWindow::ConfigureCoBoSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::SetLedSignal,this,&MainWindow::SetCoBoLedSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::BlinkLedsSignal,this,&MainWindow::BlinkCoBoLedsSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::FlipLedSignal,this,&MainWindow::FlipCoBoLedSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::PulseLedSignal,this,&MainWindow::PulseCoBoLedSignal);
        connect(fCoBoMaintenance,&CoBoMaintenanceWindow::PulserSignal,this,&MainWindow::TriggerAsAdPulserSignal);
    }
    fCoBoMaintenance->SetControlState(fCoBoCanSubmit,fCoBoStatus,fCoBoIdentity,fCoBoReason);
    if(!preferredTarget.isEmpty())fCoBoMaintenance->PreferTarget(preferredTarget);
    if(!fCoBoTargets.isEmpty())fCoBoMaintenance->ShowTargets(fCoBoTargets,{});
    fCoBoMaintenance->show();fCoBoMaintenance->raise();fCoBoMaintenance->activateWindow();
    emit InspectCoBoMaintenanceTargetsSignal();
}
void MainWindow::ShowCoBoMaintenanceStateSlot(bool canSubmit,QByteArray status,QByteArray identity,QString reason)
{
    if(!fConfig.nativeEcc)return;
    fCoBoCanSubmit=canSubmit;fCoBoStatus=status;fCoBoIdentity=identity;fCoBoReason=reason;
    if(fCoBoMaintenance)fCoBoMaintenance->SetControlState(canSubmit,status,identity,reason);
    ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
}
void MainWindow::ShowCoBoMaintenanceTargetsSlot(QByteArray body,QString error)
{
    if(!fConfig.nativeEcc)return;
    fCoBoTargets=error.isEmpty() ? body : QByteArray();
    fSingleCoBoLifecycleTarget.clear();
    if(error.isEmpty())try {
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(body.toStdString());
        int dataSources=0;bool hasMutant=false;QString matchingSource;
        for(const auto &node:fConfig.nodes) {
            if(node.mutant)hasMutant=true;
            else if(node.nativeSenderId==QString::fromStdString(targets.cobos.empty() ? std::string() : targets.cobos.front().id)) {
                ++dataSources;matchingSource=node.nativeSenderId;
            }
        }
        if(targets.cobos.size()==1 && dataSources==1 && !hasMutant)
            fSingleCoBoLifecycleTarget=matchingSource;
    } catch(...) {}
    if(!fSingleCoBoLifecycleTarget.isEmpty()) {
        ui->NodeProgress->setText(tr("Progress selected node"));
        ui->NodeProgress->setToolTip(tr("The fixed ECC has exactly one active CoBo and no MuTaNT. This advances the whole one-CoBo ECC lifecycle; it is not an independently stored board state machine."));
        ui->NodeProgress->setProperty("scope", "single-system");
        ui->NodeProgress->setAccessibleDescription(tr("Advance the lifecycle of the entire fixed ECC system, which contains exactly one CoBo."));
        ui->NodeRegress->setText(tr("Regress selected node"));
        ui->NodeRegress->setToolTip(tr("Regress the whole fixed one-CoBo ECC lifecycle. Hardware actions still require fresh service-side state and confirmation."));
        ui->NodeRegress->setProperty("scope", "single-system");
        ui->NodeRegress->setAccessibleDescription(tr("Regress the lifecycle of the entire fixed ECC system, which contains exactly one CoBo."));
        ui->NodeRegress->setVisible(true);
    } else {
        ui->NodeProgress->setText(tr("Progress selected node"));
        ui->NodeRegress->setText(tr("Regress selected node"));
        ui->NodeRegress->setVisible(true);
        ui->NodeProgress->setToolTip(tr("For a multi-board ECC use Progress all. Individual maintenance is available in the menus."));
        ui->NodeRegress->setToolTip(tr("For a multi-board ECC use Regress all."));
    }
    for (QPushButton *button : {ui->NodeProgress, ui->NodeRegress}) {
        button->style()->unpolish(button);
        button->style()->polish(button);
    }
    if(fCoBoMaintenance)fCoBoMaintenance->ShowTargets(body,error);
    ShowControlSlot(ui->Phase->text(),fConnected,fBusy,fActive);
}
bool MainWindow::SelectedSingleCoBoLifecycleTarget() const
{
    const int selected=SelectedId();
    if(fSingleCoBoLifecycleTarget.isEmpty() || selected<0)return false;
    for(const auto &node:fConfig.nodes)
        if(node.id==selected)return !node.mutant && node.nativeSenderId==fSingleCoBoLifecycleTarget;
    return false;
}
void MainWindow::on_NodeProgress_clicked()
{
    if(fConfig.nativeEcc) {
        if(SelectedSingleCoBoLifecycleTarget())emit ProgressSignal(-1);

    } else if(SelectedId()>=0) emit ProgressSignal(SelectedId());
}
void MainWindow::on_NodeRegress_clicked()
{
    if(fConfig.nativeEcc) {if(SelectedSingleCoBoLifecycleTarget())emit RegressSignal(-1);}
    else if(SelectedId()>=0)emit RegressSignal(SelectedId());
}
