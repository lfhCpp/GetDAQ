#include "onlinestore.h"
#include "../datareceiver/mfmheader.h"
#include <QMutexLocker>
namespace GetDAQ {
void OnlineStore::Reset(){QMutexLocker lock(&mutex);events.clear();lastEvent.clear();cachedBytes=0;cachedFrames=0;++epoch;}
QVector<OnlineEvent> OnlineStore::Snapshot()const{QMutexLocker lock(&mutex);return events;}
QString OnlineStore::Statistics()const{QMutexLocker lock(&mutex);QStringList state;for(auto it=links.begin();it!=links.end();++it)state<<it.key()+": "+it.value();return QString("Frames %1 | Preview discarded %2 | Errors %3 | Cache %4 MiB | Received %5 MiB | %6").arg(received).arg(dropped).arg(errors).arg(cachedBytes/1048576.0,0,'f',1).arg(bytes/1048576.0,0,'f',1).arg(state.join("; "));}
void OnlineStore::Insert(const QString &source,const QByteArray &body){
 const auto index=GrawFrameReader::BuildIndex(body,nullptr,4096);QMutexLocker lock(&mutex);bytes+=body.size();if(!index.error.isEmpty()){++errors;return;}
 for(auto record:index.frames){
  ++received;const auto identity=source+':'+QString::number(record.cobo)+':'+QString::number(record.asad);const auto data=body.mid(record.offset,record.bytes);record.offset=0;
  const auto stamp=data.size()>=22?MfmHeaderInteger(reinterpret_cast<const std::uint8_t *>(data.constData()+16),6,record.littleEndian):0;
  bool reset=lastEvent.contains(identity) && record.event<lastEvent[identity];
  if(!events.isEmpty() && quint32(events.last().key)==record.event)for(const auto &old:events.last().frames)if(old.source==source && old.record.cobo==record.cobo && old.record.asad==record.asad && old.stamp!=stamp)reset=true;
  // No Run ID is carried by the legacy stream. Conservatively discard previews
  // at counter rollback/timestamp conflict instead of mixing old and new events.
  if(reset){dropped+=events.size();events.clear();lastEvent.clear();cachedBytes=0;cachedFrames=0;++epoch;}
  lastEvent[identity]=record.event;const quint64 key=(epoch<<32)|record.event;int event=-1;for(int i=0;i<events.size();++i)if(events[i].key==key){event=i;break;}
  if(event<0){events.append(OnlineEvent{key,{}});event=events.size()-1;}
  bool duplicate=false;for(const auto &old:events[event].frames)if(old.source==source && old.record.cobo==record.cobo && old.record.asad==record.asad && old.bytes==data)duplicate=true;
  if(!duplicate){events[event].frames.append(OnlineFrame{source,data,record,stamp});cachedBytes+=data.size();++cachedFrames;}
  while(events.size()>100 || cachedFrames>4096 || cachedBytes>256LL*1024*1024){for(const auto &frame:events.first().frames)cachedBytes-=frame.bytes.size();cachedFrames-=events.first().frames.size();events.removeFirst();++dropped;}
 }
}
}
