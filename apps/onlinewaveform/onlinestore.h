#ifndef GETDAQ_ONLINE_STORE_H
#define GETDAQ_ONLINE_STORE_H
#include "grawreader.h"
#include <QMutex>
#include <QMap>
#include <QVector>
#include <QStringList>
namespace GetDAQ {
struct OnlineFrame {QString source;QByteArray bytes;GrawFrameRecord record;quint64 stamp;};
struct OnlineEvent {quint64 key;QVector<OnlineFrame> frames;};
class OnlineStore {
public:
 mutable QMutex mutex;QVector<OnlineEvent> events;QMap<QString,QString> links;
 quint64 received=0,dropped=0,errors=0,bytes=0,epoch=0; qint64 cachedBytes=0;int cachedFrames=0;
 void Reset();void Insert(const QString &,const QByteArray &);QString Statistics()const;
 QVector<OnlineEvent> Snapshot()const;
private:QMap<QString,quint32> lastEvent;
};
}
#endif
