#include "coboconnectionwindow.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
class CoBoConnectionTests:public QObject {
    Q_OBJECT
    void Tool(const QString &directory,const QByteArray &script) {
        QFile file(directory+"/getdaq-cobo-info");QVERIFY(file.open(QIODevice::WriteOnly));file.write("#!/usr/bin/env python3\n"+script);file.close();QVERIFY(file.setPermissions(QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner));
    }
    void Settings(CoBoConnectionWindow &window,const QString &directory) {
        window.findChild<QLineEdit *>("CoBoPrograms")->setText(directory);window.findChild<QLineEdit *>("CoBoAddress")->setText("192.168.40.64");
    }
private slots:
    void successfulConnectionUsesOnlyFixedReadOperations() {
        QTemporaryDir temp;Tool(temp.path(),R"PY(import sys,json,pathlib
args=sys.argv[1:]
with open(pathlib.Path(__file__).parent/'commands.jsonl','a') as log: log.write(json.dumps(args)+'\n')
if args[0]=='devices': print(json.dumps({'node':'CoBo[0]','devices':['ctrl','aget','asad']}))
elif args[0]=='read-register' and args[2]=='ctrl' and args[3] in ('hardwareVersion','firmwareRelease','firmwareDate'): print(json.dumps({'value':{'hardwareVersion':'5','firmwareRelease':'249','firmwareDate':'538380306'}[args[3]]}))
elif args[0]=='read-field' and args[2:4]==['ctrl','pipeCtrl'] and args[4] in ('firmwareEnable','intEnable'): print(json.dumps({'value':'0'}))
else: raise SystemExit('Unexpected hardware operation')
)PY");
        CoBoConnectionWindow window;Settings(window,temp.path());window.findChild<QPushButton *>("ConnectDirectCoBo")->click();
        auto *status=window.findChild<QLabel *>("CoBoConnectionStatus");QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("Real CoBo connection verified"),10000);
        const auto *values=window.findChild<QTreeWidget *>("CoBoObservationValues");QCOMPARE(values->topLevelItem(1)->text(1),QString("CoBo[0]"));QCOMPARE(values->topLevelItem(3)->text(1),QString("5"));
        QFile log(temp.filePath("commands.jsonl"));QVERIFY(log.open(QIODevice::ReadOnly));const auto commands=log.readAll().trimmed().split('\n');QCOMPARE(commands.size(),6);for(const auto &command:commands)QVERIFY(!command.contains("write") && !command.contains("prepare") && !command.contains("start"));
        window.findChild<QPushButton *>("DisconnectDirectCoBo")->click();QCOMPARE(values->topLevelItemCount(),0);QVERIFY(!status->text().contains("verified"));
    }
    void failureDoesNotShowConnectedOrOldValues() {
        QTemporaryDir temp;Tool(temp.path(),"import sys\nprint('Board unavailable',file=sys.stderr)\nsys.exit(2)\n");CoBoConnectionWindow window;Settings(window,temp.path());window.findChild<QPushButton *>("ConnectDirectCoBo")->click();
        auto *status=window.findChild<QLabel *>("CoBoConnectionStatus");QTRY_VERIFY_WITH_TIMEOUT(status->text().contains("not verified"),5000);QCOMPARE(window.findChild<QTreeWidget *>("CoBoObservationValues")->topLevelItemCount(),0);
    }
    void editingEndpointCancelsObservation() {
        QTemporaryDir temp;Tool(temp.path(),"import time\ntime.sleep(1)\nprint('{\"node\":\"wrong\",\"devices\":[]}')\n");CoBoConnectionWindow window;Settings(window,temp.path());window.findChild<QPushButton *>("ConnectDirectCoBo")->click();window.findChild<QLineEdit *>("CoBoAddress")->setText("192.168.40.65");QTest::qWait(1200);QVERIFY(!window.findChild<QLabel *>("CoBoConnectionStatus")->text().contains("verified"));QCOMPARE(window.findChild<QTreeWidget *>("CoBoObservationValues")->topLevelItemCount(),0);
    }
};
QTEST_MAIN(CoBoConnectionTests)
#include "test_coboconnection.moc"
