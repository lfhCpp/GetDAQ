#include "servicelogpanel.h"
#include <QPlainTextEdit>
#include <QStackedWidget>
#include <QComboBox>
#include <QToolButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDateTime>
#include <QFontMetrics>
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
 auto button=[&](const QString &text,const QString &name,int kind){auto *b=new QToolButton(this);b->setText(text);b->setObjectName(name);b->setCheckable(true);b->setAutoExclusive(true);b->setStyleSheet("QToolButton { padding:2px 10px; }");b->setFixedHeight(qMax(26,b->fontMetrics().height()+6));b->setMinimumWidth(b->fontMetrics().horizontalAdvance(text)+24);header->addWidget(b);connect(b,&QToolButton::clicked,this,[this,kind]{Select(kind);});return b;};
 auto *event=button(tr("Event log"),"ShowEventLog",0);button(tr("ECC log"),"ShowEccLog",1);fData=button(tr("DataExporter log"),"ShowReceiverLog",2);event->setChecked(true);
 auto *clear=new QToolButton(this);clear->setText(tr("Clear"));clear->setObjectName("ClearLog");clear->setStyleSheet("QToolButton { padding:2px 8px; }");clear->setFixedHeight(fData->height());clear->setToolTip(tr("Clear the current display; keep raw log files and combined export history."));header->addWidget(clear);connect(clear,&QToolButton::clicked,this,&ServiceLogPanel::ClearVisible);header->addStretch();fInstances=new QComboBox(this);fInstances->setObjectName("ServiceLogInstance");fInstances->setMinimumContentsLength(16);fInstances->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);header->addWidget(fInstances);layout->addLayout(header);
 fPages=new QStackedWidget(this);fPages->addWidget(fEvents);fService=new QPlainTextEdit(this);fService->setObjectName("BackendLogText");fService->setReadOnly(true);fService->document()->setMaximumBlockCount(4000);fPages->addWidget(fService);layout->addWidget(fPages);fInstances->hide();
 connect(fInstances,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{fSelection.clear();ShowSelected();Poll();});auto *timer=new QTimer(this);timer->setInterval(2000);connect(timer,&QTimer::timeout,this,&ServiceLogPanel::Poll);timer->start();if(!fEvents->toPlainText().isEmpty())RecordEvent(fEvents->toPlainText());
}
void ServiceLogPanel::SetProject(const QString &path){fProjectSet=true;fProject=path;fProjectRevision.clear();fSelection.clear();if(fKind)ReloadInstances();}
QString ServiceLogPanel::PresentLog(QString text){return LogTimeline::CompactOutput(text);}
QString ServiceLogPanel::VisibleText()const{return fKind?fService->toPlainText():fEvents->toPlainText();}
QString ServiceLogPanel::CombinedText()const{return fTimeline.ExportText(fInFlight.size());}
void ServiceLogPanel::RecordEvent(const QString &line){fTimeline.Append("EVENT LOG | this GUI",line);}
void ServiceLogPanel::ClearVisible(){
 if(!fKind){fEvents->clear();return;}
 if(fInstances->currentIndex()>=0){const auto key=StreamKey(fInstances->currentData().toJsonObject());fBuffers[key].clear();++fClearEpoch[key];}
 fService->clear();
}
void ServiceLogPanel::Select(int kind){fKind=kind;fPages->setCurrentIndex(kind?1:0);fInstances->setVisible(kind!=0);fSelection.clear();ReloadInstances();ShowSelected();Poll();}
QString ServiceLogPanel::StreamKey(const QJsonObject &entry)const{return ProjectFile()+'/'+entry["url"].toString()+'/'+entry["id"].toString()+'/'+entry["instance_id"].toString()+'/'+entry["component"].toString();}
QString ServiceLogPanel::ProjectFile()const{
 if(fProjectSet || !fProject.isEmpty())return fProject;const auto root=QSettings().value(StartupConfigurationKey()+"/distributed-project").toString();return root.isEmpty()?QString():root+"/project.json";
}
void ServiceLogPanel::ReloadInstances(){
 const QSignalBlocker blocker(fInstances);const auto previous=fInstances->currentData().toJsonObject();fInstances->clear();fStreams.clear();
 const auto path=ProjectFile();const QFileInfo info(path);fProjectRevision=path+':'+QString::number(info.lastModified().toMSecsSinceEpoch())+':'+QString::number(info.size());
 QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>4*1024*1024){if(fKind)fService->setPlainText(tr("No managed log stream attached. External ECC / receiver connections remain usable."));return;}
 const auto project=QJsonDocument::fromJson(file.readAll()).object();const auto hosts=project["nodes"].toArray();QSet<QString> sentries;
 auto add=[&](const QJsonObject &group,const QJsonObject &assignment,const QString &component){
  for(const auto &h:hosts){auto entry=h.toObject();if(entry["id"]!=assignment["node_id"])continue;
   const auto instance=group["id"].toString()+'-'+(component=="ecc"?QString("ecc"):assignment["id"].toString());
   const auto name=entry["name"].toString()+" / "+group["name"].toString();entry["instance_id"]=instance;entry["component"]=component;entry["receiver_backend"]=group["receiver_backend"].toString("DataExporter");entry["display_name"]=name;
   if(component=="sentry"){
    const auto folder=group["project_directories"].toObject()[entry["id"].toString()].toString();
    const auto key=entry["id"].toString()+'/'+(group["project_mode"].toBool()?folder:instance);if(sentries.contains(key))continue;sentries.insert(key);entry["display_name"]=name+" / Sentry";
   }
   const auto label=component=="ecc"?QString("ECC LOG"):component=="sentry"?QString("SENTRY LOG"):entry["receiver_backend"].toString()+" DATA LOG";
   entry["source_label"]=label+" | "+name+" | "+instance;fStreams.append(entry);
  }
 };
 for(const auto &value:project["groups"].toArray()){
  const auto group=value.toObject();add(group,group["ecc"].toObject(),"ecc");
  for(const auto &rx:group["receivers"].toArray()){add(group,rx.toObject(),"receiver");add(group,rx.toObject(),"sentry");}
 }
 for(const auto &entry:fStreams)if((fKind==1 && entry["component"]=="ecc") || (fKind==2 && entry["component"]!="ecc"))fInstances->addItem(entry["display_name"].toString(),entry);
 for(int i=0;i<fInstances->count();++i)if(fInstances->itemData(i).toJsonObject()==previous){fInstances->setCurrentIndex(i);break;}
 if(fKind && fInstances->count()==0)fService->setPlainText(tr("No managed service instances in this project. External services have no attached log stream."));
}
void ServiceLogPanel::ShowSelected(){
 if(!fKind || fInstances->currentIndex()<0)return;const auto entry=fInstances->currentData().toJsonObject();const auto key=StreamKey(entry);
 fData->setText(entry["receiver_backend"]=="dataRouter"?tr("DataRouter log"):tr("DataExporter log"));
 if(fSelection!=key){fSelection=key;fService->setPlainText(PresentLog(fBuffers.value(key)));}
}
void ServiceLogPanel::Poll(){
 const auto path=ProjectFile();const QFileInfo info(path);const auto revision=path+':'+QString::number(info.lastModified().toMSecsSinceEpoch())+':'+QString::number(info.size());if(revision!=fProjectRevision)ReloadInstances();ShowSelected();
 // Collect all components, including when Event log is selected. Bound concurrent
 // requests and rotate through hosts so an unavailable host cannot block others.
 if(fKind && fInstances->currentIndex()>=0)ReadStream(fInstances->currentData().toJsonObject());
 for(int i=0;i<fStreams.size() && fInFlight.size()<4;++i){if(fNextStream>=fStreams.size())fNextStream=0;ReadStream(fStreams[fNextStream++]);}
}
void ServiceLogPanel::ReadStream(const QJsonObject &entry){
 const auto key=StreamKey(entry);if(fInFlight.contains(key) || fInFlight.size()>=4)return;
#ifdef Q_OS_LINUX
 fInFlight.insert(key);const auto cursor=fCursors.value(key);const int clearEpoch=fClearEpoch.value(key);auto *watcher=new QFutureWatcher<QPair<QJsonObject,QString>>(this);
 connect(watcher,&QFutureWatcher<QPair<QJsonObject,QString>>::finished,this,[this,watcher,key,entry,clearEpoch]{
  const auto result=watcher->result();watcher->deleteLater();fInFlight.remove(key);
  if(!result.second.isEmpty()){
   if(fErrors.value(key)!=result.second){fErrors[key]=result.second;fTimeline.Append(entry["source_label"].toString()+" [collector]",tr("Log read unavailable: %1").arg(result.second));if(fSelection==key && clearEpoch==fClearEpoch.value(key))fService->appendPlainText(tr("Log stream unavailable: %1").arg(result.second));}return;
  }
  fErrors.remove(key);const auto observation=result.first;const auto text=observation["text"].toString();
  QString snapshotNote;if(observation["reset"].toBool() && observation["available"].toBool())snapshotNote=QString("[Initial tail or log file reset; byte range %1..%2; file-size boundary %3]\n").arg(observation["start_offset"].toDouble()).arg(observation["offset"].toDouble()).arg(observation.contains("snapshot_size")?QString::number(qint64(observation["snapshot_size"].toDouble())):QString("not reported"));
  fTimeline.AppendChunk(key,entry["source_label"].toString(),snapshotNote+text,observation["reset"].toBool());
  // Clear cannot be undone by a response that was already in flight. Its cursor
  // and export history still advance so the old bytes will not be read again.
  auto &buffer=fBuffers[key];if(observation["reset"].toBool())buffer.clear();
  if(clearEpoch==fClearEpoch.value(key))buffer+=text;if(buffer.size()>256*1024)buffer=buffer.right(256*1024);fCursors[key]=observation;
  if(fKind && fSelection==key && clearEpoch==fClearEpoch.value(key)){
   if(!observation["available"].toBool() && buffer.isEmpty())fService->setPlainText(tr("The service has not produced a log yet."));
   else if(fService->toPlainText()!=PresentLog(buffer)){const bool follow=fService->verticalScrollBar()->value()>=fService->verticalScrollBar()->maximum()-2;fService->setPlainText(PresentLog(buffer));if(follow)fService->verticalScrollBar()->setValue(fService->verticalScrollBar()->maximum());}
  }
 });
 watcher->setFuture(QtConcurrent::run([entry,cursor]{try{Deployment::OperationScope scope({},4000);const auto endpoint=Deployment::NodeEndpoint(entry);const auto url="/v1/instances/"+entry["instance_id"].toString()+"/logs/"+entry["component"].toString()+"?offset="+QString::number(qint64(cursor["offset"].toDouble()))+"&generation="+cursor["generation"].toString();return qMakePair(Deployment::Get(endpoint,url),QString());}catch(const std::exception &e){return qMakePair(QJsonObject(),QString::fromUtf8(e.what()));}}));
#else
 Q_UNUSED(entry);if(fKind)fService->setPlainText(tr("Managed log streaming requires the Linux host manager."));
#endif
}
