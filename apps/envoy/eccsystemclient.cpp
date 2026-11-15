#include "eccsystemclient.h"
#include "eccsystemclient_p.h"
#include <QFile>
#include <QFileInfo>
#include <QHostAddress>
#include <QRegularExpression>
#include <stdexcept>
namespace {
void Digest(const QString &text) {
    if (!QRegularExpression("^[0-9a-f]{64}$").match(text).hasMatch()) throw std::invalid_argument("ECC client requires explicit SHA/session values");
}
}
using namespace EccSystemClientInternal;
EccSystemClient::EccSystemClient(const EccSystemClientOptions &options, IntentWriter writer, QObject *parent,
                                 const HttpClient::Transport &transport)
    : QObject(parent), fOptions(options), fWriter(writer), fHttp(new HttpClient(transport, this))
{
    const QHostAddress address(options.address);
    if (address.protocol() != QAbstractSocket::IPv4Protocol || address.toString() != options.address || address == QHostAddress::AnyIPv4 ||
        options.port < 1 || options.port > 65535 || options.timeoutMs < 100 || options.timeoutMs > 60000 ||
        options.serviceId.isEmpty() || options.clientId.isEmpty() || !QFileInfo(options.credentialFile).isAbsolute() ||
        options.statusMaxAgeMs < 100 || options.statusMaxAgeMs > 3600000)
        throw std::invalid_argument("Invalid fixed ECC client endpoint/identity/credential path");
    Digest(options.snapshotSha256); Digest(options.inputBindingSha256); Digest(options.clientEpoch);
    Headers();
}
EccSystemClient::~EccSystemClient() { fHttp->Cancel(); }
QMap<QByteArray, QByteArray> EccSystemClient::Headers()
{
    if (fCredentialFault) throw std::runtime_error("ECC client credential fault is latched");
    try {
        const QFileInfo info(fOptions.credentialFile);
        if (!info.isFile() || info.isSymLink() || info.size() < 64 || info.size() > 65) throw std::runtime_error("Invalid ECC credential file");
#ifdef Q_OS_UNIX
        const auto unsafe = QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup | QFile::ReadOther | QFile::WriteOther | QFile::ExeOther;
        if (info.permissions() & unsafe) throw std::runtime_error("ECC credential requires private owner permissions");
#endif
        QFile file(fOptions.credentialFile);
        if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Cannot read ECC credential file");
        QByteArray token = file.readAll();
        if (token.size() == 65 && token.endsWith('\n')) token.chop(1);
        Digest(QString::fromLatin1(token));
        if (!fCredential.isEmpty() && token != fCredential) throw std::runtime_error("ECC credential changed; restoration does not clear gate");
        fCredential = token;
        return {{"Authorization", "Bearer " + token}, {"Connection", "close"}};
    } catch (...) { fCredentialFault = fFault = true; fMatched = false; throw; }
}
QUrl EccSystemClient::Url(const QString &path) const
{
    QUrl url; url.setScheme("http"); url.setHost(fOptions.address); url.setPort(fOptions.port); url.setPath(path); return url;
}
bool EccSystemClient::QueryIdentity()
{
    if (fIdentityBusy || fCredentialFault) return false;
    QMap<QByteArray, QByteArray> headers;
    try { headers = Headers(); } catch (const std::exception &e) { emit IdentitySignal(false, {}, Failure(e)); return false; }
    fIdentityBusy = true;
    fStatusAvailable = false;
    fHttp->Request(Url("/v1/identity"), {}, {}, fOptions.timeoutMs,
        [this](const QByteArray &body, const QString &failure) {
            fIdentityBusy = false;
            QString error = failure;
            if (error.isEmpty()) try {
                const auto identity = GetDAQ::DecodeEccControlIdentity(body.toStdString());
                if (identity.serviceId != Utf8(fOptions.serviceId) || identity.clientId != Utf8(fOptions.clientId) ||
                    identity.snapshotSha256 != Utf8(fOptions.snapshotSha256) || identity.inputBindingSha256 != Utf8(fOptions.inputBindingSha256))
                    throw std::runtime_error("ECC service/client/fixed snapshot differs");
                if (!fIdentity.instanceId.empty() && identity.instanceId != fIdentity.instanceId) throw std::runtime_error("ECC instance changed; no automatic takeover");
                fIdentity = identity; fMatched = !fFault && !fControlFault;
            } catch (const std::exception &e) { fFault = true; error = Failure(e); }
            if (!error.isEmpty()) fMatched = false;
            emit IdentitySignal(fMatched, error.isEmpty() ? body : QByteArray(), error);
        }, headers);
    return true;
}
bool EccSystemClient::QueryStatus()
{
    if (fStatusBusy || fAlarmBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray, QByteArray> headers;
    try { headers = Headers(); } catch (const std::exception &e) { emit StatusSignal({}, Failure(e)); return false; }
    fStatusBusy = true;const auto generation=fRecoveryGeneration;
    fHttp->Request(Url("/v1/status"), {}, {}, fOptions.timeoutMs,
        [this,generation](const QByteArray &body, const QString &failure) {
            fStatusBusy = false;
            if(generation!=fRecoveryGeneration){emit StatusSignal({},"Pre-recovery status discarded; refresh required");return;}
            QString error = failure;
            if (error.isEmpty()) try {
                const auto status = GetDAQ::DecodeEccControlSystemStatus(body.toStdString());
                if (status.instanceId != fIdentity.instanceId || status.snapshotSha256 != Utf8(fOptions.snapshotSha256))
                    throw std::runtime_error("ECC status instance/snapshot differs; no takeover");
                if (!status.clientEpoch.empty() && status.clientEpoch != Utf8(fOptions.clientEpoch))
                    throw std::runtime_error("ECC client epoch belongs to another owner");
                fRemoteBusy = status.operationActive;fControllerState=status.controller.state;fLatestStatus=status;
                bool unknown = status.controlNeedsReconciliation || status.controller.outcomeUnknown || status.controller.needsReconciliation || status.dataNeedsReconciliation;
                for (const auto &node : status.nodes) unknown = unknown || node.needsReconciliation;
                if (unknown) LatchControlFault();
                fStatusAvailable = true; fStatusClock.restart();
            } catch (const std::exception &e) { fFault = true; error = Failure(e); }
            if (!error.isEmpty()) fStatusAvailable = false;
            emit StatusSignal(error.isEmpty() ? body : QByteArray(), error);
        }, headers);
    return true;
}
bool EccSystemClient::CanDeferProgressUntilReadCompletes() const
{
    return fMatched && !fFault && !fControlFault && !fRemoteBusy && !Pending() && !fSubmitBusy && !fReadBusy && !fIdentityBusy &&
        fStatusAvailable && fStatusClock.isValid() && fStatusClock.elapsed()<=fOptions.statusMaxAgeMs &&
        (!fOptions.alarmStatusEnabled || fAlarmAvailable || fAlarmBusy) && (fStatusBusy || fAlarmBusy || fConfigurationBusy);
}
bool EccSystemClient::QueryAlarms()
{
    if (!fOptions.alarmStatusEnabled || fAlarmBusy || fStatusBusy || fCredentialFault || fIdentity.instanceId.empty()) return false;
    QMap<QByteArray,QByteArray> headers;
    try { headers=Headers(); } catch (const std::exception &e) { emit AlarmSignal({},Failure(e)); return false; }
    fAlarmBusy=true; fAlarmAvailable=false;const auto generation=fRecoveryGeneration;
    fHttp->Request(Url("/v1/alarms"),{},{},fOptions.timeoutMs,[this,generation](const QByteArray &body,const QString &failure) {
        fAlarmBusy=false;
        if(generation!=fRecoveryGeneration){emit AlarmSignal({},"Pre-recovery alarm cache discarded; refresh required");return;}
        QString error=failure;
        if (error.isEmpty()) try {
            const auto alarm=GetDAQ::DecodeEccControlAlarmStatus(body.toStdString());
            if (alarm.instanceId!=fIdentity.instanceId || alarm.snapshotSha256!=Utf8(fOptions.snapshotSha256)) throw std::runtime_error("ECC alarm instance/snapshot differs; no takeover");
            if (alarm.latched || alarm.stopRequested) LatchControlFault();
            fAlarmAvailable=true;
        } catch (const std::exception &e) { fFault=true; error=Failure(e); }
        if (!error.isEmpty()) LatchControlFault();
        emit AlarmSignal(error.isEmpty() ? body : QByteArray(),error);
    },headers);
    return true;
}
