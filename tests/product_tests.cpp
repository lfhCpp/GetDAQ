#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QSignalSpy>
#include "runconfig.h"
#include "runcontrol.h"
#include "runrecorder.h"
#include "protocol.h"
#include "mainwindow.h"
#include "ipwindow.h"
#include "getdaq_test_support_p.h"
#include "mocknode_fixture_p.h"
#include "waveformviewer_tests_p.h"
using namespace GetDAQTestSupport;
class ProductTests:public QObject {Q_OBJECT
private slots:
 void initTestCase(){QCoreApplication::setOrganizationName("GetDAQ-Product-Tests");QCoreApplication::setApplicationName("OriginalBackendBaseline");qRegisterMetaType<QVector<NodeStatus>>();}
 void yamlAndRetiredProfileGuard(){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(c.Save(dir.filePath("envoy.yml"),&error));QFile f(dir.filePath("envoy.yml"));QVERIFY(f.open(QIODevice::ReadOnly));const auto bytes=f.readAll();QVERIFY(!bytes.contains("native_ecc"));QVERIFY(!bytes.contains("simulation"));RunConfig loaded;QVERIFY(loaded.Load(f.fileName(),&error));QCOMPARE(loaded.nodes.size(),2);c.nativeEcc=true;QVERIFY(!c.Validate(&error));QVERIFY(error.contains("removed"));c.nativeEcc=false;c.simulation=true;QVERIFY(!c.Validate(&error));}
 void remoteReceiverProtocol(){NodeConfig node;node.id=8;node.host="192.0.2.16";node.routerHost="198.51.100.16";node.routerPort=46005;node.exporterPort=0;auto body=Protocol::EccRequest(node,"pedestals","Configure");QVERIFY(body.contains("DataRouter"));QVERIFY(body.contains("198.51.100.16"));QVERIFY(body.contains("46005"));QVERIFY(!body.contains("DataExporter"));QVERIFY(body.contains("CoBo[8]"));}
 void fullSoapLifecycle(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,2),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,3),3000);control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,4),3000);control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.PauseSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,6),3000);control.ResumeSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QCOMPARE(journal["phase"].toString(),QString("Complete"));control.DisconnectSlot();}
 void missingBackupIsWarning(){QTemporaryDir dir;auto c=Config(dir.path());RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),finished(&control,&RunControl::RunFinishedSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}control.StartRunSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);QVERIFY(QFile::remove(c.configDir+"/describe-"+c.nodes[0].ConfigId("describe",c.experiment)+".xcfg"));control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);QVERIFY(At(states,4));QVERIFY(QFileInfo::exists(c.RunDirectory()+"/configuration-backup-warning.json"));control.DisconnectSlot();}
 void sameRunRequiresExplicitReplacement(){QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));QVERIFY(!RunRecorder::Reserve(c,false,&error));QVERIFY(RunRecorder::Reserve(c,true,&error));QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));QVERIFY(!journal["replacement_backup"].toString().isEmpty());}
 void originalWindowAndNodeOptions(){MainWindow window;auto *table=window.findChild<QTableWidget *>("StatusTable");QVERIFY(table);QCOMPARE(table->columnCount(),13);for(auto *action:window.findChildren<QAction *>())QVERIFY2(!action->objectName().startsWith("Native"),qPrintable(action->objectName()));auto c=RunConfig::Default();IPWindow nodes(c);auto *settings=nodes.findChild<QTableWidget *>("NodeSettingsTable");QVERIFY(settings);QCOMPARE(settings->columnCount(),14);QVERIFY(!nodes.findChild<QCheckBox *>("UseNativeWholeSystemControl"));for(const auto &name:{"LoadConfig","SaveConfig","Nodes"}){auto *b=window.findChild<QPushButton *>(name);QVERIFY(b);QCOMPARE(b->width(),172);}}
 void waveformRawFrames(){GetDAQTestInternal::RunWaveformViewerIndexesRawAndLayeredFrames();}
 void waveformCompactFrames(){GetDAQTestInternal::RunWaveformViewerDecodesCompactCoboFrames();}
 void waveformTruncation(){GetDAQTestInternal::RunWaveformViewerRejectsTruncatedMfmInput();}
 void waveformChannelExport(){GetDAQTestInternal::RunWaveformViewerExportsSelectedSamplesAsCsv();}
 void waveformNavigation(){GetDAQTestInternal::RunWaveformViewerNavigatesAndWrapsFrames();}
 void waveformPulseMetrics(){GetDAQTestInternal::RunWaveformViewerComputesLegacyPulseMetrics();}
};
QTEST_MAIN(ProductTests)
#include "product_tests.moc"
