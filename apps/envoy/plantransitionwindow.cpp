#include "plantransitionwindow.h"
#include "ui_plantransitionwindow.h"
#include <QFile>
#include <QFileInfo>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QSaveFile>
#include <stdexcept>
namespace {
QByteArray ReadMetadata(const QString &path,qint64 maximum) {
    const QFileInfo info(path);QFile file(path);
    if(!info.isFile() || info.isSymLink() || info.size()>maximum || !file.open(QIODevice::ReadOnly))throw std::runtime_error("Cannot read bounded local review metadata file");
    const auto bytes=file.read(maximum+1);
    if(bytes.size()>maximum || file.error()!=QFile::NoError)throw std::runtime_error("Review metadata read failed or exceeded its limit");
    return bytes;
}
}
PlanTransitionWindow::PlanTransitionWindow(QWidget *parent) : QDialog(parent),ui(new Ui::PlanTransitionWindow)
{
    ui->setupUi(this);ui->TargetsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);ui->TargetsTable->horizontalHeader()->setStretchLastSection(true);
    connect(ui->BrowseSuccessor,&QPushButton::clicked,this,[this] {const auto path=QFileDialog::getOpenFileName(this,tr("Saved successor identity"),{},tr("JSON (*.json)"));if(!path.isEmpty())ui->SuccessorFile->setText(path);});
    connect(ui->BrowseTargets,&QPushButton::clicked,this,[this] {const auto path=QFileDialog::getOpenFileName(this,tr("Reviewed data node targets"),{},tr("JSON (*.json)"));if(!path.isEmpty())ui->TargetsFile->setText(path);});
    const auto changed=[this] {fReviewed=false;UpdateControls();};
    connect(ui->SuccessorFile,&QLineEdit::textChanged,this,changed);connect(ui->TargetsFile,&QLineEdit::textChanged,this,changed);
    connect(ui->Review,&QPushButton::clicked,this,[this] {QString error;if(!LoadReview(ui->SuccessorFile->text(),ui->TargetsFile->text(),&error))ui->Status->setText(error);});
    connect(ui->Submit,&QPushButton::clicked,this,&PlanTransitionWindow::Submit);
    connect(ui->CompletePartial,&QPushButton::clicked,this,&PlanTransitionWindow::Complete);
    connect(ui->InspectCompletion,&QPushButton::clicked,this,[this] {if(ui->InspectCompletion->isEnabled())emit InspectCompletionSignal();});
    connect(ui->InspectReceipt,&QPushButton::clicked,this,[this] {if(fCanInspect)emit InspectSignal();});
    connect(ui->ExportReceipt,&QPushButton::clicked,this,[this] {
        const auto path=QFileDialog::getSaveFileName(this,tr("Export original retirement receipt"),"retirement-receipt.json",tr("JSON (*.json)"));
        if(path.isEmpty())return;
        QString error;if(!SaveReceipt(path,&error))ui->Status->setText(error);else ui->Status->setText(tr("Original receipt exported to %1. Successor startup still verifies all data nodes.").arg(path));
    });UpdateControls();
}
PlanTransitionWindow::~PlanTransitionWindow() {delete ui;}
void PlanTransitionWindow::UpdateControls() {
    ui->Submit->setEnabled(fCanSubmit && fReviewed && !fSubmitted && fReviewedInstance==QString::fromStdString(fIdentity.instanceId));
    ui->Review->setEnabled(!fSubmitted || fCanSubmit);ui->SuccessorFile->setEnabled(!fSubmitted || fCanSubmit);ui->TargetsFile->setEnabled(!fSubmitted || fCanSubmit);
    ui->BrowseSuccessor->setEnabled(ui->SuccessorFile->isEnabled());ui->BrowseTargets->setEnabled(ui->TargetsFile->isEnabled());
    ui->InspectReceipt->setEnabled(fCanInspect);ui->ExportReceipt->setEnabled(!fReceipt.isEmpty());
    ui->CompletePartial->setEnabled(fCanComplete && !fCompletionPending && !fSavedOriginalStatus.isEmpty() && !fSavedOriginalRequest.isEmpty());
    ui->InspectCompletion->setEnabled(fCanObserveCompletion && !fCompletionRequest.isEmpty());
}
void PlanTransitionWindow::SetControlState(bool canSubmit,bool canInspect,QByteArray status,QByteArray identity,QString reason) {
    fCanSubmit=canSubmit;fCanInspect=canInspect;
    try {
        if(!identity.isEmpty()) {
            const auto next=GetDAQ::DecodeEccControlIdentity(identity.toStdString());
            if(next.instanceId!=fIdentity.instanceId) {fReceipt.clear();fCanComplete=fCanObserveCompletion=false;fSavedOriginalRequest.clear();fSavedOriginalStatus.clear();fCompletionRequest.clear();fCompletionPending=false;}
            fIdentity=next;
        }
        if(!status.isEmpty())fOriginal=GetDAQ::DecodeEccControlSystemStatus(status.toStdString());
        if(fOriginal.instanceId!=fIdentity.instanceId || fOriginal.snapshotSha256!=fIdentity.snapshotSha256)fCanSubmit=fCanInspect=false;
        if(fOriginal.instanceId!=fIdentity.instanceId || fOriginal.snapshotSha256!=fIdentity.snapshotSha256 || fOriginal.operationActive ||
            (fOriginal.controller.state!=1 && fOriginal.controller.state!=2) || fOriginal.controller.outcomeUnknown || fOriginal.controller.error || fOriginal.controller.transition ||
            !fOriginal.controlNeedsReconciliation || !fOriginal.dataNeedsReconciliation)fReceipt.clear();
        if(fReviewed && fReviewedInstance!=QString::fromStdString(fIdentity.instanceId))fReviewed=false;
        ui->ControlState->setText(reason);
    } catch(const std::exception &e) {fCanSubmit=fCanInspect=false;ui->ControlState->setText(QString::fromUtf8(e.what()));}
    UpdateControls();
}
bool PlanTransitionWindow::LoadReview(const QString &successorFile,const QString &targetsFile,QString *error) {
    try {
        const auto successor=ReadMetadata(successorFile,65536),targets=ReadMetadata(targetsFile,8192);
        const auto review=GetDAQ::ReviewEccPlanTransition(successor.toStdString(),targets.toStdString());
        if(fIdentity.instanceId.empty() || fOriginal.instanceId!=fIdentity.instanceId || fOriginal.snapshotSha256!=fIdentity.snapshotSha256)throw std::runtime_error("Connect to and inspect the original ECC before reviewing a transition");
        if(review.targets.size()!=fOriginal.nodes.size())throw std::runtime_error("Reviewed targets must cover every original data node");
        if(review.successor.scopeSha256==fOriginal.dataScopeSha256 || review.successor.epoch==fOriginal.dataEpoch)throw std::runtime_error("Reviewed successor requires different data scope and epoch");
        ui->TargetsTable->setRowCount(int(review.targets.size()));
        for(std::size_t row=0;row<review.targets.size();++row) {
            const auto &target=review.targets[row];const GetDAQ::EccControlDataNodeStatus *old=nullptr;
            for(const auto &node:fOriginal.nodes)if(node.agentId==target.agentId)old=&node;
            if(!old || old->planSha256==target.planSha256)throw std::runtime_error("Review contains an unknown node or an unchanged receiver plan");
            const QStringList cells={QString::fromStdString(target.agentId),QString::fromStdString(old->planSha256),QString::fromStdString(target.planSha256),QString::fromStdString(target.planFile)};
            for(int column=0;column<cells.size();++column)ui->TargetsTable->setItem(int(row),column,new QTableWidgetItem(cells[column]));
        }
        fReview=review;fSuccessorFile=successorFile;fTargetsFile=targetsFile;fReviewedInstance=QString::fromStdString(fIdentity.instanceId);fReviewed=true;fSubmitted=false;
        ui->SuccessorFile->setText(successorFile);ui->TargetsFile->setText(targetsFile);fReviewed=true;
        ui->Summary->setText(tr("Original ECC: %1\nSuccessor snapshot: %2\nSuccessor plan SHA-256: %3\nPlan file on ECC host: %4\nNew audit root: %5")
            .arg(fReviewedInstance,QString::fromStdString(review.successor.snapshotSha256),QString::fromStdString(review.successorSha256),QString::fromStdString(review.successor.planFile),QString::fromStdString(review.successor.auditRoot)));
        ui->Status->setText(tr("Review loaded. The ECC service verifies its own saved identity file and holds it during retirement. Local files are review copies."));UpdateControls();return true;
    } catch(const std::exception &e) {fReviewed=false;UpdateControls();if(error)*error=QString::fromUtf8(e.what());return false;}
}
void PlanTransitionWindow::Submit() {
    if(!ui->Submit->isEnabled())return;
    try {
        if(ReadMetadata(fSuccessorFile,65536).toStdString()!=fReview.successorBytes || ReadMetadata(fTargetsFile,8192).toStdString()!=fReview.targetsBytes)throw std::runtime_error("Review files changed; reload and inspect the new review");
        if(QMessageBox::question(this,tr("Retire original receiver plans"),tr("Retire the reviewed plans on all original data nodes?\nThe original ECC will stop accepting new control commands. Partial failure is held and is not automatically retried or rolled back.\nNew services and the successor ECC must be started explicitly."),QMessageBox::Yes|QMessageBox::Cancel,QMessageBox::Cancel)!=QMessageBox::Yes)return;
        fSubmitted=true;UpdateControls();ui->Status->setText(tr("Original retirement intent is being saved and submitted once. Inspect the same operation if the reply is lost."));
        emit SubmitSignal(QByteArray::fromStdString(fReview.successorBytes),QByteArray::fromStdString(fReview.targetsBytes));
    } catch(const std::exception &e) {fReviewed=false;UpdateControls();ui->Status->setText(QString::fromUtf8(e.what()));}
}
void PlanTransitionWindow::ShowResult(QByteArray receipt,QByteArray request,QString error) {
    if(!error.isEmpty()) {fReceipt.clear();ui->Status->setText(error);UpdateControls();return;}
    try {
        const auto original=GetDAQ::DecodeEccControlRequest(request.toStdString());
        if(fOriginal.instanceId!=original.instanceId || fIdentity.instanceId!=original.instanceId || fOriginal.snapshotSha256!=original.snapshotSha256 ||
            fIdentity.snapshotSha256!=original.snapshotSha256 || fOriginal.operationActive || !fOriginal.controlNeedsReconciliation || !fOriginal.dataNeedsReconciliation ||
            (fOriginal.controller.state!=1 && fOriginal.controller.state!=2) || fOriginal.controller.outcomeUnknown || fOriginal.controller.error || fOriginal.controller.transition)
            throw std::runtime_error("A saved normal cycle cannot override a current ECC instance/state or hardware fault");
        const auto status=!fSavedOriginalStatus.isEmpty() && request==fSavedOriginalRequest ? GetDAQ::DecodeEccControlSystemStatus(fSavedOriginalStatus.toStdString()) : fOriginal;
        GetDAQ::ValidateEccPlanRetirementReceipt(receipt.toStdString(),original,status);
        fReceipt=receipt;ui->Status->setText(tr("Original receipt matched. Export it before stopping the original ECC. Successor startup rechecks each new data service."));UpdateControls();
    } catch(const std::exception &e) {fReceipt.clear();UpdateControls();ui->Status->setText(QString::fromUtf8(e.what()));}
}
bool PlanTransitionWindow::SaveReceipt(const QString &file,QString *error) {
    if(fReceipt.isEmpty()) {if(error)*error=tr("No checked original receipt is available");return false;}
    QSaveFile output(file);
    if(!output.open(QIODevice::WriteOnly) || !output.setPermissions(QFile::ReadOwner|QFile::WriteOwner) || output.write(fReceipt)!=fReceipt.size() || !output.commit()) {
        if(error)*error=tr("Cannot save the complete original receipt");
        return false;
    }
    return true;
}
