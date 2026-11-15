#include "offline_tools_tests_p.h"
#include "getdaq_test_support_p.h"
#include "waveformviewer_testfixtures_p.h"

#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMainWindow>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPointer>
#include <QProcess>
#include <QPushButton>
#include <QSignalSpy>
#include <QStandardItemModel>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolButton>

#include "mainwindow.h"
#include "offlinedatatoolswindow.h"
#include "scanplanwindow.h"
#include "waveformwindow.h"

namespace GetDAQTestInternal {

void RunWaveformToolBlocksRunUntilItsWindowIsClosed()
{
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);MainWindow window;window.SetConfig(config);window.show();
    auto *action=window.findChild<QAction *>("OfflineWaveformViewer");QVERIFY(action && action->isEnabled());
    QVERIFY(window.findChild<QToolButton *>("MenuTools")->menu()->actions().contains(action));
    action->trigger();QPointer<QMainWindow> viewer=window.findChild<QMainWindow *>("WaveformViewerWindow");QVERIFY(viewer && viewer->isVisible());
    action->trigger();QCOMPARE(window.findChildren<QMainWindow *>("WaveformViewerWindow").count(),1);
    auto *table=window.findChild<QTableWidget *>("StatusTable");auto *start=window.findChild<QPushButton *>("StartRun");
    window.findChild<QPushButton *>("Connect")->click();QTRY_COMPARE(table->item(0,1)->text(),QString("Idle"));
    for(const auto state:{"Described","Prepared","Ready"}) {
        QTRY_VERIFY(window.findChild<QPushButton *>("Progress")->isEnabled());
        window.findChild<QPushButton *>("Progress")->click();QTRY_COMPARE(table->item(0,1)->text(),QString(state));
    }
    QTRY_VERIFY(action->isEnabled());QVERIFY(!start->isEnabled());QVERIFY(start->toolTip().contains("waveform"));
    QSignalSpy starts(&window,&MainWindow::StartSignal),scanStarts(&window,&MainWindow::StartScanPointSignal);
    QVERIFY(QMetaObject::invokeMethod(&window,"on_StartRun_clicked",Qt::DirectConnection));QCOMPARE(starts.count(),0);
    QVERIFY(!QFileInfo::exists(config.RunDirectory()));
    window.StartSignal(false);
    QTRY_VERIFY(window.findChild<QPlainTextEdit *>("Log")->toPlainText().contains("Control rejected Run start"));
    QVERIFY(!QFileInfo::exists(config.RunDirectory()));
    window.StartScanPointSignal("unused-journal");
    QTRY_VERIFY(window.findChild<QPlainTextEdit *>("Log")->toPlainText().contains("Control rejected scan Run start"));
    scanStarts.clear();
    const auto screenshots=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    if(!screenshots.isEmpty()){QDir().mkpath(screenshots);QVERIFY(window.grab().save(QDir(screenshots).filePath("viewer-main-ready.png")));QVERIFY(viewer->grab().save(QDir(screenshots).filePath("viewer-window.png")));}
    window.findChild<QAction *>("NativeOfflineScanPlans")->trigger();
    auto *scan=window.findChild<ScanPlanWindow *>();QVERIFY(scan);scan->StartScanPointRequested("unused-journal");QCOMPARE(scanStarts.count(),0);scan->close();
    viewer->close();QTRY_VERIFY(viewer.isNull());QTRY_VERIFY(start->isEnabled());
    start->click();QTRY_COMPARE(window.findChild<QLabel *>("Phase")->text(),QString("Running"));
    QVERIFY(!action->isEnabled());action->trigger();QVERIFY(!window.findChild<QMainWindow *>("WaveformViewerWindow"));
    window.findChild<QPushButton *>("StopRun")->click();QTRY_COMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),1);
    QTRY_VERIFY(action->isEnabled());action->trigger();QVERIFY(window.findChild<QMainWindow *>("WaveformViewerWindow"));
    window.close();QTRY_VERIFY(!window.isVisible());
}
void RunExternallyObservedRunClosesIntegratedWaveformViewer()
{
    MainWindow window;auto config=RunConfig::Default();window.SetConfig(config);window.show();
    auto *action=window.findChild<QAction *>("OfflineWaveformViewer");action->trigger();
    QPointer<QMainWindow> viewer=window.findChild<QMainWindow *>("WaveformViewerWindow");QVERIFY(viewer);
    QVector<NodeStatus> status;NodeStatus node;node.id=config.nodes.first().id;node.state=5;node.eccOnline=true;status.append(node);
    QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,status)));
    QTRY_VERIFY(viewer.isNull());QVERIFY(!action->isEnabled());
    status[0].state=4;QVERIFY(QMetaObject::invokeMethod(&window,"ShowStatusSlot",Qt::DirectConnection,Q_ARG(QVector<NodeStatus>,status)));
    QVERIFY(action->isEnabled());window.close();QTRY_VERIFY(!window.isVisible());
}
void RunOfflineDataToolsRunExistingProgramsAndPreserveInput()
{
    const auto programs=qEnvironmentVariable("GETDAQ_TEST_OFFLINE_TOOLS_DIR");
    if(programs.isEmpty()){QTest::qSkip("Set GETDAQ_TEST_OFFLINE_TOOLS_DIR to the built offline program directory",__FILE__,__LINE__);return;}
    QTemporaryDir dir;const auto input=dir.filePath("raw with spaces;literal.graw");
    const auto read=[](const QString &path){QFile file(path);if(!file.open(QIODevice::ReadOnly))return QByteArray();return file.readAll();};
    const int items=4*68*512;QByteArray raw(256+items*4,'\0');
    raw[0]=char(8);Put24BigEndian(&raw,1,raw.size()/256);Put16BigEndian(&raw,5,1);raw[7]=char(5);
    Put16BigEndian(&raw,8,1);Put16BigEndian(&raw,10,4);Put32BigEndian(&raw,12,items);Put32BigEndian(&raw,22,1234);
    for(int a=0;a<4;++a)for(int c=0;c<68;++c)for(int b=0;b<512;++b)
        Put32BigEndian(&raw,256+((a*68+c)*512+b)*4,(quint32(a)<<30)|(quint32(c)<<23)|(quint32(b)<<14)|quint32(b));
    QVERIFY(WriteWaveformFixture(input,raw));OfflineDataToolsWindow tools(nullptr,programs);tools.show();
    auto *mode=tools.findChild<QComboBox *>("OfflineToolMode");auto *inputs=tools.findChild<QListWidget *>("OfflineToolInputs");
    auto *output=tools.findChild<QLineEdit *>("OfflineToolOutput");auto *run=tools.findChild<QPushButton *>("OfflineToolStart");
    auto *status=tools.findChild<QLabel *>("OfflineToolStatus");auto *log=tools.findChild<QPlainTextEdit *>("OfflineToolLog");
    auto *remove=tools.findChild<QPushButton *>("OfflineToolRemove");
    QVERIFY(!run->isEnabled());QVERIFY(run->toolTip().contains("input"));QVERIFY(!remove->isEnabled());
    inputs->addItem(input);mode->setCurrentIndex(0);run->click();
    QTRY_COMPARE(status->text(),QString("Completed successfully."));QVERIFY(log->toPlainText().contains("VALID_STRUCTURE frames=1"));
    mode->setCurrentIndex(1);const auto matrix=dir.filePath("converted data.gmat");output->setText(matrix);run->click();
    QTRY_COMPARE(status->text(),QString("Completed successfully."));QCOMPARE(QFileInfo(matrix).size(),qint64(4*68*512*8));
    const auto matrixBytes=read(matrix);QVERIFY(!run->isEnabled());QVERIFY(run->toolTip().contains("existing path"));
    run->click();QCOMPARE(read(matrix),matrixBytes);
    mode->setCurrentIndex(2);const auto merged=dir.filePath("merged events.gmer");output->setText(merged);run->click();
    QTRY_COMPARE(status->text(),QString("Completed successfully."));QVERIFY(QFileInfo(merged).size()>0);QCOMPARE(read(input),raw);
    const auto shots=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    if(!shots.isEmpty()){QDir().mkpath(shots);QVERIFY(tools.grab().save(QDir(shots).filePath("data-tools-complete.png")));}
    QVERIFY(WriteWaveformFixture(input,MakeType1Frame()));mode->setCurrentIndex(1);output->setText(dir.filePath("incomplete.gmat"));run->click();
    QTRY_VERIFY(status->text().contains("Tool failed"));QVERIFY(log->toPlainText().contains("incomplete"));QVERIFY(!QFileInfo::exists(output->text()));
    inputs->setCurrentRow(0);QVERIFY(remove->isEnabled());remove->click();QCOMPARE(inputs->count(),0);
    QVERIFY(!remove->isEnabled());QVERIFY(!run->isEnabled());
}
void RunOfflineDataToolsMissingProgramsAndCloseDuringProcess_data()
{
    QTest::addColumn<int>("control");
    QTest::addColumn<bool>("ignoreTerminate");
    QTest::newRow("window-close")<<0<<false;QTest::newRow("close-button")<<1<<false;QTest::newRow("escape")<<2<<false;
    QTest::newRow("window-close-unresponsive-child")<<0<<true;
}
void RunOfflineDataToolsMissingProgramsAndCloseDuringProcess()
{
#ifdef Q_OS_UNIX
    QFETCH(int,control);QFETCH(bool,ignoreTerminate);
    QTemporaryDir dir;OfflineDataToolsWindow empty(nullptr,dir.path());
    auto *mode=empty.findChild<QComboBox *>("OfflineToolMode");QCOMPARE(mode->currentIndex(),-1);
    QVERIFY(!empty.findChild<QPushButton *>("OfflineToolStart")->isEnabled());
    const auto script=dir.filePath("getdaq-root-viewer");
    const QByteArray program=ignoreTerminate?QByteArray("#!/bin/sh\ntrap '' TERM\nexec sleep 30\n"):
        QByteArray("#!/bin/sh\nexec sleep 30\n");
    QVERIFY(WriteWaveformFixture(script,program));
    QVERIFY(QFile::setPermissions(script,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    QPointer<OfflineDataToolsWindow> tools=new OfflineDataToolsWindow(nullptr,dir.path());
    tools->setAttribute(Qt::WA_DeleteOnClose);tools->show();mode=tools->findChild<QComboBox *>("OfflineToolMode");QCOMPARE(mode->currentIndex(),5);
    auto *model=qobject_cast<QStandardItemModel *>(mode->model());QVERIFY(!model->item(3)->isEnabled());QVERIFY(!model->item(4)->isEnabled());
    tools->findChild<QPushButton *>("OfflineToolStart")->click();
    auto *process=tools->findChild<QProcess *>();QTRY_COMPARE(process->state(),QProcess::Running);
    QVERIFY(!tools->findChild<QComboBox *>("OfflineToolMode")->isEnabled());
    if(control==0)QVERIFY(tools->close());
    else if(control==1)tools->findChild<QPushButton *>("OfflineToolClose")->click();
    else QTest::keyClick(tools,Qt::Key_Escape);
    QVERIFY(!tools->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(tools.isNull(),ignoreTerminate?5000:3000);
#else
    QTest::qSkip("Unix child-process termination fixture",__FILE__,__LINE__);return;
#endif
}
void RunOfflineDataToolsAllCloseControls_data()
{
    QTest::addColumn<int>("control");
    QTest::newRow("window-close")<<0;QTest::newRow("close-button")<<1;QTest::newRow("escape")<<2;
}
void RunOfflineDataToolsAllCloseControls()
{
    QFETCH(int,control);QTemporaryDir dir;
    QPointer<OfflineDataToolsWindow> tools=new OfflineDataToolsWindow(nullptr,dir.path());
    tools->setAttribute(Qt::WA_DeleteOnClose);tools->show();QVERIFY(tools->isWindow());
    QVERIFY(tools->windowFlags().testFlag(Qt::WindowCloseButtonHint));
    if(control==0)QVERIFY(tools->close());
    else if(control==1)tools->findChild<QPushButton *>("OfflineToolClose")->click();
    else QTest::keyClick(tools,Qt::Key_Escape);
    QTRY_VERIFY(tools.isNull());
}
void RunOfflineDataToolsMenuWindowClosesAndCanReopen_data()
{
    QTest::addColumn<int>("control");
    QTest::newRow("window-close")<<0;QTest::newRow("close-button")<<1;QTest::newRow("escape")<<2;
}
void RunOfflineDataToolsMenuWindowClosesAndCanReopen()
{
#ifdef Q_OS_UNIX
    QFETCH(int,control);
    struct RestorePath {QByteArray path=qgetenv("PATH");~RestorePath(){qputenv("PATH",path);}} restore;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto tool=dir.filePath("getdaq-inspect");
    QVERIFY(WriteWaveformFixture(tool,"#!/bin/sh\nexit 0\n"));
    QVERIFY(QFile::setPermissions(tool,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    qputenv("PATH",QFile::encodeName(dir.path())+QDir::listSeparator().toLatin1()+restore.path);
    MainWindow window;window.SetConfig(RunConfig::Default());window.show();
    auto *action=window.findChild<QAction *>("OfflineDataTools");QVERIFY(action);QVERIFY(action->isEnabled());
    action->trigger();
    QPointer<OfflineDataToolsWindow> tools=window.findChild<OfflineDataToolsWindow *>();QVERIFY(tools);QVERIFY(tools->isVisible());
    if(control==0)QVERIFY(tools->close());
    else if(control==1)tools->findChild<QPushButton *>("OfflineToolClose")->click();
    else QTest::keyClick(tools,Qt::Key_Escape);
    QTRY_VERIFY(tools.isNull());
    QVERIFY(action->isEnabled());
    action->trigger();
    QPointer<OfflineDataToolsWindow> reopened=window.findChild<OfflineDataToolsWindow *>();QVERIFY(reopened);QVERIFY(reopened->isVisible());
    reopened->close();QTRY_VERIFY(reopened.isNull());
#else
    QTest::qSkip("Temporary executable discovery fixture requires Unix permissions",__FILE__,__LINE__);return;
#endif
}
void RunOfflineDataToolsClosePendingLocksMenuUntilProcessExits()
{
#ifdef Q_OS_UNIX
    struct RestorePath { QByteArray path=qgetenv("PATH");~RestorePath(){qputenv("PATH",path);} } restore;
    QTemporaryDir dir;QVERIFY(dir.isValid());
    const auto tool=dir.filePath("getdaq-inspect");
    QVERIFY(WriteWaveformFixture(tool,"#!/bin/sh\ntrap '' TERM\nexec sleep 30\n"));
    QVERIFY(QFile::setPermissions(tool,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    const auto input=dir.filePath("input.graw");QVERIFY(WriteWaveformFixture(input,MakeType1Frame()));
    qputenv("PATH",QFile::encodeName(dir.path())+QDir::listSeparator().toLatin1()+restore.path);
    MainWindow window;window.SetConfig(RunConfig::Default());window.show();
    auto *action=window.findChild<QAction *>("OfflineDataTools");QVERIFY(action->isEnabled());action->trigger();
    QPointer<OfflineDataToolsWindow> tools=window.findChild<OfflineDataToolsWindow *>();QVERIFY(tools);
    tools->findChild<QListWidget *>("OfflineToolInputs")->addItem(input);
    tools->findChild<QPushButton *>("OfflineToolStart")->click();
    auto *process=tools->findChild<QProcess *>();QTRY_COMPARE(process->state(),QProcess::Running);
    QVERIFY(tools->close());QVERIFY(!tools->isVisible());QVERIFY(!action->isEnabled());
    action->trigger();QVERIFY(!tools->isVisible());
    QTRY_VERIFY_WITH_TIMEOUT(tools.isNull(),5000);QTRY_VERIFY(action->isEnabled());
    window.close();QTRY_VERIFY(!window.isVisible());
#else
    QTest::qSkip("Unix child-process termination fixture",__FILE__,__LINE__);return;
#endif
}
void RunMultipleOfflineWindowsKeepRunLockedUntilBothClose()
{
    const auto programs=qEnvironmentVariable("GETDAQ_TEST_OFFLINE_TOOLS_DIR");
    if(programs.isEmpty()){QTest::qSkip("Set GETDAQ_TEST_OFFLINE_TOOLS_DIR to the built offline program directory",__FILE__,__LINE__);return;}
    struct RestorePath {QByteArray path=qgetenv("PATH");~RestorePath(){qputenv("PATH",path);}} restore;
    qputenv("PATH",QFile::encodeName(programs)+QDir::listSeparator().toLatin1()+restore.path);
    QTemporaryDir dir;auto config=GetDAQTestSupport::Config(dir.path());GetDAQTestSupport::MakeFiles(config);MainWindow window;window.SetConfig(config);window.show();
    auto *data=window.findChild<QAction *>("OfflineDataTools");QVERIFY(data->isEnabled());data->trigger();
    QPointer<OfflineDataToolsWindow> tools=window.findChild<OfflineDataToolsWindow *>();QVERIFY(tools);
    data->trigger();QCOMPARE(window.findChildren<OfflineDataToolsWindow *>().count(),1);
    window.findChild<QAction *>("OfflineWaveformViewer")->trigger();
    QPointer<QMainWindow> viewer=window.findChild<QMainWindow *>("WaveformViewerWindow");QVERIFY(viewer);
    window.findChild<QPushButton *>("Connect")->click();
    auto *table=window.findChild<QTableWidget *>("StatusTable");QTRY_COMPARE(table->item(0,1)->text(),QString("Idle"));
    for(const auto state:{"Described","Prepared","Ready"}) {
        QTRY_VERIFY(window.findChild<QPushButton *>("Progress")->isEnabled());window.findChild<QPushButton *>("Progress")->click();
        QTRY_COMPARE(table->item(0,1)->text(),QString(state));
    }
    auto *start=window.findChild<QPushButton *>("StartRun");QVERIFY(!start->isEnabled());
    viewer->close();QTRY_VERIFY(viewer.isNull());QVERIFY(!start->isEnabled());
    window.StartSignal(false);QTRY_VERIFY(window.findChild<QPlainTextEdit *>("Log")->toPlainText().contains("Control rejected Run start"));
    QVERIFY(!QFileInfo::exists(config.RunDirectory()));tools->close();QTRY_VERIFY(tools.isNull());QTRY_VERIFY(start->isEnabled());
    start->click();QTRY_COMPARE(window.findChild<QLabel *>("Phase")->text(),QString("Running"));QVERIFY(!data->isEnabled());
    const auto shots=qEnvironmentVariable("GETDAQ_UI_SCREENSHOT_DIR");
    if(!shots.isEmpty()){
        auto *menu=window.findChild<QToolButton *>("MenuTools")->menu();menu->popup(window.mapToGlobal(QPoint(350,40)));QTest::qWait(30);
        QVERIFY(menu->grab().save(QDir(shots).filePath("tools-menu-running-disabled.png")));menu->hide();
    }
    window.findChild<QPushButton *>("StopRun")->click();QTRY_COMPARE(window.findChild<QSpinBox *>("RunNumber")->value(),1);QTRY_VERIFY(data->isEnabled());
    window.close();QTRY_VERIFY(!window.isVisible());
}

} // namespace GetDAQTestInternal
