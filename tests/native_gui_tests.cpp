#include <limits>
#include "native_gui_tests_p.h"

#include <QtTest>
#include <QTemporaryDir>
#include <QFrame>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QComboBox>
#include <QToolButton>
#include <QLayout>
#include <QTableWidget>
#include <QHeaderView>
#include <QCheckBox>
#include <QScrollArea>
#include <QAction>
#include <QMenuBar>
#include <QMessageBox>
#include <QTimer>
#include <QSignalSpy>
#include <QColor>

#include "mainwindow.h"
#include "ipwindow.h"
#include "widgetplot2d.h"
#include "cobomaintenancewindow.h"
#include "cobo_maintenance_fixture.h"
#include "plan_transition_fixture.h"
#include "runconfig.h"
#include "getdaq_test_support_p.h"

namespace GetDAQTestInternal {
void RunNativeGuiMetadataAndWholeSystemControls()
{
    QTemporaryDir dir; RunConfig c=GetDAQTestSupport::Config(dir.path()); c.simulation=false; c.nativeEcc=true;
    c.nodes.removeLast(); c.nodes.first().name="CoBo0";c.nodes.first().nativeSenderId="C0"; c.nodes.first().nativeAgentId="A0";
    auto &client=c.nativeClient; client.address="127.0.0.1"; client.port=24000; client.serviceId="ecc"; client.clientId="console";
    client.clientEpoch=QString(64,'a'); client.snapshotSha256=QString(64,'b'); client.inputBindingSha256=QString(64,'c'); client.credentialFile=dir.filePath("credential"); c.nativeRecordDir=dir.filePath("records");
    client.sourceStatusEnabled=true;client.receiverFailureStatusEnabled=true;client.alarmStatusEnabled=true;
    MainWindow window; window.SetConfig(c); window.show();
    QVERIFY(window.styleSheet().contains("#StatusSectionTitle"));
    QVERIFY(window.styleSheet().contains("#f4f7f9"));
    QVERIFY(window.styleSheet().contains("#087f5b"));
    QVERIFY(window.findChild<QFrame *>("HeaderPanel"));
    QVERIFY(window.findChild<QFrame *>("SetupPanel"));
    QVERIFY(!window.findChild<QFrame *>("RunDetailsPanel"));
    QVERIFY(window.findChild<QFrame *>("SetupPanel")->isAncestorOf(window.findChild<QLineEdit *>("Experiment")));
    QVERIFY(window.findChild<QFrame *>("RunPanel"));
    auto *setupPanel=window.findChild<QFrame *>("SetupPanel");
    auto *openYaml=window.findChild<QPushButton *>("LoadConfig");
    auto *saveYaml=window.findChild<QPushButton *>("SaveConfig");
    auto *nodesPaths=window.findChild<QPushButton *>("Nodes");
    QVERIFY(setupPanel && openYaml && saveYaml && nodesPaths);
    QVERIFY(openYaml->width()>=120);
    QVERIFY(qAbs(openYaml->width()-saveYaml->width())<=3);
    QVERIFY(qAbs(saveYaml->width()-nodesPaths->width())<=3);
    auto *configStatus=window.findChild<QLabel *>("ConfigPath");
    QVERIFY(configStatus);
    const auto *setupInputs=window.findChild<QWidget *>("SetupInputs");QVERIFY(setupInputs);
    const int statusRight=configStatus->mapTo(setupInputs,QPoint(configStatus->width(),0)).x();
    QVERIFY(setupInputs->width()-statusRight<=14);
    QVERIFY(window.menuBar()->isHidden());
    QCOMPARE(window.menuBar()->actions().size(),0);
    const QList<QPair<QString,QString>> menuControls={
        {QStringLiteral("MenuConfiguration"),QStringLiteral("Configuration")},
        {QStringLiteral("MenuTools"),QStringLiteral("Tools")},
        {QStringLiteral("MenuRun"),QStringLiteral("Run")},
        {QStringLiteral("MenuRecovery"),QStringLiteral("Recovery and safety")}
    };
    QToolButton *firstMenuButton=nullptr;
    QHBoxLayout *menuLayout=window.findChild<QHBoxLayout *>("HeaderMenuLayout");
    QVERIFY(menuLayout);
    for(const auto &item:menuControls) {
        auto *button=window.findChild<QToolButton *>(item.first);
        QVERIFY(button && button->menu());
        QCOMPARE(button->menu()->title(),item.second);
        QCOMPARE(button->text(),item.first==QStringLiteral("MenuRecovery")?QStringLiteral("Recovery"):item.second);
        QVERIFY(menuLayout->indexOf(button)>=0);
        if(!firstMenuButton)firstMenuButton=button;
    }
    auto *runStart=window.findChild<QPushButton *>("StartRun");
    auto *runStop=window.findChild<QPushButton *>("StopRun");
    auto *recoverControl=window.findChild<QPushButton *>("Recover");
    auto *acquisitionPanel=window.findChild<QFrame *>("RunPanel");
    auto *menuPanel=window.findChild<QWidget *>("HeaderMenuPanel");
    auto *title=window.findChild<QLabel *>("Title");
    auto *mode=window.findChild<QLabel *>("Mode");
    QVERIFY(!window.findChild<QLabel *>("ConnectionIndicator"));
    auto *connectionBadge=window.findChild<QFrame *>("ConnectionStatusBadge");
    auto *phaseLabel=window.findChild<QLabel *>("Phase");
    auto *runControls=window.findChild<QLayout *>("Controls");
    QVERIFY(runStart && runStop && recoverControl && acquisitionPanel && firstMenuButton && menuPanel && title && mode && runControls);
    QVERIFY(recoverControl->mapTo(acquisitionPanel,QPoint(0,recoverControl->height())).y() <= acquisitionPanel->height()-1);
    QVERIFY(connectionBadge && phaseLabel);
    QCOMPARE(phaseLabel->parentWidget(),connectionBadge);
    QCOMPARE(runStart->text(),QStringLiteral("Run start"));
    QCOMPARE(runStop->text(),QStringLiteral("Run stop"));
    QVERIFY(menuPanel->width()>=260);
    QCOMPARE(firstMenuButton->parentWidget(),menuPanel);
    QVERIFY(firstMenuButton->mapTo(window.centralWidget(),QPoint(0,0)).x()
            > title->mapTo(window.centralWidget(),QPoint(title->width(),0)).x());
    QVERIFY(menuPanel->mapTo(window.centralWidget(),QPoint(menuPanel->width(),0)).x()
            < mode->mapTo(window.centralWidget(),QPoint(0,0)).x());
    QVERIFY(qAbs(firstMenuButton->mapTo(window.centralWidget(),QPoint(0,firstMenuButton->height()/2)).y()
                 -title->mapTo(window.centralWidget(),QPoint(0,title->height()/2)).y())<8);
    QVERIFY(runControls->indexOf(firstMenuButton)<0);
    QVERIFY(window.findChild<QLabel *>("StatusSectionTitle")->toolTip().contains("whole system"));
    auto initialTable=window.findChild<QTableWidget *>("StatusTable");
    QVERIFY(initialTable->horizontalHeaderItem(1)->toolTip().contains("CoBo lifecycle"));
    QCOMPARE(initialTable->horizontalHeaderItem(1)->text(),QStringLiteral("ECC"));
    QVERIFY(initialTable->horizontalHeaderItem(2)->toolTip().contains("whole ECC system"));
    QCOMPARE(initialTable->horizontalHeaderItem(2)->text(),QStringLiteral("Transition"));
    QCOMPARE(initialTable->columnWidth(2),90);
    QCOMPARE(TransitionName(0),QStringLiteral("Stable"));
    QCOMPARE(TransitionName(3),QStringLiteral("Configuring"));
    QCOMPARE(TransitionName(8),QStringLiteral("Breaking up"));
    QCOMPARE(TransitionName(99),QStringLiteral("Unknown (99)"));
    QVERIFY(initialTable->alternatingRowColors());
    QCOMPARE(initialTable->horizontalHeader()->sectionResizeMode(0),QHeaderView::Interactive);
    QVERIFY(initialTable->columnWidth(0)<200);
    QVERIFY(!initialTable->wordWrap());
    QVERIFY(window.findChild<WidgetPlot2D *>()->isVisible());
    QCOMPARE(initialTable->horizontalHeader()->logicalIndex(0),initialTable->columnCount()-1);
    QCOMPARE(initialTable->horizontalHeader()->logicalIndex(1),0);
    QVERIFY(window.findChild<QLabel *>("Mode")->text()=="HARDWARE");
    QVERIFY(!window.findChild<QCheckBox *>("Simulation")->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Native monitoring"),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
    QCOMPARE(window.findChild<QLabel *>("Phase")->property("state").toString(),QStringLiteral("idle"));
    QCOMPARE(connectionBadge->property("state").toString(),QStringLiteral("idle"));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Native requires inspection"),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
    QCOMPARE(window.findChild<QLabel *>("Phase")->property("state").toString(),QStringLiteral("error"));
    QCOMPARE(connectionBadge->property("state").toString(),QStringLiteral("error"));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Native monitoring"),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,false)));
    QCOMPARE(connectionBadge->property("state").toString(),QStringLiteral("busy"));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Native monitoring"),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
    QCOMPARE(window.findChild<QPushButton *>("NodeProgress")->text(),QString("Progress selected node"));
    QVERIFY(!window.findChild<QPushButton *>("NodeProgress")->isEnabled()); // no live Prepared eligibility/target observation yet
    QVERIFY(window.findChild<QPushButton *>("NodeRegress")->isVisible());
    QVERIFY(!window.findChild<QCheckBox *>("Simulation")->isEnabled()); QVERIFY(window.findChild<QPushButton *>("Progress")->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowCoBoMaintenanceStateSlot",Qt::DirectConnection,
        Q_ARG(bool,true),Q_ARG(QByteArray,CoBoMaintenanceFixture::Status()),Q_ARG(QByteArray,PlanTransitionFixture::Identity()),Q_ARG(QString,QString("Prepared"))));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowCoBoMaintenanceTargetsSlot",Qt::DirectConnection,
        Q_ARG(QByteArray,CoBoMaintenanceFixture::Targets()),Q_ARG(QString,QString())));
    QVERIFY(!runStart->isEnabled()); // Connecting an uninitialized ECC cannot start acquisition.
    auto statusTable=window.findChild<QTableWidget *>("StatusTable");statusTable->selectRow(0);
    QVERIFY(!window.findChild<QPushButton *>("NodeProgress")->isEnabled());
    QSignalSpy maintenanceInspection(&window,&MainWindow::InspectCoBoMaintenanceTargetsSignal);
    window.findChild<QAction *>("NativeCoBoMaintenance")->trigger();
    auto maintenanceWindow=window.findChild<CoBoMaintenanceWindow *>("NativeCoBoMaintenanceWindow");
    QVERIFY(maintenanceWindow && maintenanceWindow->isVisible());
    QCOMPARE(maintenanceWindow->findChild<QComboBox *>("Target")->currentText(),QString("C0"));
    QCOMPARE(maintenanceInspection.size(),1);maintenanceWindow->hide();
    auto stoppedAlarm=window.findChild<QAction *>("NativeStoppedAlarmRelease");QVERIFY(stoppedAlarm && stoppedAlarm->isEnabled());
    QSignalSpy stoppedAlarmRequests(&window,&MainWindow::ReleaseStoppedAlarmSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});stoppedAlarm->trigger();QCOMPARE(stoppedAlarmRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});stoppedAlarm->trigger();QCOMPARE(stoppedAlarmRequests.size(),1);
    NodeStatus row; row.id=c.nodes.first().id; row.eccOnline=true; row.state=4; row.nativeReceivedBytes="18446744073709551615"; row.nativeFrames="9007199254740993"; row.nativeFiles="9007199254740993";
    row.process="ready (cached)"; row.nativePid=42; row.nativeSessionId=QString(64,'d'); row.dataPath="/fixture-data/session";
    row.nativeSourceState="unknown";row.nativeSourceError="Fixture source fault";row.nativeSourceObserved=row.nativeSourceFault=true;
    row.nativeReceiverFailurePhase="complete";row.nativeReceiverFailureOperation="internal-receiver-fixture";row.nativeReceiverFailureDetail="C0 exit 7; data and Run require inspection";row.nativeReceiverFailureObserved=row.nativeReceiverFailureHeld=true;
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{row})));
    const auto table=window.findChild<QTableWidget *>("StatusTable"); QCOMPARE(table->item(0,5)->text(),row.nativeFiles); QCOMPARE(table->item(0,6)->text(),QString::number(double(std::numeric_limits<quint64>::max())/1e9,'f',3)); QVERIFY(table->item(0,6)->toolTip().contains(row.nativeReceivedBytes)); QVERIFY(table->item(0,4)->toolTip().contains(row.nativeFrames)); QCOMPARE(table->item(0,7)->text(),QString("—"));
    QVERIFY(runStart->isEnabled());
    QCOMPARE(table->item(0,1)->foreground().color(),QColor("#087f5b"));
    QCOMPARE(table->item(0,1)->background().style(),Qt::NoBrush);
    QVERIFY(table->item(0,1)->toolTip().contains(QStringLiteral("Ready")));
    row.transition=3;
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{row})));
    QCOMPARE(table->item(0,2)->foreground().color(),QColor("#996910"));
    QCOMPARE(table->item(0,2)->background().style(),Qt::NoBrush);
    QVERIFY(table->item(0,2)->toolTip().contains(QStringLiteral("whole ECC system")));
    row.transition=0;
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{row})));
    QCOMPARE(table->item(0,2)->foreground().color(),QColor("#617383"));
    QCOMPARE(table->item(0,2)->background().style(),Qt::NoBrush);
    QVERIFY(table->item(0,2)->toolTip().contains(QStringLiteral("whole ECC system")));
    QCOMPARE(table->columnCount(),13);
    QVERIFY(table->item(0,11)->text().contains(row.nativeSourceError));
    QVERIFY(table->item(0,11)->toolTip().contains(row.nativeReceiverFailureOperation));
    QCOMPARE(table->item(0,11)->foreground().color(),QColor("#b53840"));
    QCOMPARE(table->item(0,10)->toolTip(),row.dataPath);
    const QStringList originalHeaders={"Node","ECC","Transition","Sentry","Process","Files","Data GB","MB/s","Disk %","Free GB","Data path","Error",""};
    for(int column=0;column<originalHeaders.size();++column)QCOMPARE(table->horizontalHeaderItem(column)->text(),originalHeaders[column]);
    row.nativeSourceState="Source not observed";row.nativeSourceError="Source observation missing or stale";row.nativeSourceObserved=row.nativeSourceFault=false;
    row.nativeReceiverFailurePhase="Not observed";row.nativeReceiverFailureOperation.clear();row.nativeReceiverFailureDetail="Receiver cache stale";row.nativeReceiverFailureObserved=row.nativeReceiverFailureHeld=false;
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{row})));
    QVERIFY(table->item(0,11)->toolTip().contains(row.nativeSourceState));
    QVERIFY(table->item(0,11)->toolTip().contains(row.nativeReceiverFailurePhase));
    QVERIFY(!table->item(0,11)->toolTip().contains("internal-receiver-fixture"));
    auto cleanup=window.findChild<QAction *>("NativeReceiverAbort");QVERIFY(cleanup&&cleanup->isVisible()&&!cleanup->isEnabled());
    row.nativeHardwareOutcomeUnknown=true;QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,QVector<NodeStatus>{row})));QVERIFY(cleanup->isEnabled());
    QSignalSpy cleanupRequests(&window,&MainWindow::AbortFailedReceiversSignal);
    QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});cleanup->trigger();QCOMPARE(cleanupRequests.size(),0);
    QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});cleanup->trigger();QCOMPARE(cleanupRequests.size(),1);
    auto recovery=window.findChild<QAction *>("NativeDescriptionRecovery");QVERIFY(recovery && recovery->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Busy")),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,false)));QVERIFY(!recovery->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,QString("Inspection")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));QVERIFY(recovery->isEnabled());
    QSignalSpy recoveryRequests(&window,&MainWindow::ReconcileDescriptionSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});recovery->trigger();QCOMPARE(recoveryRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});recovery->trigger();QCOMPARE(recoveryRequests.size(),1);
    auto failureBreakup=window.findChild<QAction *>("NativeFailureBreakup");QVERIFY(failureBreakup && failureBreakup->isEnabled());
    QSignalSpy failureBreakupRequests(&window,&MainWindow::BreakupAfterFailureSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});failureBreakup->trigger();QCOMPARE(failureBreakupRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});failureBreakup->trigger();QCOMPARE(failureBreakupRequests.size(),1);
    auto retirement=window.findChild<QAction *>("NativeReceiverRetirement");QVERIFY(retirement && retirement->isEnabled());
    QSignalSpy retirementRequests(&window,&MainWindow::RetireFailedReceiversSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});retirement->trigger();QCOMPARE(retirementRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});retirement->trigger();QCOMPARE(retirementRequests.size(),1);
    auto acquisitionOff=window.findChild<QAction *>("NativeAcquisitionUnprepare");QVERIFY(acquisitionOff && acquisitionOff->isEnabled());
    QSignalSpy acquisitionOffRequests(&window,&MainWindow::UnprepareAcquisitionAfterFailureSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});acquisitionOff->trigger();QCOMPARE(acquisitionOffRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});acquisitionOff->trigger();QCOMPARE(acquisitionOffRequests.size(),1);
    auto acquisitionRecovery=window.findChild<QAction *>("NativeAcquisitionRecovery");QVERIFY(acquisitionRecovery && acquisitionRecovery->isEnabled());
    QSignalSpy acquisitionRecoveryRequests(&window,&MainWindow::ReconcileAcquisitionSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});acquisitionRecovery->trigger();QCOMPARE(acquisitionRecoveryRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});acquisitionRecovery->trigger();QCOMPARE(acquisitionRecoveryRequests.size(),1);
    auto alarmRecovery=window.findChild<QAction *>("NativeAlarmAcquisitionRecovery");QVERIFY(alarmRecovery && alarmRecovery->isEnabled());
    QSignalSpy alarmRecoveryRequests(&window,&MainWindow::ReconcileAcquisitionWithAlarmSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});alarmRecovery->trigger();QCOMPARE(alarmRecoveryRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});alarmRecovery->trigger();QCOMPARE(alarmRecoveryRequests.size(),1);
    auto alarmPrepare=window.findChild<QAction *>("NativeAlarmPreparationRecovery");QVERIFY(alarmPrepare && alarmPrepare->isEnabled());
    QSignalSpy alarmPrepareRequests(&window,&MainWindow::ReconcilePreparationWithAlarmSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});alarmPrepare->trigger();QCOMPARE(alarmPrepareRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});alarmPrepare->trigger();QCOMPARE(alarmPrepareRequests.size(),1);
    auto sourceRetire=window.findChild<QAction *>("NativeSourceRetirement"),sourceRecovery=window.findChild<QAction *>("NativeSourceRecovery");
    QVERIFY(sourceRetire && sourceRetire->isEnabled() && sourceRecovery && sourceRecovery->isEnabled());
    QSignalSpy sourceRetirementRequests(&window,&MainWindow::RetireFailedSourcesSignal),sourceRecoveryRequests(&window,&MainWindow::ReconcileAcquisitionWithSourcesSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});sourceRetire->trigger();QCOMPARE(sourceRetirementRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});sourceRetire->trigger();QCOMPARE(sourceRetirementRequests.size(),1);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});sourceRecovery->trigger();QCOMPARE(sourceRecoveryRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});sourceRecovery->trigger();QCOMPARE(sourceRecoveryRequests.size(),1);
    auto prepareRecovery=window.findChild<QAction *>("NativePreparationRecovery");QVERIFY(prepareRecovery && prepareRecovery->isEnabled());
    QSignalSpy prepareRecoveryRequests(&window,&MainWindow::ReconcilePreparationSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});prepareRecovery->trigger();QCOMPARE(prepareRecoveryRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});prepareRecovery->trigger();QCOMPARE(prepareRecoveryRequests.size(),1);
    auto failedUnprepare=window.findChild<QAction *>("NativeFailureUnprepare");QVERIFY(failedUnprepare && failedUnprepare->isEnabled());QSignalSpy unprepareRequests(&window,&MainWindow::UnprepareAfterFailureSignal);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});failedUnprepare->trigger();QCOMPARE(unprepareRequests.size(),0);
    QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});failedUnprepare->trigger();QCOMPARE(unprepareRequests.size(),1);


    QCOMPARE(table->horizontalHeaderItem(6)->text(),QString("Data GB")); QVERIFY(!window.findChild<QAction *>("NativePause")->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Native: waiting for current status"),Q_ARG(bool,true),Q_ARG(bool,true),Q_ARG(bool,true)));
    QVERIFY(!window.findChild<QPushButton *>("StopRun")->isEnabled()); QVERIFY(!window.findChild<QAction *>("NativeProductionStop")->isEnabled());QVERIFY(!cleanup->isEnabled());
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,Q_ARG(QString,"Disconnected"),Q_ARG(bool,false),Q_ARG(bool,false),Q_ARG(bool,false)));
    window.close(); QTRY_VERIFY(!window.isVisible());
    IPWindow settings(c); settings.resize(960,560); settings.show(); const auto scroll=settings.findChild<QScrollArea *>("NodeSettingsScroll");
    QVERIFY(scroll && scroll->widgetResizable()); QVERIFY(settings.height()<=600);
    auto nativeNoteInSettings=settings.findChild<QLabel *>("NativeTopologySourceNote");
    auto warningInSettings=settings.findChild<QLabel *>("LegacyTopologyWarning");
    QVERIFY(nativeNoteInSettings && nativeNoteInSettings->isVisible());
    QVERIFY(warningInSettings && !warningInSettings->isVisible());settings.reject();
    auto nativeTable=settings.findChild<QTableWidget *>("NodeSettingsTable");
    QVERIFY(nativeTable && nativeTable->isColumnHidden(3) && !nativeTable->isColumnHidden(14));
    IPWindow legacy(RunConfig::Default());legacy.show();
    auto warning=legacy.findChild<QLabel *>("LegacyTopologyWarning"),nativeNote=legacy.findChild<QLabel *>("NativeTopologySourceNote");
    QVERIFY(warning && warning->isVisible());QVERIFY(warning->text().contains("not hardware-validated"));
    QVERIFY(nativeNote && !nativeNote->isVisible());
    auto legacyTable=legacy.findChild<QTableWidget *>("NodeSettingsTable");
    QVERIFY(legacyTable && !legacyTable->isColumnHidden(3) && legacyTable->isColumnHidden(14));
    auto nativeMode=legacy.findChild<QCheckBox *>("UseNativeWholeSystemControl");QVERIFY(nativeMode);
    nativeMode->setChecked(true);QVERIFY(!warning->isVisible());QVERIFY(nativeNote->isVisible());
    QVERIFY(legacyTable->isColumnHidden(3) && !legacyTable->isColumnHidden(14));
}







void RunNativeSingleCoBoLifecycleUsesWholeEccTransitions()
{
    QTemporaryDir dir;RunConfig c=GetDAQTestSupport::Config(dir.path());c.simulation=false;c.nativeEcc=true;
    c.nodes.removeLast();c.nodes.first().name="CoBo0";c.nodes.first().nativeSenderId="C0";c.nodes.first().nativeAgentId="A0";
    c.nativeClient.address="127.0.0.1";c.nativeClient.port=24000;c.nativeClient.serviceId="ecc";c.nativeClient.clientId="console";
    c.nativeClient.clientEpoch=QString(64,'a');c.nativeClient.snapshotSha256=QString(64,'b');c.nativeClient.inputBindingSha256=QString(64,'c');
    c.nativeRecordDir=dir.filePath("records");
    MainWindow window;window.SetConfig(c);window.show();
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowCoBoMaintenanceTargetsSlot",Qt::DirectConnection,
        Q_ARG(QByteArray,CoBoMaintenanceFixture::Targets(true)),Q_ARG(QString,QString())));
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Qt::DirectConnection,
        Q_ARG(QString,"Native monitoring"),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
    auto *table=window.findChild<QTableWidget *>("StatusTable");table->selectRow(0);
    auto *progress=window.findChild<QPushButton *>("NodeProgress");auto *regress=window.findChild<QPushButton *>("NodeRegress");
    QCOMPARE(progress->text(),QString("Progress selected node"));
    QCOMPARE(regress->text(),QString("Regress selected node"));
    QCOMPARE(progress->property("scope").toString(),QString("single-system"));
    QCOMPARE(regress->property("scope").toString(),QString("single-system"));
    QVERIFY(progress->accessibleDescription().contains(QStringLiteral("entire fixed ECC system")));
    QVERIFY(progress->isEnabled());QVERIFY(regress->isVisible());QVERIFY(regress->isEnabled());
    QSignalSpy progressRequests(&window,&MainWindow::ProgressSignal),regressRequests(&window,&MainWindow::RegressSignal);
    QVERIFY(QMetaObject::invokeMethod(&window,"on_NodeProgress_clicked",Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&window,"on_NodeRegress_clicked",Qt::DirectConnection));
    QCOMPARE(progressRequests.size(),1);QCOMPARE(progressRequests.takeFirst().at(0).toInt(),-1);
    QCOMPARE(regressRequests.size(),1);QCOMPARE(regressRequests.takeFirst().at(0).toInt(),-1);

    MainWindow multiWindow;multiWindow.SetConfig(c);
    QVERIFY(QMetaObject::invokeMethod(&multiWindow,"ShowCoBoMaintenanceTargetsSlot",Qt::DirectConnection,
        Q_ARG(QByteArray,CoBoMaintenanceFixture::Targets(false)),Q_ARG(QString,QString())));
    QCOMPARE(multiWindow.findChild<QPushButton *>("NodeProgress")->text(),QString("Progress selected node"));
    QVERIFY(!multiWindow.findChild<QPushButton *>("NodeProgress")->isEnabled());
    QVERIFY(!multiWindow.findChild<QPushButton *>("NodeRegress")->isHidden());

    RunConfig withMutant=c;withMutant.nodes.append(GetDAQTestSupport::Config(dir.path()).nodes.last());
    MainWindow mutantWindow;mutantWindow.SetConfig(withMutant);
    QVERIFY(QMetaObject::invokeMethod(&mutantWindow,"ShowCoBoMaintenanceTargetsSlot",Qt::DirectConnection,
        Q_ARG(QByteArray,CoBoMaintenanceFixture::Targets(true)),Q_ARG(QString,QString())));
    QCOMPARE(mutantWindow.findChild<QPushButton *>("NodeProgress")->text(),QString("Progress selected node"));
    QVERIFY(!mutantWindow.findChild<QPushButton *>("NodeRegress")->isHidden());

    RunConfig mismatched=c;mismatched.nodes.first().nativeSenderId="C9";MainWindow mismatchWindow;
    mismatchWindow.SetConfig(mismatched);
    QVERIFY(QMetaObject::invokeMethod(&mismatchWindow,"ShowCoBoMaintenanceTargetsSlot",Qt::DirectConnection,
        Q_ARG(QByteArray,CoBoMaintenanceFixture::Targets(true)),Q_ARG(QString,QString())));
    QCOMPARE(mismatchWindow.findChild<QPushButton *>("NodeProgress")->text(),QString("Progress selected node"));
}
}
