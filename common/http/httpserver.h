#ifndef GETDAQ_HTTPSERVER_H
#define GETDAQ_HTTPSERVER_H
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QSet>
#include <QMap>
#include <QSslConfiguration>
#include <QSslSocket>

// 仅实现本项目需要的短连接 HTTP/1.1 GET/POST，无第三方 Web 框架。
class HttpConnection : public QObject
{
    Q_OBJECT
public:
    explicit HttpConnection(QTcpSocket *socket, QObject *parent = nullptr);
    void Reply(int code, const QByteArray &body, const QByteArray &type = "application/json");
    QByteArray RequestHeader(const QByteArray &name) const { return fHeaders.value(name.toLower()); }
signals:
    void RequestSignal(QString method, QString path, QByteArray body, HttpConnection *connection);
private:
    void ReadSlot();
    QTcpSocket *fSocket;
    QTimer fTimer;
    QByteArray fBuffer;
    QMap<QByteArray, QByteArray> fHeaders;
    bool fParsed = false;
    bool fReplied = false;
};

class HttpServer : public QTcpServer
{
    Q_OBJECT
public:
    explicit HttpServer(QObject *parent = nullptr);
    ~HttpServer() override;
    void SetTls(const QString &certificateFile,const QString &keyFile);
signals:
    void RequestSignal(QString method, QString path, QByteArray body, HttpConnection *connection);
private:
    QSet<HttpConnection *> fConnections;
    QSet<QSslSocket *> fHandshakes;
    QSslConfiguration fTls;
    bool fUseTls=false;
protected:
    void incomingConnection(qintptr descriptor) override;
};
#endif
