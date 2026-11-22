// Exercise the standalone viewer's real controls and TCP receiving path.
#define main GetDAQOnlineViewerEntryPoint
#include "../apps/onlinewaveform/main.cpp"
#undef main
#include "waveformviewer_testfixtures_p.h"
#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtEndian>
class OnlineControlsTest:public QObject {
 Q_OBJECT
private slots:
 void intervalFreezeAndLatestMatching(){
    QTemporaryDir dir;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,dir.path());QCoreApplication::setOrganizationName("GetDAQ-Tests");QCoreApplication::setApplicationName("OnlineControls");
    QTcpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));const auto endpoint=QString("127.0.0.1:%1").arg(server.serverPort());int connections=0;connect(&server,&QTcpServer::newConnection,this,[&]{++connections;});
    {OnlineWindow window({endpoint},QString());window.show();auto *period=window.findChild<QSpinBox *>("OnlineRefreshInterval");auto *timer=window.findChild<QTimer *>("OnlineDisplayTimer");auto *events=window.findChild<QComboBox *>("OnlineEventSelection");auto *follow=window.findChild<QCheckBox *>("OnlineFollowLatest");GetDAQ::WaveformCanvas *canvas=nullptr;for(auto *widget:window.findChildren<QWidget *>())if(auto *plot=dynamic_cast<GetDAQ::WaveformCanvas *>(widget))canvas=plot;QVERIFY(period && timer && events && follow && canvas);QCOMPARE(period->value(),1000);QCOMPARE(timer->interval(),1000);period->setValue(100);QCOMPARE(timer->interval(),100);window.findChild<QSpinBox *>("OnlineAgetSelection")->setValue(2);
     QTRY_VERIFY_WITH_TIMEOUT(server.hasPendingConnections(),3000);auto *peer=server.nextPendingConnection();
     auto send=[&](int event,int board=0){auto frame=GetDAQTestInternal::MakeType1Frame();frame[26]=char(board);frame[27]=0;GetDAQTestInternal::Put32BigEndian(&frame,22,event);QByteArray header(8,0);qToLittleEndian<quint64>(quint64(frame.size()),reinterpret_cast<uchar *>(header.data()));peer->write(header+frame);peer->flush();};
     send(1234);QTRY_COMPARE_WITH_TIMEOUT(quint32(events->currentData().toULongLong()),quint32(1234),3000);QTRY_VERIFY_WITH_TIMEOUT(!canvas->Traces().isEmpty(),3000);
     follow->setChecked(false);for(int i=1235;i<1355;++i)send(i);QTest::qWait(500);QCOMPARE(quint32(events->currentData().toULongLong()),quint32(1234));QVERIFY(!canvas->Traces().isEmpty());
     follow->setChecked(true);QTRY_COMPARE_WITH_TIMEOUT(quint32(events->currentData().toULongLong()),quint32(1354),3000);send(1355,1);QTest::qWait(300);QCOMPARE(quint32(events->currentData().toULongLong()),quint32(1354));QCOMPARE(connections,1);
     period->setValue(2500);QCOMPARE(timer->interval(),2500);QCOMPARE(QSettings().value("display/refresh-ms").toInt(),2500);
    }
    OnlineWindow reopened({},QString());QCOMPARE(reopened.findChild<QSpinBox *>("OnlineRefreshInterval")->value(),2500);QCOMPARE(reopened.findChild<QTimer *>("OnlineDisplayTimer")->interval(),2500);
 }
};
QTEST_MAIN(OnlineControlsTest)
#include "online_waveform_controls_test.moc"
