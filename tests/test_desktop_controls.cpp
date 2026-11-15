#include "localserviceswindow.h"
#include "waveformwindow_p.h"
#include "waveformcanvas.h"
#include "waveformviewer_testfixtures_p.h"
#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QListWidget>
#include <QLabel>
#include <QTimer>
#include <QDirIterator>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <csignal>
using namespace GetDAQTestInternal;
class DesktopControlsTests:public QObject {
    Q_OBJECT
    void Channels(GetDAQ::detail::WaveformWindow &viewer,const QVector<int> &selected,bool accept=true) {
        QTimer::singleShot(0,&viewer,[&viewer,selected,accept]{
            auto *dialog=viewer.findChild<QDialog *>("ChannelSelectionDialog");QVERIFY(dialog);
            auto *list=dialog->findChild<QListWidget *>("WaveformChannelList");QVERIFY(list);
            for(int i=0;i<list->count();++i)list->item(i)->setCheckState(selected.contains(i)?Qt::Checked:Qt::Unchecked);
            if(accept)dialog->accept();else dialog->reject();
        });
        viewer.findChild<QPushButton *>("WaveformChannels")->click();
    }
private slots:
    void realEventIdsDisambiguateBoardsAndAllowMissingAndMaxIds() {
        QTemporaryDir temp;QVERIFY(temp.isValid());auto one=MakeType1Frame(),two=MakeType1Frame(),maximum=MakeType1Frame();two[26]=3;Put32BigEndian(&maximum,22,0xffffffffU);
        const QString file=temp.filePath("events.graw");QVERIFY(WriteWaveformFixture(file,one+two+maximum));
        GetDAQ::detail::WaveformWindow viewer;viewer.LoadFile(file);
        auto *event=viewer.findChild<QLineEdit *>("WaveformEvent");auto *go=viewer.findChild<QPushButton *>("WaveformFindEvent");auto *frames=viewer.findChild<QComboBox *>("WaveformEventFrames");auto *frame=viewer.findChild<QSpinBox *>("WaveformFrame");
        QTRY_VERIFY_WITH_TIMEOUT(go->isEnabled(),10000);QCOMPARE(event->text(),QString("1234"));
        event->setText("1234");go->click();QCOMPARE(frames->count(),2);frames->setCurrentIndex(1);QCOMPARE(frame->value(),1);QVERIFY(frames->currentText().contains("CoBo 3"));
        event->setText("999");go->click();QCOMPARE(frame->value(),1);QVERIFY(viewer.statusBar()->currentMessage().contains("not found"));
        event->setText("4294967295");go->click();QCOMPARE(frame->value(),2);QCOMPARE(event->text(),QString("4294967295"));
        event->setText("4294967296");go->click();QCOMPARE(frame->value(),2);QVERIFY(viewer.statusBar()->currentMessage().contains("0..4294967295"));
        if(!qEnvironmentVariableIsEmpty("GETDAQ_DESKTOP_SCREENSHOTS")){event->setText("1234");go->click();viewer.findChild<QSpinBox *>("WaveformAget")->setValue(2);viewer.show();QTest::qWait(100);viewer.grab().save(qEnvironmentVariable("GETDAQ_DESKTOP_SCREENSHOTS")+"/waveform.png");}
    }
    void channelDialogControlsRenderedAndExportedTraces() {
        QTemporaryDir temp;const auto file=temp.filePath("channels.graw");QVERIFY(WriteWaveformFixture(file,MakeType1Frame()));
        GetDAQ::detail::WaveformWindow viewer;viewer.LoadFile(file);auto *aget=viewer.findChild<QSpinBox *>("WaveformAget");
        QTRY_VERIFY_WITH_TIMEOUT(viewer.findChild<QPushButton *>("WaveformFindEvent")->isEnabled(),10000);aget->setValue(2);
        auto *canvas=viewer.findChild<GetDAQ::WaveformCanvas *>("WaveformCanvas");
        Channels(viewer,{});QVERIFY(canvas->Traces().isEmpty());
        Channels(viewer,{7});QCOMPARE(canvas->Traces().size(),1);QCOMPARE(canvas->Traces()[0].channel,7);
        Channels(viewer,{7,8});QCOMPARE(canvas->Traces().size(),2);
        Channels(viewer,{8});QCOMPARE(canvas->Traces().size(),1);QCOMPARE(canvas->Traces()[0].channel,8);
        const auto &ordered=canvas->Traces()[0].samples;for(int i=1;i<ordered.size();++i)QVERIFY(ordered[i-1].x()<=ordered[i].x());
        GetDAQ::WaveformExportMetadata metadata;QString error;const QString csv=temp.filePath("selected.csv");
        QVERIFY2(GetDAQ::WriteWaveformCsv(csv,canvas->Traces(),metadata,&error),qPrintable(error));QFile exported(csv);QVERIFY(exported.open(QIODevice::ReadOnly));
        const auto rows=exported.readAll().split('\n');int samples=0;for(int i=1;i<rows.size();++i)if(!rows[i].trimmed().isEmpty()){QCOMPARE(rows[i].split(',')[5],QByteArray("8"));++samples;}QCOMPARE(samples,512);
        Channels(viewer,{7},false);QCOMPARE(canvas->Traces().size(),1);QCOMPARE(canvas->Traces()[0].channel,8);
        QVector<int> all;for(int i=0;i<68;++i)all.append(i);Channels(viewer,all);QVERIFY(canvas->Traces().size()>=2);
    }
    void axesHaveBucketAndFullAdcRanges() {
        GetDAQ::WaveformCanvas canvas;canvas.resize(920,520);GetDAQ::WaveformPlotTrace trace;trace.channel=7;trace.samples={QPointF(0,0),QPointF(511,4095)};canvas.SetTraces({trace},0);canvas.show();
        QTRY_VERIFY(canvas.accessibleDescription().contains("0 to 511"));
        QVERIFY(canvas.accessibleDescription().contains("-327.6"));QVERIFY(canvas.accessibleDescription().contains("4422.6"));
        QString error;QTemporaryDir temp;QVERIFY(canvas.SavePng(temp.filePath("axes.png"),&error));
    }
    void gmatUsesFrameIndexWithoutInventedEventIds() {
        QTemporaryDir temp;const auto file=temp.filePath("matrix.gmat");QVERIFY(WriteWaveformFixture(file,QByteArray(4*68*512*8*2,'\0')));
        GetDAQ::detail::WaveformWindow viewer;viewer.LoadFile(file);QVERIFY(!viewer.findChild<QPushButton *>("WaveformFindEvent")->isEnabled());
        auto *frame=viewer.findChild<QSpinBox *>("WaveformFrame");QCOMPARE(frame->maximum(),1);frame->setValue(1);QCOMPARE(frame->value(),1);
    }
    void receiverRemainsRunningAfterLauncherCloses() {
#ifndef Q_OS_LINUX
        QSKIP("Linux standalone services");
#else
        const auto bin=qEnvironmentVariable("GETDAQ_SETUP_BIN");if(bin.isEmpty())QSKIP("Set GETDAQ_SETUP_BIN to installed programs");
        QTemporaryDir temp;QVERIFY(QDir().mkdir(temp.filePath("raw")));
        QTcpServer data,status;QVERIFY(data.listen(QHostAddress::LocalHost,0));QVERIFY(status.listen(QHostAddress::LocalHost,0));const int dataPort=data.serverPort(),statusPort=status.serverPort();data.close();status.close();
        auto *launcher=new LocalServicesWindow(RunConfig::Default());launcher->findChild<QComboBox *>("ServiceMode")->setCurrentIndex(2);
        launcher->findChild<QLineEdit *>("ServicePrograms")->setText(bin);launcher->findChild<QLineEdit *>("ServiceLogs")->setText(temp.filePath("logs"));launcher->findChild<QLineEdit *>("ServiceOutput")->setText(temp.filePath("raw"));
        launcher->findChild<QSpinBox *>("ServiceDataPort")->setValue(dataPort);launcher->findChild<QSpinBox *>("ServiceStatusPort")->setValue(statusPort);
        auto *start=launcher->findChild<QPushButton *>("StartIndependentService");start->click();QVERIFY(launcher->findChild<QPlainTextEdit *>("ServiceLaunchLog")->toPlainText().contains("Confirm"));
        launcher->findChild<QCheckBox *>("ServiceStandalone")->setChecked(true);start->click();
        const QString log=launcher->findChild<QPlainTextEdit *>("ServiceLaunchLog")->toPlainText();QVERIFY2(log.contains("Independent process launched"),qPrintable(log));
        QDirIterator it(temp.filePath("logs"),{"process.json"},QDir::Files,QDirIterator::Subdirectories);QVERIFY(it.hasNext());QFile receipt(it.next());QVERIFY(receipt.open(QIODevice::ReadOnly));const int pid=QJsonDocument::fromJson(receipt.readAll()).object()["pid"].toInt();QVERIFY(pid>0);
        struct Stop {int pid;~Stop(){::kill(pid,SIGTERM);QTest::qWait(300);}} stop{pid};
        delete launcher;QVERIFY(::kill(pid,0)==0);
        QTcpSocket socket;QElapsedTimer deadline;deadline.start();bool connected=false;
        while(!connected && deadline.elapsed()<5000){socket.abort();socket.connectToHost(QHostAddress::LocalHost,quint16(statusPort));connected=socket.waitForConnected(100);if(!connected)QTest::qWait(50);}
        QVERIFY(connected);socket.write("GET /status HTTP/1.0\r\n\r\n");QVERIFY(socket.waitForBytesWritten(1000));QVERIFY(socket.waitForReadyRead(2000));QVERIFY(socket.readAll().contains("200"));
        QDirIterator raw(temp.filePath("raw"),{"*.graw"},QDir::Files,QDirIterator::Subdirectories);QVERIFY(!raw.hasNext());
#endif
    }
};
QTEST_MAIN(DesktopControlsTests)
#include "test_desktop_controls.moc"
