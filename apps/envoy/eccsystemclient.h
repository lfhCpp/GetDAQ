#ifndef ECCSYSTEMCLIENT_H
#define ECCSYSTEMCLIENT_H
#include "eccprotocol.h"
#include "scanruntimeidentity.h"
#include "httpclient.h"
#include <QUrl>
#include <QElapsedTimer>
struct EccSystemClientOptions
{
    QString address, credentialFile, serviceId, clientId, clientEpoch;
    QString snapshotSha256, inputBindingSha256;
    int port = 0;
    int timeoutMs = 2000;
    int statusMaxAgeMs = 10000;
    bool alarmStatusEnabled = false;
    bool configurationStatusEnabled = false;
    bool receiverStatusEnabled = false;
    bool sourceStatusEnabled = false;
    bool receiverFailureStatusEnabled = false;
};
// 整系统客户端：显式提交只调用一次网络API，由服务端操作ID去重。
// 调用方先持久保存意图，失联后只读观察原ID，不自动换ID重发。
// 这是GUI适配基础；没有逐板命令循环、默认成功存储或原始帧接口。
class EccSystemClient : public QObject
{
    Q_OBJECT
public:
    typedef std::function<bool(const std::string &, QString *)> IntentWriter;
    EccSystemClient(const EccSystemClientOptions &options, IntentWriter writer, QObject *parent = nullptr,
                    const HttpClient::Transport &transport = HttpClient::Transport());
    ~EccSystemClient() override;
    bool QueryIdentity();
    bool QueryStatus();
    bool QueryAlarms();
    bool QueryConfiguration();
    bool QueryMaintenanceTargets();
    bool SubmitCoBoMaintenance(const QString &coboId,const QString &operationId);
    bool SubmitCoBoConfigure(const QString &coboId,const QString &operationId);
    bool SubmitCoBoLed(const QString &coboId,unsigned int asadId,unsigned int ledType,unsigned int ledState,const QString &operationId);
    bool SubmitCoBoLedBlink(const QString &coboId,const QString &operationId);
    bool SubmitCoBoLedFlip(const QString &coboId,unsigned int ledId,const QString &operationId);
    bool SubmitCoBoLedPulse(const QString &coboId,unsigned int ledId,unsigned int periodMs,const QString &operationId);
    bool SubmitAsAdPulserTrigger(const QString &coboId,int voltageMv,const QString &operationId);
    bool QueryOwnership();
    bool OwnershipPending() const { return fOwnershipBusy; }
    bool QueryConfigureReadbacks();
    bool ConfigureReadbacksPending() const { return fReadbacksBusy; }
    bool QueryReceivers();
    bool QueryReceiverEvents(const QString &senderId,bool fileBytes=false);
    bool ReceiverMetricsPending() const {return fReceiverEventsBusy;}
    bool QueryScanRuntimeIdentity(
        const GetDAQ::ScanRuntimePointIdentityRequest &request,
        const GetDAQ::ScanRuntimeIdentityExpectation &expected);
    bool QuerySources();
    bool QueryReceiverFailure();
    bool Submit(const QString &command, const QString &operationId,const QString &catalogExperiment=QString(),unsigned int catalogRun=0,const QString &failedOperationId=QString(),const QString &alarmOperationId=QString(),bool catalogReplaceExisting=false);
    bool SubmitPlanRetirement(const QByteArray &successorBytes,const QByteArray &targetsBytes,const QString &operationId);
    bool SubmitPlanRetirementCompletion(const GetDAQ::EccControlRequest &original,const QString &operationId);
    bool QueryControlHandoff(const GetDAQ::EccControlRequest &request,const GetDAQ::EccControlSystemStatus &original);
    void RetainPlanRetirement(const GetDAQ::EccControlRequest &request);
    bool Observe(const GetDAQ::EccControlRequest &request);
    void BeginScanLifecycleObservation(const QString &operationId);
    bool CompleteScanLifecycleObservation(const QString &operationId);
    bool Pending() const { return !fPendingId.isEmpty(); }
    bool Busy() const { return fIdentityBusy || fScanIdentityBusy || fReceiverEventsBusy || fReadBusy || fStatusBusy || fSubmitBusy || fAlarmBusy || fConfigurationBusy || fHandoffBusy || fMaintenanceBusy; }
    bool CanSubmit(const QString &command) const;
    bool CanDeferProgressUntilReadCompletes() const;
signals:
    void IdentitySignal(bool matched, QByteArray body, QString error);
    void StatusSignal(QByteArray body, QString error);
    void AlarmSignal(QByteArray body, QString error);
    void ConfigurationSignal(QByteArray body, QString error);
    void MaintenanceTargetsSignal(QByteArray body,QString error);
    void OwnershipSignal(QByteArray body, QString error);
    void ConfigureReadbacksSignal(QByteArray body, QString error);
    void ReceiverSignal(QByteArray body, QString error);
    void ReceiverEventsSignal(QString senderId,QByteArray body,QString error);
    void ReceiverFileBytesSignal(QString senderId,QByteArray body,QString error);
    void SourceSignal(QByteArray body, QString error);
    void ReceiverFailureSignal(QByteArray body, QString error);
    void OperationSignal(QString operationId, QString phase, QByteArray body, QString error);
    void ControlHandoffSignal(QByteArray body,QString error);
    void ScanRuntimeIdentitySignal(QByteArray body,QString error);
private:
    EccSystemClientOptions fOptions;
    IntentWriter fWriter;
    HttpClient *fHttp;
    QByteArray fCredential;
    GetDAQ::EccControlIdentity fIdentity;
    QString fPendingId;
    bool fIdentityBusy = false;
    bool fScanIdentityBusy = false;
    bool fReadBusy = false;
    bool fSubmitBusy = false;
    bool fMatched = false;
    bool fFault = false;
    bool fCredentialFault = false;
    bool fControlFault = false;
    unsigned long long fControlFaultGeneration = 0;
    QString fScanRecoveryOperationId;
    unsigned long long fScanRecoveryFaultGeneration = 0;
    bool fScanRecoveryMayClearFault = false;
    bool fScanRecoveryObservationFault = false;
    bool fStatusBusy = false;
    bool fReceiverBusy = false; // 只读显示请求不参与命令状态或故障解除。
    bool fReceiverEventsBusy = false;
    bool fSourceBusy = false;
    bool fReceiverFailureBusy = false; // Cached metadata; never submits automatic actions.
    bool fConfigurationBusy = false;
    bool fMaintenanceBusy=false;
    unsigned int fControllerState=0;
    QByteArray fMaintenanceCanonical;
    QByteArray fConfigurationCanonical;
    bool fOwnershipBusy = false;
    QByteArray fOwnershipBinding;
    bool fReadbacksBusy = false;
    bool fAlarmBusy = false, fAlarmAvailable = false;
    bool fStatusAvailable = false;
    bool fPlanRetired = false, fHandoffBusy = false;
    GetDAQ::EccControlRequest fRetirementRequest;
    GetDAQ::EccControlSystemStatus fLatestStatus;
    bool fRemoteBusy = false;
    QElapsedTimer fStatusClock;
    quint64 fRecoveryGeneration = 0;
    QMap<QByteArray, QByteArray> Headers();
    QUrl Url(const QString &path) const;
    void Receive(const GetDAQ::EccControlRequest &request, const QByteArray &body, const QString &failure);
    bool SubmitRequest(const GetDAQ::EccControlRequest &request);
    void LatchControlFault();
};
#endif
