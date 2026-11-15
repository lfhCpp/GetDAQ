#include "plantransitionwindow.h"
#include "ui_plantransitionwindow.h"
#include <QMessageBox>
#include <QTableWidgetItem>
#include <stdexcept>

void PlanTransitionWindow::SetCompletionState(bool canComplete,bool canObserve,QByteArray originalRequest,QByteArray originalStatus,QByteArray completionRequest,QString completionPhase,QString reason) {
    fCanComplete=fCanObserveCompletion=false;
    try {
        if(originalRequest!=fSavedOriginalRequest) {fCompletionPending=false;fSavedOriginalStatus.clear();}
        fSavedOriginalRequest=originalRequest;fSavedOriginalStatus=originalStatus;fCompletionRequest=completionRequest;
        if(!originalRequest.isEmpty()) {
            const auto original=GetDAQ::DecodeEccControlRequest(originalRequest.toStdString());
            if(original.command!="retire-data-plans" || original.instanceId!=fIdentity.instanceId || original.snapshotSha256!=fIdentity.snapshotSha256)
                throw std::runtime_error("Saved retirement belongs to another ECC instance");
            if(!originalStatus.isEmpty()) {
                const auto before=GetDAQ::DecodeEccControlSystemStatus(originalStatus.toStdString());
                if(before.instanceId!=original.instanceId || before.snapshotSha256!=original.snapshotSha256 || before.operationActive || before.controlNeedsReconciliation ||
                    before.controller.outcomeUnknown || before.controller.needsReconciliation || before.controller.error || before.controller.transition || before.dataNeedsReconciliation ||
                    (before.controller.state!=1 && before.controller.state!=2) || before.nodes.size()!=original.successorReceiverPlans.size())
                    throw std::runtime_error("Saved pre-retirement status is not the original known sealed cycle");
                ui->TargetsTable->setRowCount(int(original.successorReceiverPlans.size()));
                for(std::size_t row=0;row<original.successorReceiverPlans.size();++row) {
                    const auto &target=original.successorReceiverPlans[row];const GetDAQ::EccControlDataNodeStatus *old=nullptr;
                    for(const auto &node:before.nodes)if(node.agentId==target.agentId)old=&node;
                    if(!old || old->state!="stopped" || old->needsReconciliation || old->receivers.empty())throw std::runtime_error("Saved original data node lacks its normal seal");
                    for(const auto &receiver:old->receivers)if(!receiver.reaped || !receiver.sealVerified || receiver.exitCode!=0)throw std::runtime_error("Saved original receiver is not normally sealed");
                    const QStringList cells={QString::fromStdString(target.agentId),QString::fromStdString(old->planSha256),QString::fromStdString(target.planSha256),QString::fromStdString(target.planFile)};
                    for(int column=0;column<cells.size();++column)ui->TargetsTable->setItem(int(row),column,new QTableWidgetItem(cells[column]));
                }
                ui->Summary->setText(tr("Saved original retirement: %1\nOriginal ECC: %2\nUnchanged successor plan SHA-256: %3\nUnchanged successor scope / epoch: %4 / %5")
                    .arg(QString::fromStdString(original.operationId),QString::fromStdString(original.instanceId),QString::fromStdString(original.successorPlanSha256),QString::fromStdString(original.successorScopeSha256),QString::fromStdString(original.successorEpoch)));
                fCanComplete=canComplete;
            }
            if(!completionRequest.isEmpty()) {
                const auto completion=GetDAQ::DecodeEccControlRequest(completionRequest.toStdString());
                if(completion.command!="complete-data-handoff" || completion.failedOperationId!=original.operationId || completion.instanceId!=original.instanceId ||
                    completion.snapshotSha256!=original.snapshotSha256 || completion.clientId!=original.clientId || completion.clientEpoch!=original.clientEpoch)
                    throw std::runtime_error("Saved completion does not bind the original failed retirement");
                fCanObserveCompletion=canObserve;
                if(completionRequest!=fCompletionClickedAgainst && (completionPhase=="failed" || completionPhase=="complete"))fCompletionPending=false;
                reason+=tr("\nSaved completion: %1 (%2)").arg(QString::fromStdString(completion.operationId),completionPhase);
            }
        }
        ui->CompletionStatus->setText(reason);
    }catch(const std::exception &e){fCanComplete=fCanObserveCompletion=false;fSavedOriginalStatus.clear();ui->CompletionStatus->setText(QString::fromUtf8(e.what()));}
    UpdateControls();
}
void PlanTransitionWindow::Complete() {
    if(!ui->CompletePartial->isEnabled())return;
    const auto originalBytes=fSavedOriginalRequest,statusBytes=fSavedOriginalStatus;
    const auto original=GetDAQ::DecodeEccControlRequest(originalBytes.toStdString());
    if(QMessageBox::question(this,tr("Complete original partial retirement"),
        tr("Complete the saved failed retirement %1 on its original live ECC?\nAlready accepted nodes will only be observed. If an accepted node retirement failed, first restore its exact reviewed plan and explicitly complete that original request on the same live node service. This window does not repair a node.\nOnly explicitly unsent or rejected child requests may be submitted unchanged. The original failure and old-control fence remain. New services and the successor ECC still require explicit startup.")
            .arg(QString::fromStdString(original.operationId)),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
    if(!ui->CompletePartial->isEnabled() || originalBytes!=fSavedOriginalRequest || statusBytes!=fSavedOriginalStatus) {ui->Status->setText(tr("Completion eligibility changed during confirmation; inspect the original state again."));return;}
    fCompletionPending=true;fCompletionClickedAgainst=fCompletionRequest;fReceipt.clear();UpdateControls();
    ui->Status->setText(tr("Saving a separate completion intent, then submitting once. Observe its saved operation if the reply is lost."));
    emit CompleteSignal(QString::fromStdString(original.operationId));
}
