#include "systemconfigurationpublisher.h"
#include "effectiveconfigurationsnapshot.h"
#include "configurationpublication.h"
#include "strictjson.h"
#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QJsonDocument>
class TestConfigurationPublication : public QObject {
    Q_OBJECT
private slots:
    void realConfigurationRoundTripsAndNeverOverwrites() {
        QTemporaryDir directory; QString error; XcfgDocument document; QJsonObject receipt;
        const auto source=QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
        const auto profile=QFINDTESTDATA("../config/experiment/system-profile.external-trigger.example.json");
        QVERIFY(document.Load(source,&error)); const auto destination=directory.filePath("v1");
        QVERIFY2(SystemConfigurationPublisher::Publish(document,profile,destination,&receipt,&error),qPrintable(error));
        QVERIFY(!receipt["hardware_loaded_verified"].toBool()); QFile manifest(destination+"/publication.json");
        QVERIFY(manifest.open(QIODevice::ReadOnly)); const auto bytes=manifest.readAll(); manifest.close();
        const auto publication=GetDAQ::DecodeConfigurationPublication(bytes.toStdString());
        const auto artifact=GetDAQ::VerifyConfigurationPublication(publication,[&](const GetDAQ::PublicationFile &file) {
            return GetDAQ::ReadConfigurationFile((destination+'/'+QString::fromStdString(file.stored)).toStdString());
        });
        QCOMPARE(QString::fromStdString(artifact.configurationSha256),receipt["configuration_sha256"].toString());
        QJsonObject effective; QVERIFY(EffectiveConfigurationSnapshot::Load(destination+"/effective.json",&effective,&error));
        QCOMPARE(effective["effective_sha256"],receipt["effective_sha256"]);
        QVERIFY(!SystemConfigurationPublisher::Publish(document,profile,destination,&receipt,&error)); QVERIFY(error.contains("already exists"));
        QVERIFY(manifest.open(QIODevice::ReadOnly)); QCOMPARE(manifest.readAll(),bytes);
    }
    void changedLoadedSourceCannotPublishOldConstraints() {
        QTemporaryDir directory; QString error; XcfgDocument document; QJsonObject keep; keep["keep"]=true;
        const auto source=QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
        const auto profile=QFINDTESTDATA("../config/experiment/system-profile.external-trigger.example.json"),copy=directory.filePath("input.xcfg");
        QVERIFY(QFile::copy(source,copy)); QVERIFY(document.Load(copy,&error)); QFile file(copy); QVERIFY(file.open(QIODevice::Append)); file.write("\n<!-- changed -->"); file.close();
        const auto destination=directory.filePath("not-published");
        QVERIFY(!SystemConfigurationPublisher::Publish(document,profile,destination,&keep,&error)); QVERIFY(error.contains("Source changed"));
        QVERIFY(!QFile::exists(destination)); QVERIFY(keep["keep"].toBool());
    }
    void invalidProfileLeavesNoPublicationDirectory() {
        QTemporaryDir directory; QString error; XcfgDocument document; QJsonObject keep; keep["keep"]=true;
        const auto source=QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
        const auto profile=directory.filePath("bad.json"); QFile file(profile); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("{}"); file.close();
        QVERIFY(document.Load(source,&error)); const auto destination=directory.filePath("invalid");
        QVERIFY(!SystemConfigurationPublisher::Publish(document,profile,destination,&keep,&error)); QVERIFY(!QFile::exists(destination)); QVERIFY(keep["keep"].toBool());
    }
};
QTEST_GUILESS_MAIN(TestConfigurationPublication)
#include "test_configuration_publication.moc"
