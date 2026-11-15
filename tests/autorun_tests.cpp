#include "autorun_tests_p.h"
#include "getdaq_test_support_p.h"

#include <QtTest>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>

#include "autorunwidget.h"
#include "mainwindow.h"
#include "runcontrol.h"
#include "runrecorder.h"

namespace GetDAQTestInternal {

void RunAutomaticRunSettingsRoundTrip()
{
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());
    config.autoRun.enabled=true;config.autoRun.condition=AutoRunSettings::FileSize;
    config.autoRun.action=AutoRunSettings::NextRun;config.autoRun.fileBytes=9007199254740993ULL;config.autoRun.maximumRuns=3;
    QString error;const auto path=dir.filePath("automation.yml");QVERIFY(config.Save(path,&error));
    RunConfig loaded;QVERIFY2(loaded.Load(path,&error),qPrintable(error));
    QCOMPARE(loaded.autoRun.fileBytes,quint64(9007199254740993ULL));QCOMPARE(loaded.autoRun.maximumRuns,3);
    QCOMPARE(loaded.autoRun.action,AutoRunSettings::NextRun);QVERIFY(loaded.autoRun.enabled);
    AutoRunPolicy policy;policy.Configure(loaded.autoRun,0,{});QCOMPARE(policy.stage,AutoRunPolicy::Idle);
    config.autoRun.condition=AutoRunSettings::Either;config.autoRun.durationMs=1000;
    QVERIFY(config.Save(path,&error));QVERIFY(loaded.Load(path,&error));QCOMPARE(loaded.autoRun.condition,AutoRunSettings::Either);
    config.autoRun.durationMs=0;QVERIFY(!config.Validate(&error));
    config.autoRun.condition=AutoRunSettings::NoCondition;QVERIFY(!config.Validate(&error));
}
void RunAutomaticRunPolicyWaitsForCompleteAndReady()
{
    AutoRunSettings settings;settings.enabled=true;settings.durationMs=1000;
    settings.action=AutoRunSettings::NextRun;settings.maximumRuns=2;
    AutoRunObservation view;view.connected=view.healthy=view.canControl=view.ready=true;
    AutoRunPolicy policy;policy.Configure(settings,0,view);policy.ManualStart(0,view);
    view.ready=false;view.active=view.running=true;
    QCOMPARE(policy.Tick(100,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(1099,view,5000),AutoRunPolicy::None);
    view.busy=true;QCOMPARE(policy.Tick(1100,view,5000),AutoRunPolicy::None);
    view.busy=false;QCOMPARE(policy.Tick(1200,view,5000),AutoRunPolicy::StopCurrent);
    QCOMPARE(policy.Tick(1300,view,5000),AutoRunPolicy::None);
    // A stopped hardware state is insufficient: successful journal Complete is required.
    view.running=view.active=false;view.prepared=true;
    QCOMPARE(policy.Tick(1400,view,5000),AutoRunPolicy::None);
    policy.Completed(1500);QCOMPARE(policy.Tick(1500,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(4499,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(4500,view,5000),AutoRunPolicy::PrepareNext);
    view.busy=true;QCOMPARE(policy.Tick(4600,view,5000),AutoRunPolicy::None);
    view.prepared=view.busy=false;view.ready=true;
    QCOMPARE(policy.Tick(4700,view,5000),AutoRunPolicy::StartNext);
    view.active=view.running=true;view.ready=false;
    QCOMPARE(policy.Tick(4800,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(5800,view,5000),AutoRunPolicy::StopCurrent);
    policy.Completed(6000);QCOMPARE(policy.stage,AutoRunPolicy::Idle);QCOMPARE(policy.completedRuns,2);
    QCOMPARE(policy.Tick(7000,view,5000),AutoRunPolicy::None);
}
void RunAutomaticRunEitherThresholdAndGap()
{
    AutoRunSettings settings;settings.enabled=true;settings.condition=AutoRunSettings::Either;
    settings.durationMs=1000;settings.fileBytes=100;
    AutoRunObservation view;view.connected=view.healthy=view.canControl=view.active=view.running=view.bytesKnown=true;
    view.fileBytes={{"session",1000}};
    AutoRunPolicy policy;policy.Configure(settings,0,view);
    view.fileBytes["session"]=1100;
    QCOMPARE(policy.Tick(100,view,5000),AutoRunPolicy::StopCurrent); // size first
    view.fileBytes["session"]=1000;policy.Configure(settings,200,view);
    QCOMPARE(policy.Tick(1199,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(1200,view,5000),AutoRunPolicy::StopCurrent); // time first
    settings.action=AutoRunSettings::NextRun;policy.Configure(settings,2000,view);
    QCOMPARE(policy.Tick(3000,view,5000),AutoRunPolicy::StopCurrent);
    view.active=view.running=false;view.ready=true;
    policy.Completed(10000); // slow finalization: gap begins only after Complete
    QCOMPARE(policy.Tick(12999,view,500),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(13000,view,500),AutoRunPolicy::StartNext);
    policy.Cancel("manual stop during gap");QCOMPARE(policy.Tick(15000,view,5000),AutoRunPolicy::None);
    policy.Configure(settings,20000,view);policy.ManualStart(20000,view);
    view.active=view.running=true;view.ready=false;policy.Tick(20001,view,5000);
    QCOMPARE(policy.Tick(21001,view,5000),AutoRunPolicy::StopCurrent);
    view.active=view.running=false;view.ready=true;policy.Completed(22000);
    view.healthy=false;QCOMPARE(policy.Tick(25000,view,5000),AutoRunPolicy::None);
    QCOMPARE(policy.stage,AutoRunPolicy::Idle); // elapsed 3 s cannot bypass readiness
}
void RunAutomaticRunPolicyPauseAndFreshBytes()
{
    AutoRunSettings settings;settings.enabled=true;settings.condition=AutoRunSettings::FileSize;settings.fileBytes=10;
    AutoRunObservation view;view.connected=view.healthy=view.canControl=view.ready=view.bytesKnown=true;
    view.fileBytes={{"A/session",9007199254740993ULL},{"B/session",3}};
    AutoRunPolicy policy;policy.Configure(settings,0,view);policy.ManualStart(0,view);
    view.ready=false;view.active=view.running=true;policy.Tick(10,view,1000);
    view.fileBytes["A/session"]+=9;QCOMPARE(policy.Tick(20,view,1000),AutoRunPolicy::None);QCOMPARE(policy.windowBytes,quint64(9));
    view.running=false;view.paused=true;policy.Tick(30,view,1000);
    view.fileBytes["A/session"]+=100;QCOMPARE(policy.Tick(40,view,1000),AutoRunPolicy::None);
    view.paused=false;view.running=true;QCOMPARE(policy.Tick(50,view,1000),AutoRunPolicy::None);QCOMPARE(policy.windowBytes,quint64(0));
    view.fileBytes["B/session"]+=10;QCOMPARE(policy.Tick(60,view,1000),AutoRunPolicy::StopCurrent);
    policy.Cancel("manual stop");policy.Completed(70);QCOMPARE(policy.Tick(80,view,1000),AutoRunPolicy::None);
    policy.Configure(settings,100,view);
    view.fileBytes["A/session"]-=1;view.fileBytes["B/session"]+=1000;
    QCOMPARE(policy.Tick(110,view,1000),AutoRunPolicy::None);QCOMPARE(policy.stage,AutoRunPolicy::Idle);
    QVERIFY(policy.reason.contains("decreased"));
    policy.Configure(settings,200,view);view.fileBytes.insert("new/session",100);
    QCOMPARE(policy.Tick(210,view,1000),AutoRunPolicy::None);QVERIFY(policy.reason.contains("session"));
    view.bytesKnown=false;policy.Configure(settings,300,view);
    QCOMPARE(policy.Tick(400,view,1000,200),AutoRunPolicy::None);
    QCOMPARE(policy.Tick(500,view,1000,200),AutoRunPolicy::None);QCOMPARE(policy.stage,AutoRunPolicy::Idle);
    QVERIFY(policy.reason.contains("stale"));
}
void RunAutomaticRunSimulationCyclesAndStops()
{
    for(const auto condition:{AutoRunSettings::Time,AutoRunSettings::FileSize,AutoRunSettings::Either}) {
        QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);
        config.autoRun.enabled=true;config.autoRun.condition=condition;config.autoRun.durationMs=300;
        config.autoRun.fileBytes=1000000;config.autoRun.action=AutoRunSettings::NextRun;config.autoRun.maximumRuns=2;
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal);
        QSignalSpy finished(&control,&RunControl::RunFinishedSignal),automation(&control,&RunControl::AutoRunStatusSignal);
        QElapsedTimer monotonic;monotonic.start();qint64 completedAt=-1,continuedAt=-1;
        QObject::connect(&control,&RunControl::RunFinishedSignal,&control,[&](int){if(completedAt<0)completedAt=monotonic.elapsed();});
        QObject::connect(&control,&RunControl::LogSignal,&control,[&](const QString &message){if(message.startsWith("Automatic Run: starting Run"))continuedAt=monotonic.elapsed();});
        control.ConnectSlot(config);QTRY_VERIFY(GetDAQTestSupport::At(status,1));
        for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(GetDAQTestSupport::At(status,state));QTRY_VERIFY(!phase.last().at(2).toBool());}
        control.StartRunSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Running"));
        // Two complete Run workflows include a mandatory >=3 s gap.
        // The default 5 s assertion timeout is too tight under parallel load.
        QTRY_COMPARE_WITH_TIMEOUT(finished.count(),2,15000);QCOMPARE(finished.last().at(0).toInt(),2);
        QVERIFY(completedAt>=0 && continuedAt-completedAt>=3000);
        QTRY_VERIFY(!automation.last().at(1).toBool());QTest::qWait(500);QCOMPARE(finished.count(),2);
        for(int run=0;run<2;++run){auto saved=config;saved.runNumber=run;QJsonObject journal;QString error;QVERIFY(RunRecorder::ReadJournal(saved,&journal,&error));QCOMPARE(journal.value("phase").toString(),QString("Complete"));}
        control.DisconnectSlot();
    }
}
void RunAutomaticRunManualPauseResumeStop()
{
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);
    config.autoRun.enabled=true;config.autoRun.durationMs=1000;config.autoRun.action=AutoRunSettings::NextRun;
    RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),finished(&control,&RunControl::RunFinishedSignal);
    control.ConnectSlot(config);QTRY_VERIFY(GetDAQTestSupport::At(status,1));
    for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(GetDAQTestSupport::At(status,state));QTRY_VERIFY(!phase.last().at(2).toBool());}
    control.StartRunSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Running"));QTest::qWait(400);
    control.PauseSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Paused"));QTest::qWait(1100);QCOMPARE(finished.count(),0);
    control.ResumeSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Running"));QTest::qWait(400);QCOMPARE(finished.count(),0);
    control.StopRunSlot();QTRY_COMPARE(finished.count(),1);QTest::qWait(1200);QCOMPARE(finished.count(),1);
    control.DisconnectSlot();
}
void RunAutomaticRunCollisionAndFinalizationFailure()
{
    for(const bool conflict:{true,false}) {
        QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());if(conflict)GetDAQTestSupport::MakeFiles(config);
        config.autoRun.enabled=true;config.autoRun.durationMs=200;config.autoRun.action=AutoRunSettings::NextRun;
        auto next=config;next.runNumber=1;
        if(conflict){QVERIFY(QDir().mkpath(next.RunDirectory()));QFile marker(QDir(next.RunDirectory()).filePath("keep.raw"));QVERIFY(marker.open(QIODevice::WriteOnly));marker.write("unchanged");}
        RunControl control;QSignalSpy status(&control,&RunControl::StatusSignal),phase(&control,&RunControl::ControlSignal),logs(&control,&RunControl::LogSignal),finished(&control,&RunControl::RunFinishedSignal),automation(&control,&RunControl::AutoRunStatusSignal);
        control.ConnectSlot(config);QTRY_VERIFY(GetDAQTestSupport::At(status,1));
        for(int state=2;state<=4;++state){control.ProgressSlot();QTRY_VERIFY(GetDAQTestSupport::At(status,state));QTRY_VERIFY(!phase.last().at(2).toBool());}
        control.StartRunSlot();QTRY_COMPARE(GetDAQTestSupport::Phase(phase),QString("Running"));
        if(!conflict)QVERIFY(QDir().mkpath(QDir(config.RunDirectory()).filePath("run.csv")));
        QTRY_VERIFY(!automation.isEmpty() && !automation.last().at(1).toBool());
        QTest::qWait(500);QCOMPARE(finished.count(),conflict ? 1 : 0);
        if(conflict){QFile marker(QDir(next.RunDirectory()).filePath("keep.raw"));QVERIFY(marker.open(QIODevice::ReadOnly));QCOMPARE(marker.readAll(),QByteArray("unchanged"));}
        else {QVERIFY(!QFileInfo::exists(next.RunDirectory()));control.ShutdownSlot();}
    }
}
void RunAutomaticRunUiPlacementAndScreenshot()
{
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);config.nodes=RunConfig::Default().nodes;MainWindow window;window.SetConfig(config);window.show();
    auto *automatic=window.findChild<AutoRunWidget *>();QVERIFY(automatic);
    auto settings=config.autoRun;settings.enabled=true;settings.action=AutoRunSettings::NextRun;settings.durationMs=60000;settings.condition=AutoRunSettings::Either;settings.maximumRuns=3;automatic->SetSettings(settings);
    QVERIFY(automatic->findChild<QCheckBox *>("AutoRunTimeEnabled")->isChecked());
    QVERIFY(automatic->findChild<QCheckBox *>("AutoRunSizeEnabled")->isChecked());
    const auto screenshotDir=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    for(const auto size:{QSize(1536,1024),QSize(1280,800)}) {
        window.resize(size);QApplication::processEvents();
        auto *button=window.findChild<QToolButton *>("AutoRunSettingsButton");QVERIFY(button);
        QTRY_COMPARE(automatic->isVisible(),size.width()==1536);QCOMPARE(button->isVisible(),size.width()!=1536);
        if(automatic->isVisible()) {
            auto *description=window.findChild<QLineEdit *>("Description");
            QVERIFY(automatic->mapTo(window.centralWidget(),QPoint()).x()>description->mapTo(window.centralWidget(),QPoint(description->width(),0)).x());
        }
        QTest::qWait(50);
        if(!screenshotDir.isEmpty()){QDir().mkpath(screenshotDir);QVERIFY(window.grab().save(QDir(screenshotDir).filePath(QString("automatic-%1x%2.png").arg(size.width()).arg(size.height()))));}
    }
    auto *button=window.findChild<QToolButton *>("AutoRunSettingsButton");button->click();
    auto *dialog=window.findChild<QDialog *>("AutoRunDialog");QVERIFY(dialog && dialog->isVisible());
    auto *copy=dialog->findChild<AutoRunWidget *>();QVERIFY(copy);QCOMPARE(copy->Settings().maximumRuns,3);dialog->close();
}
void RunAutomaticRunUiLocksSettingsThroughRunAndInterval()
{
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);
    config.autoRun.enabled=true;config.autoRun.condition=AutoRunSettings::Either;
    config.autoRun.durationMs=1000;config.autoRun.action=AutoRunSettings::NextRun;config.autoRun.maximumRuns=2;
    MainWindow window;window.SetConfig(config);window.show();window.resize(1536,1024);QTest::qWait(50);
    auto *automatic=window.findChild<AutoRunWidget *>();auto *apply=automatic->findChild<QPushButton *>("ApplyAutoRun");
    auto *title=window.findChild<QLabel *>("SetupPanelTitle");
    QVERIFY(automatic->mapTo(window.centralWidget(),QPoint()).y()<=title->mapTo(window.centralWidget(),QPoint()).y()+2);
    QVERIFY(apply->isEnabled());
    auto *table=window.findChild<QTableWidget *>("StatusTable");
    auto *start=window.findChild<QPushButton *>("StartRun");auto *stop=window.findChild<QPushButton *>("StopRun");
    window.findChild<QPushButton *>("Connect")->click();QTRY_COMPARE(table->item(0,1)->text(),QString("Idle"));
    for(const auto state:{"Described","Prepared","Ready"}) {
        window.findChild<QPushButton *>("Progress")->click();QTRY_COMPARE(table->item(0,1)->text(),QString(state));QTRY_VERIFY(start->isEnabled());
    }
    start->click();QTRY_COMPARE(window.findChild<QLabel *>("Phase")->text(),QString("Running"));
    QTRY_VERIFY(!apply->isEnabled());
    for(const char *name:{"AutoRunTimeEnabled","AutoRunSizeEnabled"})
        QVERIFY(!automatic->findChild<QCheckBox *>(name)->isEnabled());
    QVERIFY(!automatic->findChild<QComboBox *>("AutoRunAction")->isEnabled());
    auto *progress=automatic->findChild<QLabel *>("AutoRunStatus");QTRY_VERIFY(progress->text().contains(" OR "));
    QVERIFY(progress->wordWrap());
    const auto bounds=progress->fontMetrics().boundingRect(QRect(0,0,progress->width(),1000),Qt::TextWordWrap,progress->text());
    QVERIFY(progress->height()>=bounds.height());
    QSignalSpy changes(&window,&MainWindow::ConfigureAutoRunSignal);apply->click();QCOMPARE(changes.count(),0);
    // Even a direct queued request cannot bypass the controller's runtime lock.
    auto edited=config.autoRun;edited.enabled=false;window.ConfigureAutoRunSignal(edited);
    QTRY_VERIFY(window.findChild<QPlainTextEdit *>("Log")->toPlainText().contains("settings are locked"));
    auto *enable=automatic->findChild<QCheckBox *>("AutoRunEnabled");QVERIFY(enable->isEnabled());
    enable->click();QTRY_VERIFY(progress->text().contains("off"));QTest::qWait(1200);
    QCOMPARE(window.findChild<QLabel *>("Phase")->text(),QString("Running"));
    QCOMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),0);QVERIFY(!apply->isEnabled());
    enable->click();QTRY_VERIFY(progress->text().contains(" OR "));QTest::qWait(400);
    QCOMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),0);
    QTRY_COMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),1);
    QVERIFY(!apply->isEnabled());QTRY_VERIFY(stop->isEnabled());
    const auto screenshotDir=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    if(!screenshotDir.isEmpty()){QDir().mkpath(screenshotDir);QVERIFY(window.grab().save(QDir(screenshotDir).filePath("automatic-locked-interval.png")));}
    stop->click();QTRY_VERIFY(apply->isEnabled());QTest::qWait(3300);
    QCOMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),1);
    QVERIFY(window.findChild<QLabel *>("Phase")->text()!=QString("Running"));window.close();
}

} // namespace GetDAQTestInternal
