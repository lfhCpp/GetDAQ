#include "runrecorder_tests_p.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "runconfig.h"
#include "runcontrol.h"
#include "runrecorder.h"
#include "getdaq_test_support_p.h"

namespace GetDAQTestInternal {

void RunRunReservationFailureDoesNotPublishPartialDirectory()
{
    QTemporaryDir dir;
    RunConfig invalid = GetDAQTestSupport::Config(dir.path());
    invalid.runNumber = -1; // Save rejects invalid metadata before any Run command can be issued.
    QString error;
    QVERIFY(!RunRecorder::Reserve(invalid, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY(!QFileInfo::exists(invalid.RunDirectory()));
    const QString runParent = QFileInfo(invalid.RunDirectory()).absolutePath();
    QVERIFY(QFileInfo(runParent).isDir());
    const QStringList pending = QDir(runParent).entryList(
        QStringList() << QStringLiteral(".*.pending-*"),
        QDir::Dirs | QDir::Hidden | QDir::NoDotAndDotDot);
    QVERIFY(pending.isEmpty());
}

void RunRunRecorderExplicitReplacementRetainsOldRunUntilFinalization()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    RunConfig c = GetDAQTestSupport::Config(dir.path()); QString error;
    QVERIFY(RunRecorder::Reserve(c, &error));
    QVERIFY(RunRecorder::Finish(c, 1.25).isEmpty());
    QJsonObject oldJournal; QVERIFY(RunRecorder::ReadJournal(c, &oldJournal, &error));
    oldJournal["phase"] = "Complete"; QVERIFY(RunRecorder::WriteJournal(c, oldJournal, &error));
    const QString markerPath = c.RunDirectory() + "/old-data-marker.txt";
    QFile marker(markerPath); QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("old", 3), qint64(3)); marker.close();

    QVERIFY(!RunRecorder::Reserve(c, &error));
    QVERIFY(QFileInfo::exists(markerPath));
    QVERIFY(RunRecorder::Reserve(c, true, &error));
    QJsonObject newJournal; QVERIFY(RunRecorder::ReadJournal(c, &newJournal, &error));
    QCOMPARE(newJournal.value("phase").toString(), QString("Reserved"));
    const QString backupName = newJournal.value("replacement_backup").toString();
    QVERIFY(!backupName.isEmpty());
    const QString backupPath = QFileInfo(c.RunDirectory()).absolutePath() + "/" + backupName;
    QVERIFY(QFileInfo::exists(backupPath + "/old-data-marker.txt"));
    QVERIFY(!RunRecorder::FinalizeReplacement(c, &error));
    QVERIFY(QFileInfo::exists(backupPath + "/old-data-marker.txt"));

    QVERIFY(RunRecorder::Finish(c, 2.5).isEmpty());
    QVERIFY(RunRecorder::ReadJournal(c, &newJournal, &error));
    newJournal["phase"] = "Complete"; QVERIFY(RunRecorder::WriteJournal(c, newJournal, &error));
    QVERIFY(RunRecorder::FinalizeReplacement(c, &error));
    QVERIFY(!QFileInfo::exists(backupPath));
    QVERIFY(QFileInfo::exists(c.RunDirectory() + "/run.csv"));
    QVERIFY(RunRecorder::ReadJournal(c, &newJournal, &error));
    QVERIFY(!newJournal.contains("replacement_backup"));
    QVERIFY(RunRecorder::FinalizeReplacement(c, &error));
}

void RunRunRecorderWritesNativeReferencesWithOptionalCatalogMirror()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    RunConfig config=GetDAQTestSupport::Config(dir.path());QString error;
    QVERIFY(RunRecorder::Reserve(config,&error));
    const QByteArray receiver="{\"receivers\":[]}";
    const QByteArray identity="{\"instance_id\":\"test\"}";
    const QByteArray configuration="{\"configuration\":1}";
    QVERIFY(RunRecorder::WriteNativeReferences(config,receiver,identity,configuration,true,&error));
    auto read=[&](const QString &name)->QByteArray { QFile file(QDir(config.RunDirectory()).filePath(name));if(!file.open(QIODevice::ReadOnly))return QByteArray();return file.readAll(); };
    QCOMPARE(read("receiver-status.json"),receiver);
    QCOMPARE(read("ecc-identity.json"),identity);
    QCOMPARE(read("ecc-configuration.json"),configuration);
    QCOMPARE(read("data-catalogs.json"),receiver);

    QTemporaryDir noCatalogDir;QVERIFY(noCatalogDir.isValid());
    RunConfig noCatalog=GetDAQTestSupport::Config(noCatalogDir.path());
    QVERIFY(RunRecorder::Reserve(noCatalog,&error));
    QVERIFY(RunRecorder::WriteNativeReferences(noCatalog,"next-status",identity,QByteArray(),false,&error));
    QFile noCatalogStatus(QDir(noCatalog.RunDirectory()).filePath("receiver-status.json"));
    QVERIFY(noCatalogStatus.open(QIODevice::ReadOnly));QCOMPARE(noCatalogStatus.readAll(),QByteArray("next-status"));
    QVERIFY(!QFileInfo::exists(QDir(noCatalog.RunDirectory()).filePath("data-catalogs.json")));
}

void RunRunRecorderCompletesNativeRunInDurableOrder()
{
    QTemporaryDir dir;QVERIFY(dir.isValid());
    RunConfig config=GetDAQTestSupport::Config(dir.path());QString error;
    QVERIFY(RunRecorder::Reserve(config,&error));
    QJsonObject journal;QVERIFY(RunRecorder::ReadJournal(config,&journal,&error));
    journal["replace_completed_run"]=false;
    QString cleanupWarning;
    QVERIFY(RunRecorder::CompleteNativeRun(config,"status","identity","configuration",true,
                                           12.5,&journal,&error,&cleanupWarning));
    QVERIFY(cleanupWarning.isEmpty());
    QCOMPARE(journal.value("phase").toString(),QString("Complete"));
    QJsonObject persisted;QVERIFY(RunRecorder::ReadJournal(config,&persisted,&error));
    QCOMPARE(persisted.value("phase").toString(),QString("Complete"));
    QFile csv(QDir(config.RunDirectory()).filePath("run.csv"));
    QVERIFY(csv.open(QIODevice::ReadOnly));
    QVERIFY(csv.readAll().contains("12.500"));
    QFile catalog(QDir(config.RunDirectory()).filePath("data-catalogs.json"));
    QVERIFY(catalog.open(QIODevice::ReadOnly));QCOMPARE(catalog.readAll(),QByteArray("status"));
}

void RunRunRecorderReplacementRejectsIncompleteOrMismatchedRun()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    RunConfig c = GetDAQTestSupport::Config(dir.path()); QString error;
    QVERIFY(RunRecorder::Reserve(c, &error));
    const QString markerPath = c.RunDirectory() + "/preserve-me.txt";
    QFile marker(markerPath); QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("keep", 4), qint64(4)); marker.close();
    QVERIFY(!RunRecorder::Reserve(c, true, &error));
    QVERIFY(QFileInfo::exists(markerPath));

    QVERIFY(RunRecorder::Finish(c, 1.0).isEmpty());
    QJsonObject journal; QVERIFY(RunRecorder::ReadJournal(c, &journal, &error));
    journal["phase"] = "Complete"; QVERIFY(RunRecorder::WriteJournal(c, journal, &error));
    RunConfig changed = c; changed.nodes[0].sentryHost += "-different";
    QVERIFY(!RunRecorder::Reserve(changed, true, &error));
    QVERIFY(QFileInfo::exists(markerPath));
    QVERIFY(RunRecorder::ReadJournal(c, &journal, &error));
    QCOMPARE(journal.value("phase").toString(), QString("Complete"));
}

void RunInterruptedRecovery()
{
    QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); QString error;
    c.description = "Original metadata";
    QVERIFY(RunRecorder::Reserve(c, &error));
    QJsonObject journal;
    QVERIFY(RunRecorder::ReadJournal(c, &journal, &error));
    QJsonObject catalog; catalog[QString::number(c.nodes[0].id)] = "complete";
    journal["catalog"] = catalog; journal["duration_seconds"] = 12.5;
    QVERIFY(RunRecorder::WriteJournal(c, journal, &error));
    RunControl control;
    QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
    c.description = "Changed after crash";
    control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 1));
    for (int target : {2, 3, 4}) { control.ProgressSlot(); QTRY_VERIFY(GetDAQTestSupport::At(status, target)); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Monitoring")); }
    control.RecoverSlot(); QVERIFY(phase.last().at(3).toBool());
    control.FinalizeSlot(); QTRY_COMPARE(done.count(), 1);
    QFile csv(c.RunDirectory() + "/run.csv"); QVERIFY(csv.open(QIODevice::ReadOnly));
    const QByteArray data = csv.readAll(); QVERIFY(data.contains("Original metadata")); QVERIFY(data.contains("12.500"));
    control.ShutdownSlot();
}

}
