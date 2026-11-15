#ifndef COBOMAINTENANCEWINDOW_H
#define COBOMAINTENANCEWINDOW_H
#include "eccprotocol.h"
#include <QDialog>
namespace Ui {class CoBoMaintenanceWindow;}
// MUSIC形式的目标审阅窗口；仅控制元数据，不接收原始事件。
class CoBoMaintenanceWindow : public QDialog
{
    Q_OBJECT
public:
    explicit CoBoMaintenanceWindow(QWidget *parent=nullptr);
    ~CoBoMaintenanceWindow() override;
public slots:
    void SetControlState(bool canSubmit,QByteArray status,QByteArray identity,QString reason);
    void ShowTargets(QByteArray body,QString error);
    void PreferTarget(QString targetId);
    void ShowResult(QByteArray operation,QString error);
signals:
    void InspectSignal();
    void SubmitSignal(QString coboId,QString instanceId,QString snapshotSha256);
    void ConfigureSignal(QString coboId,QString instanceId,QString snapshotSha256);
    void SetLedSignal(QString coboId,unsigned int asadId,unsigned int ledType,unsigned int ledState,QString instanceId,QString snapshotSha256);
    void BlinkLedsSignal(QString coboId,QString instanceId,QString snapshotSha256);
    void FlipLedSignal(QString coboId,unsigned int ledId,QString instanceId,QString snapshotSha256);
    void PulseLedSignal(QString coboId,unsigned int ledId,unsigned int periodMs,QString instanceId,QString snapshotSha256);
    void PulserSignal(QString coboId,int voltageMv,QString instanceId,QString snapshotSha256);
private:
    Ui::CoBoMaintenanceWindow *ui;
    GetDAQ::EccControlIdentity fIdentity;
    GetDAQ::EccControlMaintenanceTargets fTargets;
    bool fEligible=false,fReviewed=false,fSubmitted=false,fHasOperationResult=false;
    QString fSubmittedTarget;
    QString fSubmittedCommand;
    QString fPreferredTarget;
    unsigned int fSubmittedAsad=0,fSubmittedLedType=0,fSubmittedLedState=0,fSubmittedLedFlipId=0,fSubmittedLedPulseId=0,fSubmittedLedPulsePeriodMs=0;
    int fSubmittedVoltageMv=0;
    void UpdateControls();
    void Submit();
    void Configure();
    void SetLed();
    void BlinkLeds();
    void FlipLed();
    void PulseLed();
    void TriggerPulser();
};
#endif
