#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QAction>
#include <QCheckBox>
#include <QTableWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QMessageBox>
#include <QTimer>
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
 void newRunWithIncomingFilesRequiresSeparateConfirmation(){
    QTemporaryDir dir;auto c=Config(dir.path());
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())
        if(mock->serverPort()==c.nodes[0].eccPort)mock->generatedBytes=1234;
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal),confirmation(&control,&RunControl::IncomingFilesConfirmationSignal),finished(&control,&RunControl::RunFinishedSignal);
    control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);
    for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}
    control.StartRunSlot();QCOMPARE(confirmation.size(),1);QVERIFY(!confirmation[0][0].toBool());QVERIFY(confirmation[0][1].toString().contains("/incoming"));
    QVERIFY(!QFileInfo::exists(c.RunDirectory()));QVERIFY(At(states,4));
    control.StartRunSlot(false,true);QTRY_VERIFY_WITH_TIMEOUT(At(states,5),3000);
    QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(c,&journal,&error));
    QVERIFY(journal["existing_incoming_acknowledged"].toBool());QVERIFY(!journal.contains("replacement_operation_id"));QVERIFY(!journal.contains("replacement_backup"));QVERIFY(!journal.contains("same_run_existing_incoming_acknowledged"));
    control.StopRunSlot();QTRY_VERIFY_WITH_TIMEOUT(finished.size()==1,7000);control.DisconnectSlot();
 }
 void acceptingIncomingDoesNotAuthorizeRunReplacement(){
    QTemporaryDir dir;auto c=Config(dir.path());QString error;QVERIFY(RunRecorder::Reserve(c,&error));
    RunControl control;QSignalSpy states(&control,&RunControl::StatusSignal);control.ConnectSlot(c);QTRY_VERIFY_WITH_TIMEOUT(At(states,1),3000);
    for(int s=2;s<=4;++s){control.ProgressSlot();QTRY_VERIFY_WITH_TIMEOUT(At(states,s),3000);}
    control.StartRunSlot(false,true);QVERIFY(At(states,4));QVERIFY(QFileInfo::exists(c.RunDirectory()+"/journal.json"));
    control.DisconnectSlot();
 }
 void incomingFilesDialogCanCancelAndThenStart(){
    QTemporaryDir dir;auto c=Config(dir.path());
    for(auto *mock:QCoreApplication::instance()->findChildren<GetDAQTestFixtures::MockNode *>())
        if(mock->serverPort()==c.nodes[0].eccPort)mock->generatedBytes=1234;
    MainWindow window;auto *table=window.findChild<QTableWidget *>("StatusTable");QVERIFY(table);
    QSignalSpy starts(&window,&MainWindow::StartSignal);window.ConnectSignal(c);
    QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Idle"),3000);
    for(const auto &state:{"Described","Prepared","Ready"}){window.ProgressSignal(-1);QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString(state),3000);}
    bool accept=false;int dialogs=0;QTimer click;
    connect(&click,&QTimer::timeout,&window,[&]{
        auto *dialog=window.findChild<QMessageBox *>("IncomingFilesConfirmation");if(!dialog || !dialog->isVisible())return;
        ++dialogs;QVERIFY(dialog->text().contains("does not mean"));
        if(!accept){dialog->reject();return;}
        for(auto *button:dialog->buttons())if(button->text()=="Start and retain files"){button->click();return;}
    });click.start(10);
    window.StartSignal(false,false);QTRY_COMPARE_WITH_TIMEOUT(dialogs,1,3000);
    QCOMPARE(starts.size(),1);QVERIFY(!QFileInfo::exists(c.RunDirectory()));
    accept=true;window.StartSignal(false,false);QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Running"),3000);
    QCOMPARE(dialogs,2);QCOMPARE(starts.size(),3);QVERIFY(starts.last()[1].toBool());
    window.StopSignal();QTRY_COMPARE_WITH_TIMEOUT(table->item(0,1)->text(),QString("Ready"),3000);
    QTRY_VERIFY_WITH_TIMEOUT(QFileInfo::exists(c.RunDirectory()+"/run.csv"),7000);window.DisconnectSignal();
 }
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
