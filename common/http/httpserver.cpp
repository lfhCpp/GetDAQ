#include "httpserver.h"
#include <QMap>
#include <QFile>
#include <QSslKey>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#endif

HttpConnection::HttpConnection(QTcpSocket *socket, QObject *parent) : QObject(parent), fSocket(socket)
{
    socket->setParent(this);
    socket->setReadBufferSize(96 * 1024);
    fTimer.setSingleShot(true);
    fTimer.start(15000);
    connect(&fTimer, &QTimer::timeout, this, [this]() { Reply(408, "{\"error\":\"request timeout\"}"); });
    connect(socket, &QTcpSocket::disconnected, this, &QObject::deleteLater);
    connect(socket, &QTcpSocket::readyRead, this, &HttpConnection::ReadSlot);
}

void HttpConnection::Reply(int code, const QByteArray &body, const QByteArray &type)
{
    if (fReplied) return;
    fReplied = true; fTimer.stop();
    const QByteArray reason = code == 200 ? "OK" : code == 202 ? "Accepted" : code == 400 ? "Bad Request" : code == 401 ? "Unauthorized" : code == 404 ? "Not Found" :
        code == 409 ? "Conflict" : code == 405 ? "Method Not Allowed" : code == 408 ? "Request Timeout" :
        code == 412 ? "Precondition Failed" : code == 413 ? "Payload Too Large" : code == 503 ? "Service Unavailable" : "Internal Server Error";
    fSocket->write("HTTP/1.1 " + QByteArray::number(code) + " " + reason + "\r\nContent-Type: " + type +
        "\r\nContent-Length: " + QByteArray::number(body.size()) + "\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n" + body);
    fSocket->disconnectFromHost();
    QTimer::singleShot(2000, this, [this]() { fSocket->abort(); deleteLater(); });
}

void HttpConnection::ReadSlot()
{
    if (fParsed || fReplied) { fSocket->readAll(); return; }
    fBuffer += fSocket->readAll();
    const int split = fBuffer.indexOf("\r\n\r\n");
    if (split > 16384 || (split < 0 && fBuffer.size() > 16384)) { Reply(413, "{\"error\":\"header too large\"}"); return; }
    if (split < 0) return;
    QList<QByteArray> lines = fBuffer.left(split).split('\n');
    const QList<QByteArray> first = lines.takeFirst().trimmed().split(' ');
    if (first.size() != 3 || (first[2] != "HTTP/1.1" && first[2] != "HTTP/1.0") || !first[1].startsWith('/')) {
        Reply(400, "{\"error\":\"invalid request line\"}"); return;
    }
    QMap<QByteArray, QByteArray> headers;
    for (const QByteArray &raw : lines) {
        const QByteArray line = raw.trimmed();
        const int colon = line.indexOf(':');
        const QByteArray key = line.left(colon).toLower();
        if (colon <= 0 || headers.contains(key)) { Reply(400, "{\"error\":\"duplicate or malformed header\"}"); return; }
        headers.insert(key, line.mid(colon + 1).trimmed());
    }
    if (headers.contains("transfer-encoding")) { Reply(400, "{\"error\":\"chunked requests are not supported\"}"); return; }
    bool ok = true;
    const qint64 length = headers.contains("content-length") ? headers.value("content-length").toLongLong(&ok) : 0;
    if (!ok || length < 0) { Reply(400, "{\"error\":\"invalid content length\"}"); return; }
    const qint64 maximumBody=first[0]=="POST" && first[1]=="/v1/operations" ? 2*1024*1024 : 65536;
    if (length > maximumBody) { Reply(413, "{\"error\":\"body too large\"}"); return; }
    if (fBuffer.size() < split + 4 + length) return;
    if (fBuffer.size() != split + 4 + length) { Reply(400, "{\"error\":\"pipelining is not supported\"}"); return; }
    fParsed = true;
    fHeaders = headers; // 控制服务按需读取认证头；不输出或保存凭据。
    fTimer.start(180000); // 允许归档先等待采集后端排空。
    emit RequestSignal(QString::fromLatin1(first[0]), QString::fromLatin1(first[1]), fBuffer.mid(split + 4), this);
    fBuffer.clear();
}

HttpServer::HttpServer(QObject *parent) : QTcpServer(parent)
{
    connect(this, &QTcpServer::newConnection, this, [this]() {
        while (hasPendingConnections()) {
            QTcpSocket *socket = nextPendingConnection();
#ifdef Q_OS_UNIX
            const int descriptor = int(socket->socketDescriptor());
            const int flags = ::fcntl(descriptor, F_GETFD);
            if (flags < 0 || ::fcntl(descriptor, F_SETFD, flags | FD_CLOEXEC)) {
                socket->abort(); socket->deleteLater(); continue;
            }
#endif
            if (fConnections.size() >= 64) {
                socket->write("HTTP/1.1 503 Service Unavailable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
                connect(socket, &QTcpSocket::disconnected, socket, &QObject::deleteLater);
                socket->disconnectFromHost(); continue;
            }
            HttpConnection *connection = new HttpConnection(socket, this);
            fConnections.insert(connection);
            connect(connection, &QObject::destroyed, this, [this, connection]() { fConnections.remove(connection); });
            connect(connection, &HttpConnection::RequestSignal, this, &HttpServer::RequestSignal);
        }
    });
}

HttpServer::~HttpServer()
{
    // 在成员 QSet 析构前解除回调，避免 QObject 析构子对象时访问已销毁的集合。
    for (HttpConnection *connection : fConnections) disconnect(connection, nullptr, this, nullptr);
    for (QSslSocket *socket:fHandshakes)disconnect(socket,nullptr,this,nullptr);
}
void HttpServer::SetTls(const QString &certificateFile,const QString &keyFile) {
    if(isListening() || !QSslSocket::supportsSsl())throw std::runtime_error("TLS must be configured before listening; a supported OpenSSL runtime is required");
    QFile cert(certificateFile),key(keyFile);
    if(!cert.open(QIODevice::ReadOnly) || !key.open(QIODevice::ReadOnly) || cert.size()>65536 || key.size()>65536)throw std::runtime_error("Cannot read bounded TLS certificate/key");
    const QSslCertificate certificate(cert.readAll());const QSslKey privateKey(key.readAll(),QSsl::Rsa);
    if(certificate.isNull() || privateKey.isNull())throw std::runtime_error("Invalid TLS certificate/key");
    fTls=QSslConfiguration::defaultConfiguration();fTls.setProtocol(QSsl::TlsV1_2OrLater);fTls.setLocalCertificate(certificate);fTls.setPrivateKey(privateKey);fTls.setPeerVerifyMode(QSslSocket::VerifyNone);fUseTls=true;
}
void HttpServer::incomingConnection(qintptr descriptor) {
    if(!fUseTls){QTcpServer::incomingConnection(descriptor);return;}
    auto *socket=new QSslSocket(this);
    if(!socket->setSocketDescriptor(descriptor)){delete socket;return;}
#ifdef Q_OS_UNIX
    const int flags=::fcntl(int(descriptor),F_GETFD);
    if(flags<0 || ::fcntl(int(descriptor),F_SETFD,flags|FD_CLOEXEC)){delete socket;return;}
#endif
    if(fHandshakes.size()+fConnections.size()>=64){delete socket;return;}
    fHandshakes.insert(socket);socket->setSslConfiguration(fTls);
    connect(socket,&QSslSocket::encrypted,this,[this,socket]{fHandshakes.remove(socket);disconnect(socket,nullptr,this,nullptr);addPendingConnection(socket);emit newConnection();});
    connect(socket,&QTcpSocket::disconnected,socket,&QObject::deleteLater);
    connect(socket,&QObject::destroyed,this,[this,socket]{fHandshakes.remove(socket);});
    QTimer::singleShot(10000,socket,[socket]{if(!socket->isEncrypted()){socket->abort();socket->deleteLater();}});
    socket->startServerEncryption();
}
