#include "sentry_service_tests_p.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QTemporaryDir>
#include <QUrl>
#include <QTcpServer>
#include <QTcpSocket>
#include <QScopedPointer>

#include "sentryservice.h"
#include "httpclient.h"

namespace GetDAQTestInternal {

void RunSentryCatalogFiles()
{
    QTemporaryDir dir;
    SentryConfig c; c.dataDir = dir.path(); c.checkProcess = false; c.settleMs = 0;
    QFile a(dir.filePath("a.graw")); QVERIFY(a.open(QIODevice::WriteOnly)); a.write("12345"); a.close();
    QFile b(dir.filePath("b.graw")); QVERIFY(b.open(QIODevice::WriteOnly)); b.write("67890"); b.close();
    QFile other(dir.filePath("keep.txt")); QVERIFY(other.open(QIODevice::WriteOnly)); other.write("keep"); other.close();
    QDir().mkpath(dir.filePath("folder.graw"));
    auto status = SentryFiles::Status(c); QCOMPARE(status.code, 200); QCOMPARE(status.json.value("data_path_files").toInt(), 2);
    QCOMPARE(SentryFiles::Catalog(c, "../outside", 0).code, 400);
    auto result = SentryFiles::Catalog(c, "Test", 0); QCOMPARE(result.code, 200);
    QCOMPARE(result.json.value("data_path_files").toInt(), 0);
    QFile archived(dir.filePath("Test/run_0000/a.graw")); QVERIFY(archived.open(QIODevice::ReadOnly)); QCOMPARE(archived.readAll(), QByteArray("12345"));
    QVERIFY(QFileInfo::exists(dir.filePath("keep.txt"))); QVERIFY(QFileInfo::exists(dir.filePath("folder.graw")));
    QFile manifest(dir.filePath("Test/run_0000/catalog.json")); QVERIFY(manifest.open(QIODevice::ReadOnly));
    QCOMPARE(QJsonDocument::fromJson(manifest.readAll()).object().value("status").toString(), QString("complete"));
    QCOMPARE(SentryFiles::Catalog(c, "Test", 0).code, 409);
    c.checkProcess = true; c.processName = "getdaq_nonexistent_test_process_7398";
    QCOMPARE(SentryFiles::Status(c).code, 503);
}

void RunSentryCatalogExplicitReplacementDefersOldDataCleanup()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    SentryConfig c; c.dataDir = dir.path(); c.checkProcess = false; c.settleMs = 0;
    QFile oldRaw(dir.filePath("old.graw")); QVERIFY(oldRaw.open(QIODevice::WriteOnly));
    QCOMPARE(oldRaw.write("old-payload", 11), qint64(11)); oldRaw.close();
    QFile retainedRaw(dir.filePath("retained.graw")); QVERIFY(retainedRaw.open(QIODevice::WriteOnly));
    QCOMPARE(retainedRaw.write("retained-payload", 16), qint64(16)); retainedRaw.close();
    QCOMPARE(SentryFiles::Catalog(c, "Test", 7).code, 200);
    const QString target = dir.filePath("Test/run_0007");
    QFile oldManifest(QDir(target).filePath("catalog.json")); QVERIFY(oldManifest.open(QIODevice::ReadOnly));
    const QString oldPayloadName = QJsonDocument::fromJson(oldManifest.readAll()).object().value("planned").toArray().first().toString();
    oldManifest.close();
    QVERIFY(QFileInfo::exists(QDir(target).filePath(oldPayloadName)));

    QFile sameNameRaw(dir.filePath(oldPayloadName)); QVERIFY(sameNameRaw.open(QIODevice::WriteOnly));
    QCOMPARE(sameNameRaw.write("replacement-payload", 19), qint64(19)); sameNameRaw.close();
    QFile newRaw(dir.filePath("new.graw")); QVERIFY(newRaw.open(QIODevice::WriteOnly));
    QCOMPARE(newRaw.write("new-payload", 11), qint64(11)); newRaw.close();
    QCOMPARE(SentryFiles::Catalog(c, "Test", 7, true).code, 400);
    const QString operationId = QStringLiteral("replace-run-7-test");
    const ServiceResult replacement = SentryFiles::Catalog(c, "Test", 7, true, operationId);
    QCOMPARE(replacement.code, 200);
    QCOMPARE(SentryFiles::Catalog(c, "Test", 7, true, operationId).code, 200); // Lost reply/recovery uses the same operation id.

    QFile newManifest(QDir(target).filePath("catalog.json")); QVERIFY(newManifest.open(QIODevice::ReadOnly));
    const QJsonObject newCatalog = QJsonDocument::fromJson(newManifest.readAll()).object(); newManifest.close();
    QCOMPARE(newCatalog.value("replacement_operation_id").toString(), operationId);
    const QString backupName = newCatalog.value("replacement_backup").toString();
    QVERIFY(backupName.startsWith(".run_0007.replaced-"));
    const QString backup = dir.filePath("Test/" + backupName);
    QVERIFY(QFileInfo::exists(QDir(backup).filePath(oldPayloadName)));
    QVERIFY(QFile::rename(QDir(backup).filePath("retained.graw"), QDir(target).filePath("retained.graw")));
    QFile newPayload(QDir(target).filePath("new.graw")); QVERIFY(newPayload.open(QIODevice::ReadOnly));
    QCOMPARE(newPayload.readAll(), QByteArray("new-payload")); newPayload.close();

    QCOMPARE(SentryFiles::FinalizeReplacement(c, "Test", 7, "wrong-operation").code, 409);
    QVERIFY(QFileInfo::exists(QDir(backup).filePath(oldPayloadName)));
    const ServiceResult finalized = SentryFiles::FinalizeReplacement(c, "Test", 7, operationId);
    QCOMPARE(finalized.code, 200);
    QVERIFY(!QFileInfo::exists(backup));
    QVERIFY(finalized.json.value("replacement_cleanup_complete").toBool());
    QFile sameNameResult(QDir(target).filePath(oldPayloadName)); QVERIFY(sameNameResult.open(QIODevice::ReadOnly));
    QCOMPARE(sameNameResult.readAll(), QByteArray("replacement-payload"));
    QFile retainedResult(QDir(target).filePath("retained.graw")); QVERIFY(retainedResult.open(QIODevice::ReadOnly));
    QCOMPARE(retainedResult.readAll(), QByteArray("retained-payload"));
    QFile mergedManifest(QDir(target).filePath("catalog.json")); QVERIFY(mergedManifest.open(QIODevice::ReadOnly));
    const QJsonArray mergedNames = QJsonDocument::fromJson(mergedManifest.readAll()).object().value("planned").toArray();
    mergedManifest.close();
    QVERIFY(mergedNames.contains(QJsonValue(oldPayloadName)));
    QVERIFY(mergedNames.contains(QJsonValue(QStringLiteral("retained.graw"))));
    QVERIFY(mergedNames.contains(QJsonValue(QStringLiteral("new.graw"))));
    QCOMPARE(SentryFiles::FinalizeReplacement(c, "Test", 7, operationId).code, 200);
}

void RunSentryCatalogReplacementRejectsIncompleteOldRun()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    SentryConfig c; c.dataDir = dir.path(); c.checkProcess = false; c.settleMs = 0;
    const QString target = dir.filePath("Test/run_0003"); QVERIFY(QDir().mkpath(target));
    QFile marker(QDir(target).filePath("keep.txt")); QVERIFY(marker.open(QIODevice::WriteOnly));
    QCOMPARE(marker.write("preserve", 8), qint64(8)); marker.close();
    QFile incoming(dir.filePath("incoming.graw")); QVERIFY(incoming.open(QIODevice::WriteOnly));
    QCOMPARE(incoming.write("new", 3), qint64(3)); incoming.close();
    const ServiceResult result = SentryFiles::Catalog(c, "Test", 3, true, "op-3");
    QCOMPARE(result.code, 409);
    QVERIFY(QFileInfo::exists(marker.fileName()));
    QVERIFY(QFileInfo::exists(incoming.fileName()));
}

void RunSentryReplacementResumesAfterMergedManifestBeforeBackupCleanup()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    SentryConfig c; c.dataDir = dir.path(); c.checkProcess = false; c.settleMs = 0;
    QFile oldRaw(dir.filePath("old.graw")); QVERIFY(oldRaw.open(QIODevice::WriteOnly));
    QCOMPARE(oldRaw.write("old", 3), qint64(3)); oldRaw.close();
    QFile uniqueRaw(dir.filePath("unique.graw")); QVERIFY(uniqueRaw.open(QIODevice::WriteOnly));
    QCOMPARE(uniqueRaw.write("keep", 4), qint64(4)); uniqueRaw.close();
    QCOMPARE(SentryFiles::Catalog(c, "Test", 9).code, 200);
    const QString target = dir.filePath("Test/run_0009");
    QFile replacementRaw(dir.filePath("old.graw")); QVERIFY(replacementRaw.open(QIODevice::WriteOnly));
    QCOMPARE(replacementRaw.write("new", 3), qint64(3)); replacementRaw.close();
    QFile additionalRaw(dir.filePath("new.graw")); QVERIFY(additionalRaw.open(QIODevice::WriteOnly));
    QCOMPARE(additionalRaw.write("add", 3), qint64(3)); additionalRaw.close();
    const QString operation = QStringLiteral("resume-merged-manifest");
    QCOMPARE(SentryFiles::Catalog(c, "Test", 9, true, operation).code, 200);

    QFile catalogFile(QDir(target).filePath("catalog.json")); QVERIFY(catalogFile.open(QIODevice::ReadOnly));
    QJsonObject catalog = QJsonDocument::fromJson(catalogFile.readAll()).object(); catalogFile.close();
    const QString backup = QDir(dir.filePath("Test")).filePath(catalog.value("replacement_backup").toString());
    QVERIFY(QFile::rename(QDir(backup).filePath("unique.graw"), QDir(target).filePath("unique.graw")));
    QJsonArray names = catalog.value("planned").toArray(); names.append("unique.graw");
    catalog["planned"] = names; catalog["moved"] = names;
    catalog["replacement_preserved"] = QJsonArray{QStringLiteral("unique.graw")};
    QVERIFY(catalogFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(catalogFile.write(QJsonDocument(catalog).toJson()), qint64(QJsonDocument(catalog).toJson().size()));
    catalogFile.close();

    const ServiceResult finalized = SentryFiles::FinalizeReplacement(c, "Test", 9, operation);
    QCOMPARE(finalized.code, 200); QVERIFY(finalized.json.value("replacement_cleanup_complete").toBool());
    QVERIFY(!QFileInfo::exists(backup));
    QFile kept(QDir(target).filePath("unique.graw")); QVERIFY(kept.open(QIODevice::ReadOnly)); QCOMPARE(kept.readAll(), QByteArray("keep"));
    QFile replaced(QDir(target).filePath("old.graw")); QVERIFY(replaced.open(QIODevice::ReadOnly)); QCOMPARE(replaced.readAll(), QByteArray("new"));
    QCOMPARE(SentryFiles::FinalizeReplacement(c, "Test", 9, operation).code, 200);
}

void RunSentryConfigStrictFields()
{
    QTemporaryDir dir;
    QJsonObject base; base["data_dir"] = dir.path(); base["check_process"] = false;
    const auto loads = [&dir](const QJsonObject &object) {
        QFile file(dir.filePath("sentry-config.json"));
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        file.write(QJsonDocument(object).toJson(QJsonDocument::Compact)); file.close();
        SentryConfig config; QString error;
        return SentryConfig::Load(file.fileName(), &config, &error);
    };
    QVERIFY(loads(base));
    QJsonObject valid = base; valid["port"] = 49152; valid["settle_ms"] = 250;
    valid["process_name"] = "getdaq-receiver"; valid["disk_name"] = "DAQ";
    QVERIFY(loads(valid));
    for (const QString &key : {QString("port"), QString("settle_ms")}) {
        QJsonObject fractional = base; fractional[key] = 12.5; QVERIFY(!loads(fractional));
        QJsonObject stringValue = base; stringValue[key] = "12"; QVERIFY(!loads(stringValue));
    }
    QJsonObject processType = base; processType["process_name"] = 123; QVERIFY(!loads(processType));
    QJsonObject diskType = base; diskType["disk_name"] = true; QVERIFY(!loads(diskType));
    QJsonObject bindType = base; bindType["bind"] = 127; QVERIFY(!loads(bindType));
    QJsonObject portRange = base; portRange["port"] = 65536; QVERIFY(!loads(portRange));
    QJsonObject settleRange = base; settleRange["settle_ms"] = 120001; QVERIFY(!loads(settleRange));
}

void RunSentryHttpEndpoints()
{
    QTemporaryDir dir;
    SentryConfig config; config.dataDir = dir.path(); config.checkProcess = false; config.settleMs = 100; config.port = 0;
    QString error; SentryService service(config); QVERIFY2(service.Start(&error), qPrintable(error));
    HttpClient client;
    const QString base = QString("http://127.0.0.1:%1").arg(service.Port());
    bool done = false; QByteArray response; QString failure;
    auto callback = [&](const QByteArray &body, const QString &e) { response = body; failure = e; done = true; };
    client.Request(QUrl(base + "/status"), {}, {}, 1000, callback);
    QTRY_VERIFY(done); QVERIFY2(failure.isEmpty(), qPrintable(failure)); QVERIFY(QJsonDocument::fromJson(response).object().contains("disk_total_gb"));
    done = false;
    client.Request(QUrl(base + "/catalog"), "{\"experiment\":\"Test\",\"run_number\":1}", "application/json", 2000, callback);
    QTRY_VERIFY(done); QVERIFY2(failure.isEmpty(), qPrintable(failure));
    done = false;
    client.Request(QUrl(base + "/catalog"), "{\"experiment\":\"Test\",\"run_number\":1}", "application/json", 2000, callback);
    QTRY_VERIFY(done); QVERIFY(!failure.isEmpty());
    done = false;
    QFile replacementRaw(dir.filePath("replacement.graw")); QVERIFY(replacementRaw.open(QIODevice::WriteOnly));
    QCOMPARE(replacementRaw.write("payload", 7), qint64(7)); replacementRaw.close();
    client.Request(QUrl(base + "/catalog"),
                   "{\"experiment\":\"Test\",\"run_number\":1,\"replace_completed_run\":true,\"operation_id\":\"http-replace-1\"}",
                   "application/json", 2000, callback);
    QTRY_VERIFY(done); QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QFile replacementManifest(dir.filePath("Test/run_0001/catalog.json")); QVERIFY(replacementManifest.open(QIODevice::ReadOnly));
    const QString oldRunBackup = QJsonDocument::fromJson(replacementManifest.readAll()).object().value("replacement_backup").toString();
    replacementManifest.close();
    QVERIFY(oldRunBackup.startsWith(".run_0001.replaced-"));
    QVERIFY(QFileInfo::exists(dir.filePath("Test/" + oldRunBackup)));
    done = false;
    client.Request(QUrl(base + "/catalog/finalize"),
                   "{\"experiment\":\"Test\",\"run_number\":1,\"operation_id\":\"http-replace-1\"}",
                   "application/json", 2000, callback);
    QTRY_VERIFY(done); QVERIFY2(failure.isEmpty(), qPrintable(failure));
    QVERIFY(QJsonDocument::fromJson(response).object().value("replacement_cleanup_complete").toBool());
    QVERIFY(!QFileInfo::exists(dir.filePath("Test/" + oldRunBackup)));
    done = false;
    client.Request(QUrl(base + "/catalog"), "{\"experiment\":\"Test\",\"run_number\":1.5}", "application/json", 1000, callback);
    QTRY_VERIFY(done); QVERIFY(!failure.isEmpty());
    QTcpSocket socket; socket.connectToHost(QHostAddress::LocalHost, service.Port());
    QTRY_COMPARE(socket.state(), QAbstractSocket::ConnectedState);
    socket.write("POST /catalog HTTP/1.1\r\nHost: localhost\r\nContent-Length: 2\r\nContent-Length: 1\r\n\r\n{}");
    QTRY_VERIFY(socket.bytesAvailable() > 0); QVERIFY(socket.readAll().contains("400 Bad Request"));
}

}
