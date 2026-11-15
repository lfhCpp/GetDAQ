#ifndef HTTPCLIENT_H
#define HTTPCLIENT_H
#include <QObject>
#include <QNetworkAccessManager>
#include <QSet>
#include <QMap>
#include <functional>

struct HttpClientResponse
{
    QByteArray body;
    QString error;
};

class HttpClient : public QObject
{
    Q_OBJECT
public:
    typedef std::function<HttpClientResponse(const QString &, const QUrl &, const QByteArray &,
        const QByteArray &, const QMap<QByteArray, QByteArray> &)> Transport;
    explicit HttpClient(QObject *parent = nullptr);
    HttpClient(const Transport &transport, QObject *parent = nullptr);
    typedef std::function<void(const QByteArray &, const QString &)> Callback;
    void Request(const QUrl &url, const QByteArray &body, const QByteArray &type, int timeoutMs, Callback callback,
                 const QMap<QByteArray, QByteArray> &headers = {}, int maximumResponseBytes = 1024 * 1024);
    void Cancel();
private:
    QNetworkAccessManager *fManager;
    QSet<QNetworkReply *> fReplies;
    Transport fTransport;
    quint64 fGeneration = 0;
};
#endif
