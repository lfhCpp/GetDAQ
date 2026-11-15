#include "httpclient.h"
#include <QNetworkReply>
#include <QTimer>
#include <QVariant>
#include <memory>
#include <cstring>

namespace {
// 控制命令发送后不能倒回重发；Qt默认的QByteArray上传会允许网络层重试。
class SinglePassUpload : public QIODevice
{
public:
    explicit SinglePassUpload(const QByteArray &bytes, QObject *parent)
        : QIODevice(parent), fBytes(bytes), fOffset(0) { open(QIODevice::ReadOnly); }
    bool isSequential() const override { return true; }
    bool reset() override { return false; }
    bool seek(qint64) override { return false; }
    qint64 bytesAvailable() const override { return fBytes.size() - fOffset + QIODevice::bytesAvailable(); }
protected:
    qint64 readData(char *data, qint64 maximum) override {
        const qint64 count = qMin(maximum, qint64(fBytes.size()) - fOffset);
        if (count > 0) { std::memcpy(data, fBytes.constData() + fOffset, std::size_t(count)); fOffset += count; }
        return count;
    }
    qint64 writeData(const char *, qint64) override { return -1; }
private:
    QByteArray fBytes;
    qint64 fOffset;
};
}

HttpClient::HttpClient(QObject *parent) : QObject(parent), fManager(new QNetworkAccessManager(this)) {}
HttpClient::HttpClient(const Transport &transport, QObject *parent)
    : QObject(parent), fManager(transport ? nullptr : new QNetworkAccessManager(this)), fTransport(transport) {}

void HttpClient::Request(const QUrl &url, const QByteArray &body, const QByteArray &type, int timeoutMs, Callback callback,
                         const QMap<QByteArray, QByteArray> &headers, int maximumResponseBytes)
{
    if (maximumResponseBytes < 1 || maximumResponseBytes > 8 * 1024 * 1024) { callback({}, "Invalid response budget."); return; }
    if (fTransport) {
        const quint64 generation = fGeneration;
        const QString method = type.isEmpty() ? QStringLiteral("GET") : QStringLiteral("POST");
        QTimer::singleShot(0, this, [this, generation, method, url, body, type, timeoutMs,
                                     callback, headers, maximumResponseBytes]() {
            Q_UNUSED(timeoutMs);
            if (generation != fGeneration) return;
            HttpClientResponse response;
            try { response = fTransport(method, url, body, type, headers); }
            catch (...) { response.error = "In-process HTTP transport threw an exception."; }
            if (generation != fGeneration) return;
            if (response.error.isEmpty() && response.body.size() > maximumResponseBytes) {
                callback({}, "Response exceeds its bounded metadata budget.");
                return;
            }
            callback(response.body, response.error);
        });
        return;
    }
    QNetworkRequest request(url);
    if (!type.isEmpty()) request.setHeader(QNetworkRequest::ContentTypeHeader, QString::fromLatin1(type));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    for (auto it = headers.constBegin(); it != headers.constEnd(); ++it) request.setRawHeader(it.key(), it.value());
    QNetworkReply *reply = nullptr;
    if (type.isEmpty()) reply = fManager->get(request);
    else {
        request.setAttribute(QNetworkRequest::DoNotBufferUploadDataAttribute, true);
        request.setHeader(QNetworkRequest::ContentLengthHeader, body.size());
        SinglePassUpload *upload = new SinglePassUpload(body, this);
        reply = fManager->post(request, upload);
        upload->setParent(reply);
    }
    fReplies.insert(reply);
    QTimer *timer = new QTimer(reply);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, reply, [reply]() { reply->setProperty("failure", "Request timed out; remote result may be unknown."); reply->abort(); });
    timer->start(timeoutMs);
    const std::shared_ptr<QByteArray> data(new QByteArray);
    connect(reply, &QNetworkReply::readyRead, this, [reply, data, maximumResponseBytes]() {
        if (data->size() + reply->bytesAvailable() > maximumResponseBytes) {
            reply->setProperty("failure", "Response exceeds its bounded metadata budget."); reply->abort();
        } else data->append(reply->readAll());
    });
    connect(reply, &QNetworkReply::finished, this, [this, reply, timer, data, callback, maximumResponseBytes]() {
        timer->stop();
        QString error = reply->property("failure").toString();
        const int code = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (error.isEmpty() && reply->error() != QNetworkReply::NoError) error = reply->errorString();
        if (error.isEmpty() && (code < 200 || code >= 300)) error = QString("HTTP %1").arg(code);
        // 代理可能改写reason phrase，调用者须按数字状态区分404与未知网络结果。
        if (!error.isEmpty() && code >= 400) error = QString("HTTP %1: %2").arg(code).arg(error);
        if (error.isEmpty()) {
            if (data->size() + reply->bytesAvailable() > maximumResponseBytes) error = "Response exceeds its bounded metadata budget.";
            else data->append(reply->readAll());
        }
        fReplies.remove(reply);
        reply->deleteLater();
        callback(*data, error);
    });
}

void HttpClient::Cancel()
{
    ++fGeneration;
    const auto replies = fReplies;
    fReplies.clear();
    for (QNetworkReply *reply : replies) {
        disconnect(reply, nullptr, this, nullptr);
        // A network error may have finished the reply while its finished()
        // callback is still queued. Re-aborting that reply can make Qt report
        // that QNetworkReplyImplPrivate::error() was called twice.
        if (!reply->isFinished()) reply->abort();
        reply->deleteLater();
    }
}
