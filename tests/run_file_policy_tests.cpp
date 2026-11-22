#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonDocument>
#include <QJsonArray>
#include "sentryservice.h"
#include "sentryservice_http_p.h"
#include "sentry_service_tests_p.h"
class RunFilePolicyTests:public QObject {
 Q_OBJECT
 static void Put(const QString &p,const QByteArray &v){QFile f(p);QVERIFY(f.open(QIODevice::WriteOnly));QCOMPARE(f.write(v),qint64(v.size()));}
 static QByteArray Read(const QString &p){QFile f(p);if(!f.open(QIODevice::ReadOnly))return {};return f.readAll();}
private slots:
 void overwriteDeletesOnlySelectedRun(){
    QTemporaryDir dir;SentryConfig c;c.dataDir=dir.path();c.checkProcess=false;c.settleMs=0;
    Put(dir.filePath("old.graw"),"old");QCOMPARE(SentryFiles::Catalog(c,"Test",7).code,200);
    QVERIFY(QDir().mkpath(dir.filePath("Test/run_0008")));Put(dir.filePath("Test/run_0008/keep.graw"),"other run");Put(dir.filePath("new.graw"),"new");
    const auto result=SentryFiles::Catalog(c,"Test",7,true,"overwrite-7",true);QCOMPARE(result.code,200);QCOMPARE(result.json["replacement_policy"].toString(),QString("overwrite"));
    QVERIFY(!QFile::exists(dir.filePath("Test/run_0007/old.graw")));QCOMPARE(Read(dir.filePath("Test/run_0007/new.graw")),QByteArray("new"));QCOMPARE(Read(dir.filePath("Test/run_0008/keep.graw")),QByteArray("other run"));
    QVERIFY(QDir(dir.filePath("Test")).entryList({".run_0007.replaced-*"},QDir::Dirs|QDir::Hidden).isEmpty());QCOMPARE(SentryFiles::Catalog(c,"Test",7,true,"overwrite-7",true).code,200);QCOMPARE(SentryFiles::FinalizeReplacement(c,"Test",7,"overwrite-7").code,200);
 }
 void keepAddsOriginalTimestampedFilesWithoutRenaming(){
    QTemporaryDir dir;SentryConfig c;c.dataDir=dir.path();c.checkProcess=false;c.settleMs=0;
    Put(dir.filePath("time-old.graw"),"old");QCOMPARE(SentryFiles::Catalog(c,"Test",7).code,200);Put(dir.filePath("time-new.graw"),"new");
    auto result=SentryFiles::Catalog(c,"Test",7,true,"append-7",false,true);QCOMPARE(result.code,200);QCOMPARE(result.json["replacement_policy"].toString(),QString("append"));
    QCOMPARE(Read(dir.filePath("Test/run_0007/time-old.graw")),QByteArray("old"));QCOMPARE(Read(dir.filePath("Test/run_0007/time-new.graw")),QByteArray("new"));QVERIFY(!QFile::exists(dir.filePath("Test/run_0007/time-old_001.graw")));QVERIFY(!QFile::exists(dir.filePath("Test/run_0008")));
    const auto manifest=QJsonDocument::fromJson(Read(dir.filePath("Test/run_0007/catalog.json"))).object();QCOMPARE(manifest["run_number"].toInt(),7);QCOMPARE(manifest["planned"].toArray().size(),2);QCOMPARE(manifest["planned"],manifest["moved"]);
    Put(dir.filePath("later.graw"),"later");QCOMPARE(SentryFiles::Catalog(c,"Test",7,true,"append-7",false,true).code,200);QCOMPARE(Read(dir.filePath("later.graw")),QByteArray("later")); // same ID never consumes new incoming files
    QCOMPARE(SentryFiles::Catalog(c,"Test",7,true,"append-next",false,true).code,200);QCOMPARE(Read(dir.filePath("Test/run_0007/later.graw")),QByteArray("later"));
    Put(dir.filePath("time-old.graw"),"unexpected collision");QCOMPARE(SentryFiles::Catalog(c,"Test",7,true,"collision",false,true).code,409);QCOMPARE(Read(dir.filePath("Test/run_0007/time-old.graw")),QByteArray("old"));QCOMPARE(Read(dir.filePath("time-old.graw")),QByteArray("unexpected collision"));
 }
 void explicitPolicyValidationAndIncompleteReceipt(){
    SentryHttpInternal::MutationRequest request;int code=0;QString error;QVERIFY(!SentryHttpInternal::ParseMutation(SentryHttpInternal::Route::Catalog,R"({"experiment":"Test","run_number":7,"overwrite_existing_run":true})",&request,&code,&error));
    request={};QVERIFY(!SentryHttpInternal::ParseMutation(SentryHttpInternal::Route::Catalog,R"({"experiment":"Test","run_number":7,"replace_completed_run":true,"operation_id":"x","overwrite_existing_run":true,"append_existing_run":true})",&request,&code,&error));
    QTemporaryDir dir;SentryConfig c;c.dataDir=dir.path();c.checkProcess=false;c.settleMs=0;QVERIFY(QDir().mkpath(dir.filePath("Test/run_0007")));Put(dir.filePath("Test/run_0007/catalog.json"),R"({"status":"partial","replacement_operation_id":"partial","replacement_policy":"append"})");Put(dir.filePath("incoming.graw"),"data");QCOMPARE(SentryFiles::Catalog(c,"Test",7,true,"partial",false,true).code,409);QCOMPARE(Read(dir.filePath("incoming.graw")),QByteArray("data"));
 }
 void legacyCatalogStillPreservesOldFiles(){GetDAQTestInternal::RunSentryCatalogExplicitReplacementDefersOldDataCleanup();GetDAQTestInternal::RunSentryCatalogReplacementRejectsIncompleteOldRun();}
};
QTEST_GUILESS_MAIN(RunFilePolicyTests)
#include "run_file_policy_tests.moc"
