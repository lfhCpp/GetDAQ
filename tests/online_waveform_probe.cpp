#include "onlinereceiver.h"
#include <QCoreApplication>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMutexLocker>
#include <iostream>
int main(int argc,char **argv){
 QCoreApplication app(argc,argv);if(argc!=2)return 2;auto store=std::make_shared<GetDAQ::OnlineStore>();GetDAQ::OnlineReceiver receiver(store);receiver.Connect({QString::fromLocal8Bit(argv[1])});
 QTimer::singleShot(5000,&app,[&]{const auto events=store->Snapshot();bool decoded=false;QString error;
 for(const auto &event:events)for(const auto &frame:event.frames){QVector<GetDAQ::GrawChannelSamples> samples;if(GetDAQ::GrawFrameReader::ReadChannels(frame.bytes,frame.record,0,{0},&samples,&error)&&!samples.isEmpty())decoded=true;}
 QMutexLocker lock(&store->mutex);std::cout<<QJsonDocument(QJsonObject{{"frames",double(store->received)},{"cache_events",events.size()},{"decoded",decoded},{"errors",double(store->errors)}}).toJson(QJsonDocument::Compact).constData()<<std::endl;app.exit(decoded && store->received>0 && store->errors==0?0:1);});return app.exec();
}
