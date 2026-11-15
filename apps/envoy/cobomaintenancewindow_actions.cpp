#include "cobomaintenancewindow.h"
#include "ui_cobomaintenancewindow.h"
#include <QMessageBox>

void CoBoMaintenanceWindow::Configure() {
    if(!ui->Configure->isEnabled())return;
    const auto target=ui->Target->currentText();
    if(QMessageBox::question(this,tr("Configure only the selected CoBo"),tr("Load the original fixed Configure settings onto CoBo %1 (%2)?\nOnly this board is configured. Other boards and MuTaNT are untouched. The system remains Prepared; this does not start a Run.").arg(target,ui->ExpectedName->text()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="configure-cobo";UpdateControls();
    ui->Status->setText(tr("Saving the Configure intent, then submitting once. If the reply is lost, observe the same operation; do not resubmit."));
    emit ConfigureSignal(target,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::Submit() {
    if(!ui->Submit->isEnabled())return;
    const auto target=ui->Target->currentText();
    if(QMessageBox::question(this,tr("Prepare only the selected CoBo"),tr("Reprepare CoBo %1 (%2) using its original fixed settings?\nOnly this board is reinitialized. Other boards and saved system alignment are retained.\nThe system stays Prepared; Configure and Start remain explicit. Failure or a lost reply is held and is not automatically retried.")
        .arg(target,ui->ExpectedName->text()),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;UpdateControls();ui->Status->setText(tr("Saving the original maintenance intent, then submitting once. Keep the original operation if the reply is lost."));
    fSubmittedCommand="reprepare-cobo";
    emit SubmitSignal(target,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::SetLed() {
    if(!ui->SetLed->isEnabled())return;
    const auto target=ui->Target->currentText();const auto asad=ui->LedAsAd->currentData().toUInt();
    const auto type=ui->LedType->currentData().toUInt();const auto state=ui->LedState->currentData().toUInt();
    const QString labels[]={tr("Off"),tr("On"),tr("Slow pulse"),tr("Fast pulse")};
    if(QMessageBox::question(this,tr("Set one CoBo LED"),tr("Set CoBo %1, AsAd %2, LED %3 to %4?\nOnly the selected LED is commanded. The system must remain Prepared with data processes stopped. The reply cannot confirm physical lamp state; a lost reply is not retried.")
        .arg(target).arg(asad).arg(ui->LedType->currentText()).arg(labels[state]),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="set-cobo-led";fSubmittedAsad=asad;fSubmittedLedType=type;fSubmittedLedState=state;
    UpdateControls();ui->Status->setText(tr("Saving the original LED command and submitting once. Keep the original operation if the reply is lost."));
    emit SetLedSignal(target,asad,type,state,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::BlinkLeds() {
    if(!ui->BlinkLeds->isEnabled())return;
    const auto target=ui->Target->currentText();
    if(QMessageBox::question(this,tr("Blink all LEDs once"),tr("Blink every LED on CoBo %1 once, sequentially?\nThis is a one-shot board-manager command. It requires healthy Prepared state and stopped data processes. The acknowledgment cannot verify the physical lamps. A lost reply will not be retried.")
        .arg(target),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="blink-cobo-leds";
    UpdateControls();ui->Status->setText(tr("Saving the original one-shot blink intent and submitting once. Keep the original operation if the reply is lost."));
    emit BlinkLedsSignal(target,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::FlipLed() {
    if(!ui->FlipLed->isEnabled())return;
    const auto target=ui->Target->currentText();const auto ledId=ui->LedFlipTarget->currentData().toUInt();
    const auto selection=ui->LedFlipTarget->currentText();
    if(QMessageBox::question(this,tr("Flip selected LED state"),tr("Invert the current GPIO state for %1 on CoBo %2?\nThis reads and toggles board state, so repeating the command reverses it. It is a single non-idempotent operation, requires healthy Prepared state with data processes stopped, and will never be automatically retried. The acknowledgment cannot verify the physical lamp.")
        .arg(selection,target),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="flip-cobo-led";fSubmittedLedFlipId=ledId;
    UpdateControls();ui->Status->setText(tr("Saving the original non-idempotent LED flip intent and submitting once. If the reply is lost, inspect the same operation; do not repeat it."));
    emit FlipLedSignal(target,ledId,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::PulseLed() {
    if(!ui->PulseLed->isEnabled())return;
    const auto target=ui->Target->currentText();const auto ledId=ui->LedFlipTarget->currentData().toUInt();
    const auto periodMs=static_cast<unsigned int>(ui->LedPulsePeriodMs->value());
    const auto selection=ui->LedFlipTarget->currentText();
    const auto action=periodMs==0 ? tr("stop the pulse generator for") : tr("set the pulse period for");
    if(QMessageBox::question(this,tr("Set selected LED pulse"),tr("%1 %2 on CoBo %3 to %4 ms?\nThis uses the legacy cobo-led-pulse period_ms setting. A period of 0 stops that LED's pulser. The command is sent once, requires healthy Prepared state with data processes stopped, and its acknowledgment cannot verify physical lamp activity. A lost reply will not be retried.")
        .arg(action,selection,target).arg(periodMs),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="pulse-cobo-led";fSubmittedLedPulseId=ledId;fSubmittedLedPulsePeriodMs=periodMs;
    UpdateControls();ui->Status->setText(tr("Saving the original LED pulse intent and submitting once. Keep the original operation if the reply is lost."));
    emit PulseLedSignal(target,ledId,periodMs,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
void CoBoMaintenanceWindow::TriggerPulser() {
    if(!ui->TriggerPulser->isEnabled())return;
    const auto target=ui->Target->currentText();const int voltage=ui->PulserVoltage->value();
    if(QMessageBox::question(this,tr("Trigger one AsAd pulse"),tr("Trigger exactly one pulse on CoBo %1 at configured offset %2 mV?\nThe operation requires healthy Prepared state and stopped data processes. The reply cannot verify the physical pulse. A lost reply will not be retried.")
        .arg(target).arg(voltage),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    fHasOperationResult=false;fSubmitted=true;fSubmittedTarget=target;fSubmittedCommand="trigger-asad-pulser";fSubmittedVoltageMv=voltage;
    UpdateControls();ui->Status->setText(tr("Saving the original single-pulse intent and submitting once. Keep the original operation if the reply is lost."));
    emit PulserSignal(target,voltage,QString::fromStdString(fTargets.instanceId),QString::fromStdString(fTargets.snapshotSha256));
}
