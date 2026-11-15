#include "cobomaintenancewindow.h"
#include "ui_cobomaintenancewindow.h"
#include <stdexcept>
#include <QFormLayout>
#include <QHBoxLayout>
CoBoMaintenanceWindow::CoBoMaintenanceWindow(QWidget *parent)
    : QDialog(parent),ui(new Ui::CoBoMaintenanceWindow)
{
    ui->setupUi(this);
    setMinimumSize(1000, 520);
    setStyleSheet(QStringLiteral(
        "QLabel#Instructions { color:#18324f; font-size:17px; font-weight:700; padding:14px 16px; background:#e8f0fa; border:1px solid #d5e2f0; border-radius:8px; }"
        "QLabel#ControlState { color:#52677e; padding:10px 12px; background:#ffffff; border:1px solid #d9e1ea; border-left:4px solid #8295a9; border-radius:5px; }"
        "QLabel#Summary { color:#324b65; font-family:monospace; font-size:11px; padding:10px 12px; background:#f8fafc; border:1px solid #d9e1ea; border-radius:5px; }"
        "QLabel#Status { color:#52677e; padding:10px 12px; background:#ffffff; border:1px solid #d9e1ea; border-radius:5px; }"
        "QLabel#ExpectedName { color:#18324f; font-weight:700; padding:5px 0; }"
        "QLabel#LifecycleState { color:#18324f; font-weight:700; padding:5px 0; }"
        "QFormLayout QLabel { color:#52677e; }"
        "QComboBox, QSpinBox { min-height:24px; padding:4px 7px; }"
        "QPushButton { min-height:30px; padding:6px 10px; }"
        "QPushButton#Submit { background:#258568; color:white; border-color:#258568; font-weight:700; }"
        "QPushButton#Submit:hover { background:#1d7057; }"
        "QPushButton#TriggerPulser { background:#fff7e8; border-color:#e5c890; color:#765319; }"
        "QPushButton#TriggerPulser:hover { background:#ffefcf; }"));
    if (auto *form = qobject_cast<QFormLayout *>(ui->targetLayout)) form->setVerticalSpacing(9);
    if (auto *buttons = qobject_cast<QHBoxLayout *>(ui->actionsLayout)) buttons->setSpacing(8);
    for(unsigned int id=0;id<4;++id)ui->LedAsAd->addItem(QString::number(id),id);
    const char *types[]={"P","B","U","D"};for(unsigned int type=0;type<4;++type)ui->LedType->addItem(QString::fromLatin1(types[type]),type);
    const char *states[]={"Off","On","Slow pulse","Fast pulse"};for(unsigned int state=0;state<4;++state)ui->LedState->addItem(tr(states[state]),state);
    const char *flipTypes[]={"P","B","U","D"};for(unsigned int asad=0;asad<4;++asad)for(unsigned int type=0;type<4;++type)ui->LedFlipTarget->addItem(tr("%1 (AsAd %2)").arg(QString::fromLatin1(flipTypes[type])).arg(asad),4*asad+type);
    ui->LedFlipTarget->addItem(tr("All LEDs"),16);
    connect(ui->Inspect,&QPushButton::clicked,this,[this] {emit InspectSignal();});
    connect(ui->Submit,&QPushButton::clicked,this,&CoBoMaintenanceWindow::Submit);
    connect(ui->Configure,&QPushButton::clicked,this,&CoBoMaintenanceWindow::Configure);
    connect(ui->SetLed,&QPushButton::clicked,this,&CoBoMaintenanceWindow::SetLed);
    connect(ui->BlinkLeds,&QPushButton::clicked,this,&CoBoMaintenanceWindow::BlinkLeds);
    connect(ui->FlipLed,&QPushButton::clicked,this,&CoBoMaintenanceWindow::FlipLed);
    connect(ui->PulseLed,&QPushButton::clicked,this,&CoBoMaintenanceWindow::PulseLed);
    connect(ui->TriggerPulser,&QPushButton::clicked,this,&CoBoMaintenanceWindow::TriggerPulser);
    connect(ui->Target,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this](int) {UpdateControls();});
    UpdateControls();
}
CoBoMaintenanceWindow::~CoBoMaintenanceWindow() {delete ui;}
void CoBoMaintenanceWindow::UpdateControls() {
    const bool bound=fReviewed && fTargets.instanceId==fIdentity.instanceId && fTargets.snapshotSha256==fIdentity.snapshotSha256;
    ui->Submit->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0);
    ui->Configure->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0);
    ui->SetLed->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0);
    ui->BlinkLeds->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0);
    ui->FlipLed->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0 && ui->LedFlipTarget->currentIndex()>=0);
    ui->PulseLed->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0 && ui->LedFlipTarget->currentIndex()>=0);
    ui->LedFlipTarget->setEnabled(!fSubmitted);
    ui->LedPulsePeriodMs->setEnabled(!fSubmitted);
    ui->TriggerPulser->setEnabled(fEligible && bound && !fSubmitted && ui->Target->currentIndex()>=0);
    ui->Target->setEnabled(!fSubmitted);
    if(ui->Target->currentIndex()>=0 && ui->Target->currentIndex()<int(fTargets.cobos.size()))
    {
        const auto &target=fTargets.cobos[std::size_t(ui->Target->currentIndex())];
        ui->ExpectedName->setText(QString::fromStdString(target.nodeName));
        const QMap<QString,QString> labels={{"uninitialized",tr("Uninitialized")},{"described",tr("Described")},{"prepared",tr("Prepared")},
            {"configured",tr("Configured")},{"running",tr("Running")},{"paused",tr("Paused")},{"transitioning",tr("Transitioning")},{"unknown",tr("UNKNOWN — reconciliation required")}};
        QString state=labels.value(QString::fromStdString(target.lifecycleState),tr("UNKNOWN — unsupported state"));
        if(!target.operationId.empty())state+=tr("\nLast operation: %1 · %2 · %3").arg(QString::fromStdString(target.operation),QString::fromStdString(target.operationPhase),QString::fromStdString(target.operationId));
        ui->LifecycleState->setText(state);
        ui->LifecycleState->setToolTip(tr("ECC journal projection for this CoBo; it is not a live hardware readback. After service restart or an uncertain operation, the state is UNKNOWN until reconciled."));
    }
    else {ui->ExpectedName->clear();ui->LifecycleState->setText(tr("UNKNOWN"));}
}
void CoBoMaintenanceWindow::SetControlState(bool canSubmit,QByteArray status,QByteArray identity,QString reason) {
    fEligible=false;
    try {
        if(identity.isEmpty()) {fIdentity=GetDAQ::EccControlIdentity();fReviewed=false;fTargets=GetDAQ::EccControlMaintenanceTargets();ui->Target->clear();fSubmitted=false;fSubmittedTarget.clear();fSubmittedCommand.clear();ui->Summary->setText(tr("No fixed targets inspected."));}
        else {
            const auto next=GetDAQ::DecodeEccControlIdentity(identity.toStdString());
            if(next.instanceId!=fIdentity.instanceId || next.snapshotSha256!=fIdentity.snapshotSha256) {fReviewed=false;ui->Target->clear();fSubmitted=false;fSubmittedTarget.clear();fSubmittedCommand.clear();ui->Summary->setText(tr("No fixed targets inspected."));}
            fIdentity=next;
        }
        if(!status.isEmpty()) {
            const auto current=GetDAQ::DecodeEccControlSystemStatus(status.toStdString());
            fEligible=canSubmit && current.instanceId==fIdentity.instanceId && current.snapshotSha256==fIdentity.snapshotSha256 && current.controller.state==3 &&
                !current.controller.transition && !current.controller.error && !current.controller.outcomeUnknown && !current.controller.needsReconciliation &&
                !current.controlNeedsReconciliation && !current.dataNeedsReconciliation && !current.operationActive;
        }
        ui->ControlState->setText(reason);
    }catch(const std::exception &e){ui->ControlState->setText(QString::fromUtf8(e.what()));}
    UpdateControls();
}
void CoBoMaintenanceWindow::ShowTargets(QByteArray body,QString error) {
    const bool preserveStatus=fSubmitted || fHasOperationResult;
    fReviewed=false;ui->Target->clear();
    try {
        if(!error.isEmpty())throw std::runtime_error(error.toStdString());
        const auto targets=GetDAQ::DecodeEccControlMaintenanceTargets(body.toStdString());
        if(targets.instanceId!=fIdentity.instanceId || targets.snapshotSha256!=fIdentity.snapshotSha256 || targets.configurationSha256!=fIdentity.configurationSha256 || targets.artifactSha256!=fIdentity.artifactSha256)
            throw std::runtime_error("Maintenance targets differ from the current ECC identity");
        fTargets=targets;
        for(const auto &target:targets.cobos)ui->Target->addItem(QString::fromStdString(target.id));
        const int preferredIndex=ui->Target->findText(fPreferredTarget);
        if(preferredIndex>=0)ui->Target->setCurrentIndex(preferredIndex);
        fPreferredTarget.clear();
        fReviewed=true;
        ui->Summary->setText(tr("Original ECC: %1\nSnapshot: %2\nConfiguration: %3\nArtifact: %4")
            .arg(QString::fromStdString(targets.instanceId),QString::fromStdString(targets.snapshotSha256),QString::fromStdString(targets.configurationSha256),QString::fromStdString(targets.artifactSha256)));
        if(!preserveStatus)ui->Status->setText(tr("Fixed active targets loaded. Review the target ID and expected board name before confirming."));
    }catch(const std::exception &e){ui->Status->setText(QString::fromUtf8(e.what()));}
    UpdateControls();
}
void CoBoMaintenanceWindow::PreferTarget(QString targetId) {
    fPreferredTarget=targetId;
    const int index=ui->Target->findText(targetId);
    if(index>=0) {ui->Target->setCurrentIndex(index);fPreferredTarget.clear();}
}
void CoBoMaintenanceWindow::ShowResult(QByteArray body,QString error) {
    if(!error.isEmpty()){fHasOperationResult=true;ui->Status->setText(error);UpdateControls();emit InspectSignal();return;}
    try {
        const auto operation=GetDAQ::DecodeEccControlOperation(body.toStdString());
        if((operation.request.command!="reprepare-cobo" && operation.request.command!="configure-cobo" && operation.request.command!="set-cobo-led" && operation.request.command!="trigger-asad-pulser" && operation.request.command!="blink-cobo-leds" && operation.request.command!="flip-cobo-led" && operation.request.command!="pulse-cobo-led") || operation.request.instanceId!=fIdentity.instanceId || operation.request.snapshotSha256!=fIdentity.snapshotSha256 ||
           (!fSubmittedTarget.isEmpty() && operation.request.coboId!=fSubmittedTarget.toStdString()))throw std::runtime_error("Maintenance result differs from the original reviewed target/instance");
        if(!fSubmittedCommand.isEmpty() && operation.request.command!=fSubmittedCommand.toStdString())throw std::runtime_error("Maintenance result command differs from the submitted action");
        if(operation.request.command=="set-cobo-led" && (operation.request.ledAsadId!=fSubmittedAsad || operation.request.ledType!=fSubmittedLedType || operation.request.ledState!=fSubmittedLedState))throw std::runtime_error("LED result differs from the exact submitted selection");
        if(!fSubmittedCommand.isEmpty() && operation.request.command=="flip-cobo-led" && operation.request.ledFlipId!=fSubmittedLedFlipId)throw std::runtime_error("LED flip result differs from the exact submitted bit selection");
        if(!fSubmittedCommand.isEmpty() && operation.request.command=="pulse-cobo-led" && (operation.request.ledPulseId!=fSubmittedLedPulseId || operation.request.ledPulsePeriodMs!=fSubmittedLedPulsePeriodMs))throw std::runtime_error("LED pulse result differs from the exact submitted LED or period");
        if(!fSubmittedCommand.isEmpty() && operation.request.command=="trigger-asad-pulser" && operation.request.pulserVoltageMv!=fSubmittedVoltageMv)throw std::runtime_error("Pulser result differs from the exact submitted voltage");
        const auto state=GetDAQ::DecodeEccControlControllerStatus(operation.resultJson);
        if(operation.phase!="complete" || state.state!=3 || state.transition || state.error || state.outcomeUnknown || state.needsReconciliation)
            throw std::runtime_error("Original maintenance is not confirmed complete in known Prepared");
        const bool lifecycleChanged=operation.request.command=="reprepare-cobo" || operation.request.command=="configure-cobo";
        fSubmitted=false;fSubmittedTarget.clear();fSubmittedCommand.clear();fHasOperationResult=true;
        ui->Status->setText(operation.request.command=="set-cobo-led" ? tr("LED command acknowledged for CoBo %1. Physical lamp state is not verified; the system remains Prepared.").arg(QString::fromStdString(operation.request.coboId)) :
            operation.request.command=="blink-cobo-leds" ? tr("Sequential one-shot LED blink acknowledged for CoBo %1. Physical lamp activity is not verified; the system remains Prepared.").arg(QString::fromStdString(operation.request.coboId)) :
            operation.request.command=="flip-cobo-led" ? tr("LED state inversion acknowledged for CoBo %1 (GPIO bit %2). Physical lamp state is not verified; the system remains Prepared.").arg(QString::fromStdString(operation.request.coboId)).arg(operation.request.ledFlipId==16 ? tr("all") : QString::number(operation.request.ledFlipId)) :
            operation.request.command=="pulse-cobo-led" ? (operation.request.ledPulsePeriodMs==0 ? tr("LED pulser stop acknowledged for CoBo %1 (GPIO bit %2). Physical lamp state is not verified; the system remains Prepared.").arg(QString::fromStdString(operation.request.coboId)).arg(operation.request.ledPulseId==16 ? tr("all") : QString::number(operation.request.ledPulseId)) : tr("LED pulse period %1 ms acknowledged for CoBo %2 (GPIO bit %3). Physical lamp activity is not verified; the system remains Prepared.").arg(operation.request.ledPulsePeriodMs).arg(QString::fromStdString(operation.request.coboId)).arg(operation.request.ledPulseId==16 ? tr("all") : QString::number(operation.request.ledPulseId))) :
            operation.request.command=="trigger-asad-pulser" ? tr("One pulser trigger acknowledged for CoBo %1 at %2 mV offset. Physical pulse is not verified; the system remains Prepared.").arg(QString::fromStdString(operation.request.coboId)).arg(operation.request.pulserVoltageMv) :
            operation.request.command=="configure-cobo" ? tr("CoBo %1 Configure completed. The system remains Prepared; explicitly Run start when ready.").arg(QString::fromStdString(operation.request.coboId)) :
            tr("CoBo %1 maintenance completed. System remains Prepared. Inspect diagnostics, then explicitly Configure before the next Run.").arg(QString::fromStdString(operation.request.coboId)));
        if(lifecycleChanged)emit InspectSignal();
    }catch(const std::exception &e){ui->Status->setText(QString::fromUtf8(e.what()));}
    UpdateControls();
}
