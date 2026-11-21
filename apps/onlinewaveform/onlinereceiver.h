#ifndef GETDAQ_ONLINE_RECEIVER_H
#define GETDAQ_ONLINE_RECEIVER_H
#include "onlinestore.h"
#include <QObject>
#include <QTcpSocket>
#include <memory>
namespace GetDAQ {
class OnlineReceiver:public QObject {
 Q_OBJECT
public:explicit OnlineReceiver(std::shared_ptr<OnlineStore> store):store(std::move(store)){}
public slots:void Connect(QStringList endpoints);void Stop();
private:
 struct Connection {QTcpSocket *socket=nullptr;QString endpoint;QByteArray header,body;quint64 expected=0,remaining=0;bool retained=false;};
 std::shared_ptr<OnlineStore> store;QVector<std::shared_ptr<Connection>> clients;bool stopped=true;qint64 pendingBytes=0;
 void Read(std::shared_ptr<Connection>);void Release(std::shared_ptr<Connection>);
};
}
#endif
