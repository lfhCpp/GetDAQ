#include <QScrollArea>
#include "onlinereceiver.h"
#include "waveformcanvas.h"
#include "waveformexport.h"
#include <QApplication>
#include <QMainWindow>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QComboBox>
#include <QSpinBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QDialog>
#include <QListWidget>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFutureWatcher>
#include <QSignalBlocker>
#include <QThread>
#include <QTimer>
#include <QtConcurrent>
#include <QCommandLineParser>
#include <iostream>
using namespace GetDAQ;
class OnlineWindow:public QMainWindow {
 std::shared_ptr<OnlineStore> store=std::make_shared<OnlineStore>();OnlineReceiver *receiver;QThread thread;
 QString choicesPath;QStringList initialEndpoints,activeEndpoints;QJsonArray currentChoices;QComboBox *exporters;QCheckBox *paused;WaveformCanvas *canvas;QComboBox *event,*source;QSpinBox *cobo,*asad,*aget;QCheckBox *follow;QLabel *status;QVector<int> channels;QVector<OnlineFrame> frozen;bool decoding=false;quint64 displayed=0;QString selection;
public:
 OnlineWindow(QStringList initial,QString path):choicesPath(path),initialEndpoints(initial){
  setWindowTitle("GetDAQ Online Waveform Viewer");resize(1180,720);auto *central=new QWidget;auto *layout=new QVBoxLayout(central);auto *network=new QHBoxLayout;
  exporters=new QComboBox;exporters->setObjectName("ConnectedDataExporters");exporters->setToolTip("Uses the current GetDAQ project. For an external receiver select DataExporter and its TCP preview export port once in the connection settings.");network->addWidget(new QLabel("DataExporter"));network->addWidget(exporters,1);auto *refresh=new QPushButton("Refresh choices");paused=new QCheckBox("Pause preview");network->addWidget(refresh);network->addWidget(paused);layout->addLayout(network);

  auto *controls=new QHBoxLayout;event=new QComboBox;event->setMinimumContentsLength(16);source=new QComboBox;cobo=new QSpinBox;cobo->setRange(0,255);asad=new QSpinBox;asad->setRange(0,3);aget=new QSpinBox;aget->setRange(0,3);follow=new QCheckBox("Follow latest");follow->setChecked(true);auto *channelButton=new QPushButton("Channels… (68)");auto *surface=new QCheckBox("3D");auto *save=new QPushButton("Export plot/data…");auto *reset=new QPushButton("New session");
  controls->addWidget(follow);controls->addWidget(new QLabel("Event ID"));controls->addWidget(event,1);auto *jump=new QLineEdit;jump->setPlaceholderText("Event ID");jump->setMaximumWidth(85);auto *go=new QPushButton("Go");controls->addWidget(jump);controls->addWidget(go);
  connect(go,&QPushButton::clicked,this,[this,jump]{bool ok=false;const auto id=jump->text().toUInt(&ok);if(!ok)return;for(int i=event->count()-1;i>=0;--i)if(quint32(event->itemData(i).toULongLong())==id){follow->setChecked(false);event->setCurrentIndex(i);frozen.clear();selection.clear();return;}status->setText("Requested event is not in the bounded live cache; use saved raw data for older events.");});connect(jump,&QLineEdit::returnPressed,go,&QPushButton::click);layout->addLayout(controls);controls=new QHBoxLayout;controls->addWidget(new QLabel("Source"));controls->addWidget(source,1);controls->addWidget(new QLabel("CoBo"));controls->addWidget(cobo);controls->addWidget(new QLabel("AsAd"));controls->addWidget(asad);controls->addWidget(new QLabel("AGET"));controls->addWidget(aget);controls->addWidget(channelButton);controls->addWidget(surface);controls->addWidget(save);controls->addWidget(reset);layout->addLayout(controls);
  canvas=new WaveformCanvas;canvas->setProperty("emptyMessage","Waiting for a matching online waveform");auto *plotScroll=new QScrollArea;plotScroll->setWidgetResizable(true);plotScroll->setFrameShape(QFrame::NoFrame);plotScroll->setWidget(canvas);layout->addWidget(plotScroll,1);status=new QLabel("Connect to a DataExporter TCP export endpoint. Preview may discard frames; raw recording is independent.");status->setWordWrap(true);layout->addWidget(status);setCentralWidget(central);for(int i=0;i<68;++i)channels<<i;
  receiver=new OnlineReceiver(store);receiver->moveToThread(&thread);connect(&thread,&QThread::finished,receiver,&QObject::deleteLater);thread.start();
  connect(exporters,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[this]{UseExporter();});connect(paused,&QCheckBox::toggled,this,[this]{UseExporter();});connect(refresh,&QPushButton::clicked,this,[this]{LoadChoices();});
  connect(reset,&QPushButton::clicked,this,[this]{store->Reset();frozen.clear();canvas->Clear();selection.clear();});
  connect(surface,&QCheckBox::toggled,canvas,&WaveformCanvas::SetSurface);
  connect(event,QOverload<int>::of(&QComboBox::activated),this,[this]{follow->setChecked(false);frozen.clear();selection.clear();});
  connect(follow,&QCheckBox::toggled,this,[this]{frozen.clear();selection.clear();});
  connect(channelButton,&QPushButton::clicked,this,[this,channelButton]{QDialog dialog(this);dialog.setWindowTitle("Select AGET channels");QVBoxLayout layout(&dialog);QListWidget list;for(int i=0;i<68;++i){auto *item=new QListWidgetItem(QString("Channel %1").arg(i),&list);item->setFlags(item->flags()|Qt::ItemIsUserCheckable);item->setCheckState(channels.contains(i)?Qt::Checked:Qt::Unchecked);}layout.addWidget(&list);QPushButton all("All"),none("None");QHBoxLayout selectionButtons;selectionButtons.addWidget(&all);selectionButtons.addWidget(&none);layout.addLayout(&selectionButtons);connect(&all,&QPushButton::clicked,&dialog,[&list]{for(int i=0;i<list.count();++i)list.item(i)->setCheckState(Qt::Checked);});connect(&none,&QPushButton::clicked,&dialog,[&list]{for(int i=0;i<list.count();++i)list.item(i)->setCheckState(Qt::Unchecked);});QDialogButtonBox buttons(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);layout.addWidget(&buttons);connect(&buttons,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);connect(&buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);if(dialog.exec()!=QDialog::Accepted)return;channels.clear();for(int i=0;i<68;++i)if(list.item(i)->checkState()==Qt::Checked)channels<<i;channelButton->setText(QString("Channels… (%1)").arg(channels.size()));selection.clear();});
  auto invalidate=[this]{selection.clear();canvas->Clear();};for(auto *field:{cobo,asad,aget})connect(field,QOverload<int>::of(&QSpinBox::valueChanged),this,[invalidate](int){invalidate();});connect(source,QOverload<int>::of(&QComboBox::currentIndexChanged),this,[invalidate](int){invalidate();});
  connect(save,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,"Export displayed waveform",{},"PNG (*.png);;CSV (*.csv)");if(path.isEmpty())return;QString error;if(path.endsWith(".png",Qt::CaseInsensitive))canvas->SavePng(path,&error);else{WaveformExportMetadata metadata;metadata.sourceFile=source->currentText();metadata.frame=QString::number(quint32(displayed));metadata.cobo=QString::number(cobo->value());metadata.asad=QString::number(asad->value());metadata.aget=aget->value();WriteWaveformCsv(path,canvas->Traces(),metadata,&error);}if(!error.isEmpty())status->setText(error);});
  auto *timer=new QTimer(this);connect(timer,&QTimer::timeout,this,[this]{Refresh();});timer->start(200);auto *choicesTimer=new QTimer(this);connect(choicesTimer,&QTimer::timeout,this,[this]{LoadChoices();});choicesTimer->start(2000);LoadChoices();
 }
 ~OnlineWindow(){QMetaObject::invokeMethod(receiver,"Stop",Qt::BlockingQueuedConnection);thread.quit();thread.wait();}
 void LoadChoices(){
  QJsonArray choices;if(!choicesPath.isEmpty()){QFile file(choicesPath);if(file.open(QIODevice::ReadOnly) && file.size()<1024*1024)choices=QJsonDocument::fromJson(file.readAll()).object()["sources"].toArray();}else for(const auto &endpoint:initialEndpoints)choices.append(QJsonObject{{"id",endpoint},{"label",endpoint},{"endpoint",endpoint}});
  if(choices==currentChoices && exporters->count())return;const auto previous=exporters->currentData().toString();currentChoices=choices;{QSignalBlocker block(exporters);exporters->clear();if(choices.size()>1)exporters->addItem("All configured DataExporters","*");for(const auto &entry:choices){const auto source=entry.toObject();exporters->addItem(source["label"].toString(),source["endpoint"]);}if(choices.isEmpty())exporters->addItem("No DataExporter in the current configuration",QString());int index=exporters->findData(previous);exporters->setCurrentIndex(index>=0?index:0);}UseExporter();
 }
 void UseExporter(){
  QStringList endpoints;if(!paused->isChecked()){const auto selected=exporters->currentData().toString();for(const auto &entry:currentChoices){const auto endpoint=entry.toObject()["endpoint"].toString();if(!endpoint.isEmpty() && (selected=="*" || selected==endpoint)){endpoints<<endpoint;}}}if(endpoints==activeEndpoints)return;activeEndpoints=endpoints;source->clear();for(const auto &endpoint:endpoints)source->addItem(endpoint);selection.clear();frozen.clear();canvas->Clear();QMetaObject::invokeMethod(receiver,"Connect",Qt::QueuedConnection,Q_ARG(QStringList,endpoints));
 }
 void Refresh(){
  status->setText(store->Statistics()+" | Preview only; cached events may be incomplete.");if(decoding)return;
  const auto events=store->Snapshot();const quint64 previous=event->currentData().toULongLong();{QSignalBlocker block(event);event->clear();for(const auto &e:events)event->addItem(QString("%1 (session %2)").arg(quint32(e.key)).arg(e.key>>32),QVariant::fromValue<qulonglong>(e.key));int index=event->findData(QVariant::fromValue<qulonglong>(previous));if(follow->isChecked())index=events.size()-1;if(!follow->isChecked() && !frozen.isEmpty() && index<0){event->addItem(QString("Frozen %1 (session %2)").arg(quint32(displayed)).arg(displayed>>32),QVariant::fromValue<qulonglong>(displayed));index=event->count()-1;}if(index>=0)event->setCurrentIndex(index);}
  const auto key=event->currentData().toULongLong();if(follow->isChecked() && events.isEmpty())frozen.clear();if(follow->isChecked() || frozen.isEmpty()){for(const auto &e:events)if(e.key==key){frozen=e.frames;displayed=key;break;}}
  for(const auto &frame:frozen)if(frame.source==source->currentText() && frame.record.cobo==cobo->value() && frame.record.asad==asad->value()){
   const auto signature=QString::number(displayed)+':'+source->currentText()+':'+QString::number(cobo->value())+':'+QString::number(asad->value())+':'+QString::number(aget->value())+':'+QString::number(channels.size())+':'+QString::number(frame.stamp)+':'+QString::number(qHash(frame.bytes));if(signature==selection)return;selection=signature;
   if(channels.isEmpty()){canvas->Clear();return;}decoding=true;auto *watcher=new QFutureWatcher<QPair<QVector<WaveformPlotTrace>,QString>>(this);const auto selectedChannels=channels;const int selectedAget=aget->value();
   connect(watcher,&QFutureWatcher<QPair<QVector<WaveformPlotTrace>,QString>>::finished,this,[this,watcher,signature]{auto result=watcher->result();watcher->deleteLater();decoding=false;if(signature!=selection)return;if(!result.second.isEmpty()){status->setText(result.second);return;}canvas->SetTraces(result.first,aget->value());});
   watcher->setFuture(QtConcurrent::run([frame,selectedChannels,selectedAget]{QVector<GrawChannelSamples> decoded;QString error;QVector<WaveformPlotTrace> traces;if(GrawFrameReader::ReadChannels(frame.bytes,frame.record,selectedAget,selectedChannels,&decoded,&error))for(const auto &channel:decoded){WaveformPlotTrace trace;trace.channel=channel.channel;for(const auto &sample:channel.samples)trace.samples<<QPointF(sample.bucket,sample.value);traces<<trace;}return qMakePair(traces,error);}));return;
  }
  canvas->Clear();selection.clear();status->setText(store->Statistics()+" | No matching cached frame for this event/source/CoBo/AsAd.");
 }
};
int main(int argc,char **argv){QApplication app(argc,argv);app.setApplicationName("getdaq-online-waveform-viewer");QCommandLineParser parser;parser.addHelpOption();parser.addOption({"endpoint","DataExporter IP:export-port; repeat for multiple sources.","address"});parser.addOption({"sources-file","Current GetDAQ DataExporter choices.","path"});parser.addOption({"screenshot","Capture initial standalone window without connecting.","path"});parser.process(app);OnlineWindow window(parser.values("endpoint"),parser.value("sources-file"));window.show();if(parser.isSet("screenshot"))QTimer::singleShot(300,&app,[&]{window.grab().save(parser.value("screenshot"));app.quit();});return app.exec();}
