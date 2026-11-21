#include "onlinereceiver.h"
#include <QUrl>
#include <QTimer>
#include <QMutexLocker>
#include <QtEndian>
namespace GetDAQ {
void OnlineReceiver::Release(std::shared_ptr<Connection> c){if(c->retained)pendingBytes-=c->expected;c->expected=c->remaining=0;c->retained=false;c->body.clear();c->header.clear();}
void OnlineReceiver::Stop(){stopped=true;for(auto &c:clients){c->socket->disconnect(this);c->socket->abort();Release(c);c->socket->deleteLater();}clients.clear();}
void OnlineReceiver::Connect(QStringList endpoints){
 Stop();stopped=false;store->Reset();endpoints.removeDuplicates();endpoints=endpoints.mid(0,32);
 for(const auto &endpoint:endpoints){QUrl url("tcp://"+endpoint);if(url.host().isEmpty() || url.port()<1 || url.port()>65535){QMutexLocker lock(&store->mutex);store->links[endpoint]="Invalid host/export port";continue;}
 auto c=std::make_shared<Connection>();c->endpoint=endpoint;c->socket=new QTcpSocket(this);c->socket->setReadBufferSize(256*1024);clients.append(c);
 connect(c->socket,&QTcpSocket::readyRead,this,[this,c]{Read(c);});
 connect(c->socket,&QTcpSocket::connected,this,[this,c]{store->Reset();QMutexLocker lock(&store->mutex);store->links[c->endpoint]="Connected";});
 connect(c->socket,&QTcpSocket::disconnected,this,[this,c,url]{Release(c);{QMutexLocker lock(&store->mutex);store->links[c->endpoint]="Disconnected; retrying";}if(!stopped)QTimer::singleShot(2000,c->socket,[this,c,url]{if(!stopped && clients.contains(c) && c->socket->state()==QAbstractSocket::UnconnectedState)c->socket->connectToHost(url.host(),quint16(url.port()));});});
 connect(c->socket,QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::error),this,[this,c,url](QAbstractSocket::SocketError){QMutexLocker lock(&store->mutex);store->links[c->endpoint]=c->socket->errorString();if(!stopped && c->socket->state()==QAbstractSocket::UnconnectedState)QTimer::singleShot(2000,c->socket,[this,c,url]{if(!stopped && clients.contains(c) && c->socket->state()==QAbstractSocket::UnconnectedState)c->socket->connectToHost(url.host(),quint16(url.port()));});});
 c->socket->connectToHost(url.host(),quint16(url.port()));
 }
}
void OnlineReceiver::Read(std::shared_ptr<Connection> c){
 qint64 worked=0;while(c->socket->bytesAvailable() && worked<256*1024){
  if(!c->remaining){c->header+=c->socket->read(8-c->header.size());if(c->header.size()!=8)return;c->expected=qFromLittleEndian<quint64>(reinterpret_cast<const uchar *>(c->header.constData()));c->header.clear();
   if(!c->expected)continue;if(c->expected>64ULL*1024*1024 || c->expected<8){{QMutexLocker lock(&store->mutex);++store->errors;store->links[c->endpoint]="Invalid TCP message length";}c->socket->abort();return;}
   c->remaining=c->expected;c->retained=pendingBytes+qint64(c->expected)<=64LL*1024*1024;if(c->retained){pendingBytes+=c->expected;c->body.reserve(int(c->expected));}else{QMutexLocker lock(&store->mutex);++store->dropped;}
  }
  const auto part=c->socket->read(qMin<quint64>(c->remaining,16384));if(part.isEmpty())return;worked+=part.size();c->remaining-=part.size();if(c->retained)c->body+=part;
  if(!c->remaining){if(c->retained)store->Insert(c->endpoint,c->body);Release(c);}
 }
 if(c->socket->bytesAvailable())QTimer::singleShot(0,this,[this,c]{if(!stopped && clients.contains(c))Read(c);});
}
}
