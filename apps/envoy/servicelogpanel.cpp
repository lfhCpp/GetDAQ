#include "servicelogpanel.h"
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QComboBox>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QFutureWatcher>
#include <QSignalBlocker>
#include <QSettings>
#include <QScrollBar>
#include <QUrlQuery>
#include <QRegularExpression>
#include <QtConcurrent>
#include "applicationinstance.h"
#ifdef Q_OS_LINUX
#include "deploymentcontroller.h"
#endif
ServiceLogPanel::ServiceLogPanel(QPlainTextEdit *events,QWidget *parent):QWidget(parent),fEvents(events){
 setObjectName("ServiceLogPanel");auto *layout=new QVBoxLayout(this);layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);auto *header=new QHBoxLayout;header->setContentsMargins(0,0,0,0);
 auto button=[&](const QString &text,const QString &name,int kind){auto *b=new QToolButton(this);b->setText(text);b->setObjectName(name);b->setCheckable(true);b->setAutoExclusive(true);b->setFixedHeight(22);header->addWidget(b);connect(b,&QToolButton::clicked,this,[this,kind]{Select(kind);});return b;};
 auto *event=button(tr("Event log"),"ShowEventLog",0);button(tr("ECC log"),"ShowEccLog",1);fData=button(tr("DataExporter log"),"ShowReceiverLog",2);event->setChecked(true);header->addStretch();fInstances=new QComboBox(this);fInstances->setObjectName("ServiceLogInstance");fInstances->setMinimumContentsLength(16);fInstances->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);header->addWidget(fInstances);layout->addLayout(header);
 fPages=new QStackedWidget(this);fPages->addWidget(fEvents);fService=new QPlainTextEdit(this);fService->setObjectName("BackendLogText");fService->setReadOnly(true);fService->document()->setMaximumBlockCount(4000);fPages->addWidget(fService);layout->addWidget(fPages);fInstances->hide();
 connect(fInstances,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{fSelection.clear();fLastError.clear();Poll();});auto *timer=new QTimer(this);timer->setInterval(2000);connect(timer,&QTimer::timeout,this,&ServiceLogPanel::Poll);timer->start();
}
void ServiceLogPanel::SetProject(const QString &path){fProject=path;fSelection.clear();if(fKind)ReloadInstances();}
QString ServiceLogPanel::PresentLog(QString text){
 // The original receiver prints a carriage-return spinner, not new log events.
 // Collapse only consecutive complete spinner records; retain other output verbatim.
 static const QRegularExpression waiting("(?:^|(?<=[\\r\\n]))(?:[ \\t]*Waiting for client connection[ .]*[\\r\\n]+)+");
 return text.replace(waiting,QStringLiteral("Waiting for incoming data connection (receiver listening).\n"));
}
QString ServiceLogPanel::VisibleText()const{return fKind?fService->toPlainText():fEvents->toPlainText();}
void ServiceLogPanel::Select(int kind){fKind=kind;fPages->setCurrentIndex(kind?1:0);fInstances->setVisible(kind!=0);fSelection.clear();fLastError.clear();if(kind){ReloadInstances();Poll();}}
void ServiceLogPanel::ReloadInstances(){
 const QSignalBlocker blocker(fInstances);const auto previous=fInstances->currentData().toJsonObject();fInstances->clear();
 const auto path=fProject.isEmpty()?QSettings().value(StartupConfigurationKey()+"/distributed-project").toString()+"/project.json":fProject;
 QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>4*1024*1024){fService->setPlainText(tr("Service logs are not attached to this connection. Pair the execution host and start/manage its services to view logs; external ECC and receiver connections remain usable."));return;}
 const auto project=QJsonDocument::fromJson(file.readAll()).object();const auto hosts=project["nodes"].toArray();
 for(const auto &value:project["groups"].toArray()){
  const auto group=value.toObject();const auto role=fKind==1?QString("ecc"):QString("receiver");QJsonArray assignments;if(fKind==1)assignments.append(group["ecc"]);else assignments=group["receivers"].toArray();
  for(const auto &v:assignments){const auto assignment=v.toObject();for(const auto &h:hosts){const auto node=h.toObject();if(node["id"]!=assignment["node_id"])continue;
   const auto instance=group["id"].toString()+'-'+(fKind==1?QString("ecc"):assignment["id"].toString());const auto name=node["name"].toString()+" / "+group["name"].toString();auto entry=node;entry["instance_id"]=instance;entry["component"]=role;entry["receiver_backend"]=group["receiver_backend"].toString("DataExporter");fInstances->addItem(name,entry);
   if(fKind==2){entry["component"]="sentry";fInstances->addItem(name+" / Sentry",entry);}
  }}
 }
 for(int i=0;i<fInstances->count();++i)if(fInstances->itemData(i).toJsonObject()==previous){fInstances->setCurrentIndex(i);break;}
 if(fInstances->count()==0)fService->setPlainText(tr("No managed service instances in this project. Existing external services have no attached log stream."));
}
void ServiceLogPanel::Poll(){
 if(!fKind || fBusy || fInstances->currentIndex()<0)return;const auto entry=fInstances->currentData().toJsonObject();const auto key=entry["id"].toString()+'/'+entry["instance_id"].toString()+'/'+entry["component"].toString();
 fData->setText(entry["receiver_backend"]=="dataRouter"?tr("DataRouter log"):tr("DataExporter log"));if(fSelection!=key){fSelection=key;fService->setPlainText(PresentLog(fBuffers.value(key)));}
#ifdef Q_OS_LINUX
 fBusy=true;const auto cursor=fCursors.value(key);auto *watcher=new QFutureWatcher<QPair<QJsonObject,QString>>(this);
 connect(watcher,&QFutureWatcher<QPair<QJsonObject,QString>>::finished,this,[this,watcher,key]{const auto result=watcher->result();watcher->deleteLater();fBusy=false;
  if(!result.second.isEmpty()){if(fSelection==key && fLastError!=result.second){fService->appendPlainText(tr("Log stream unavailable: %1").arg(result.second));fLastError=result.second;}return;}
  fLastError.clear();const auto observation=result.first;auto &buffer=fBuffers[key];if(observation["reset"].toBool())buffer.clear();buffer+=observation["text"].toString();if(buffer.size()>256*1024)buffer=buffer.right(256*1024);fCursors[key]=observation;
  if(fSelection==key){if(!observation["available"].toBool() && buffer.isEmpty())fService->setPlainText(tr("The service has not produced a log yet."));else if(fService->toPlainText()!=PresentLog(buffer)){const bool follow=fService->verticalScrollBar()->value()>=fService->verticalScrollBar()->maximum()-2;fService->setPlainText(PresentLog(buffer));if(follow)fService->verticalScrollBar()->setValue(fService->verticalScrollBar()->maximum());}}
 });
 watcher->setFuture(QtConcurrent::run([entry,cursor]{try{Deployment::OperationScope scope({},4000);const auto endpoint=Deployment::NodeEndpoint(entry);const auto url="/v1/instances/"+entry["instance_id"].toString()+"/logs/"+entry["component"].toString()+"?offset="+QString::number(qint64(cursor["offset"].toDouble()))+"&generation="+cursor["generation"].toString();return qMakePair(Deployment::Get(endpoint,url),QString());}catch(const std::exception &e){return qMakePair(QJsonObject(),QString::fromUtf8(e.what()));}}));
#else
 fService->setPlainText(tr("Managed service log streaming currently requires the Linux host manager."));
#endif
}
