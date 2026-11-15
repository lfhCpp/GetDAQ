#include "operation_log_tests_p.h"

#include <QtTest>
#include <QCoreApplication>
#include <QColor>
#include <QApplication>
#include <QAction>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTimer>
#include <QTextBlock>
#include <QTextLayout>
#include <QToolButton>
#include <QUuid>

#include "mainwindow.h"
#include "diagnostics.h"
#include "getdaq_test_support_p.h"

namespace GetDAQTestInternal {

void RunEachConnectionGetsAnIndependentPersistentOperationLog()
{
    const QString previousApplicationName=QCoreApplication::applicationName();
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QString("getdaq-log-test-")+QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString localData=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    QString firstPath,secondPath;
    {
        MainWindow window;
        QTemporaryDir runDirectory;
        QVERIFY(runDirectory.isValid());
        RunConfig config=GetDAQTestSupport::Config(runDirectory.path());
        config.simulation=true;
        window.SetConfig(config);
        auto *connectButton=window.findChild<QPushButton *>("Connect");
        auto *disconnectButton=window.findChild<QPushButton *>("Disconnect");
        QVERIFY(connectButton && disconnectButton);
        const auto latestSessionPath=[&window]() {
            const QString text=window.findChild<QPlainTextEdit *>("Log")->toPlainText();
            const QString marker=QStringLiteral("Connection session log: ");
            const int start=text.lastIndexOf(marker);
            if(start<0)return QString();
            const int end=text.indexOf('\n',start);
            return text.mid(start+marker.size(),end<0 ? -1 : end-start-marker.size());
        };
        QTest::mouseClick(connectButton,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(disconnectButton->isEnabled(),2000);
        firstPath=latestSessionPath();
        QVERIFY(!firstPath.isEmpty());QVERIFY(QFileInfo::exists(firstPath));
        QVERIFY(QFileInfo(firstPath).absolutePath().endsWith(QStringLiteral("logs/connections")));
        QFile first(firstPath);QVERIFY(first.open(QIODevice::ReadOnly));
        QVERIFY(first.readAll().contains("Connection session log:"));first.close();
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowLogSlot",Qt::DirectConnection,Q_ARG(QString,QString("first-session-marker"))));
        QTest::mouseClick(disconnectButton,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(connectButton->isEnabled(),2000);
        QVERIFY(first.open(QIODevice::ReadOnly));
        const QByteArray firstBytes=first.readAll();
        QVERIFY(firstBytes.contains("first-session-marker"));
        QVERIFY(firstBytes.contains("Connection closed"));
        first.close();

        QTest::mouseClick(connectButton,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(disconnectButton->isEnabled(),2000);
        secondPath=latestSessionPath();
        QVERIFY(secondPath!=firstPath);
        QVERIFY(QFileInfo::exists(secondPath));
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowLogSlot",Qt::DirectConnection,Q_ARG(QString,QString("second-session-marker"))));
        QTest::mouseClick(disconnectButton,Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(connectButton->isEnabled(),2000);
        QVERIFY(first.open(QIODevice::ReadOnly));
        QVERIFY(!first.readAll().contains("second-session-marker"));
        QFile second(secondPath);QVERIFY(second.open(QIODevice::ReadOnly));
        QVERIFY(second.readAll().contains("second-session-marker"));
    }
    QFile::remove(firstPath);QFile::remove(secondPath);
    QDir(QFileInfo(firstPath).absolutePath()).rmdir("connections");
    QDir(localData).removeRecursively();
    QDir(QDir(QDir::tempPath()).filePath(QString("GetDAQ-%1").arg(QCoreApplication::applicationPid()))).removeRecursively();
    QCoreApplication::setApplicationName(previousApplicationName);
    QStandardPaths::setTestModeEnabled(false);
}

void RunVisibleOperationLogCanBeExported()
{
    struct NativeDialogSetting {
        bool previous;
        NativeDialogSetting():previous(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs))
        { QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true); }
        ~NativeDialogSetting(){QApplication::setAttribute(Qt::AA_DontUseNativeDialogs,previous);}
    } nativeDialogSetting;
    QTemporaryDir directory;QVERIFY(directory.isValid());
    const QString previousApplicationName=QCoreApplication::applicationName();
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QString("getdaq-export-log-test-")+QUuid::createUuid().toString(QUuid::WithoutBraces));
    const QString localData=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString destination=directory.filePath("operation-log.txt");
    {
        MainWindow window;
        auto *log=window.findChild<QPlainTextEdit *>("Log");
        auto *exportAction=window.findChild<QAction *>("ExportVisibleLog");
        QVERIFY(log&&exportAction);
        QVERIFY(!window.findChild<QLabel *>("LogSaveNote"));
        QVERIFY(window.findChild<QMenu *>("ToolsLogsMenu"));
        auto *toolsButton=window.findChild<QToolButton *>("MenuTools");
        QVERIFY(toolsButton && toolsButton->menu()->findChild<QAction *>("ExportVisibleLog"));
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowLogSlot",Qt::DirectConnection,Q_ARG(QString,QString("export-visible-marker"))));
        QTimer::singleShot(0,[&destination] {
            auto *dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if(dialog){dialog->selectFile(destination);QMetaObject::invokeMethod(dialog,"accept",Qt::DirectConnection);}
        });
        exportAction->trigger();
        QFile exported(destination);QVERIFY(exported.open(QIODevice::ReadOnly));
        const QByteArray contents=exported.readAll();
        QVERIFY(contents.contains("export-visible-marker"));
        QVERIFY(contents.endsWith('\n'));
    }
    QDir(localData).removeRecursively();
    QCoreApplication::setApplicationName(previousApplicationName);
    QStandardPaths::setTestModeEnabled(false);
}


void RunEventLogColorsPreserveOriginalText()
{
    MainWindow window;auto *log=window.findChild<QPlainTextEdit *>("Log");QVERIFY(log);
    const QList<QPair<QString,int>> messages={{"No errors",Diagnostics::Information},{"Warning: original warning text",Diagnostics::Warning},{"Unusual operation result\nOriginal second line <&>",Diagnostics::Error}};
    const QList<QColor> colors={QColor("#213547"),QColor("#b86b00"),QColor("#c74349")};
    for(int i=0;i<messages.size();++i) {
        const auto &message=messages[i];QVERIFY(QMetaObject::invokeMethod(&window,"ShowLogEntry",Qt::DirectConnection,Q_ARG(QString,message.first),Q_ARG(int,message.second)));
        QVERIFY(log->toPlainText().endsWith(message.first));
        auto block=log->document()->lastBlock();QVERIFY(block.layout());
        const auto formats=block.layout()->formats();QVERIFY(!formats.isEmpty());QCOMPARE(formats.first().format.foreground().color(),colors[i]);
    }
    QCOMPARE(Diagnostics::Classify("No errors"),Diagnostics::Information);
    QCOMPARE(Diagnostics::Classify("Hardware write outcome UNKNOWN at step 3"),Diagnostics::Error);
    QCOMPARE(Diagnostics::Classify("Warning: accepted legacy readDataDelay=255 remains unverified"),Diagnostics::Warning);
    QCOMPARE(Diagnostics::Classify("Warning: connection is stale"),Diagnostics::Warning);
    QCOMPARE(Diagnostics::Classify("Existing Run configuration identity differs; preserve it."),Diagnostics::Error);
    // Trimming the bounded document must not shift or lose severity metadata.
    for(int i=0;i<2005;++i)QVERIFY(QMetaObject::invokeMethod(&window,"ShowLogEntry",Qt::DirectConnection,Q_ARG(QString,QString("Warning: %1").arg(i)),Q_ARG(int,int(Diagnostics::Warning))));
    QCOMPARE(log->document()->blockCount(),2000);QVERIFY(log->toPlainText().endsWith("Warning: 2004"));
    QCOMPARE(log->document()->lastBlock().layout()->formats().first().format.foreground().color(),QColor("#b86b00"));
}
}
