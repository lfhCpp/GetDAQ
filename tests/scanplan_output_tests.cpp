#include "scanplan_output_tests_p.h"

#include <QtTest>
#include <QTemporaryDir>
#include <QFile>
#include <QCryptographicHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "scanplanoutput.h"

namespace GetDAQTestInternal {
void RunScanPlanOutputInspectionRejectsMalformedPlans()
{
    QTemporaryDir directory;
    QVERIFY(directory.isValid());
    QFile point(directory.filePath(QStringLiteral("point-000.json")));
    QVERIFY(point.open(QIODevice::WriteOnly));
    const QByteArray artifactBytes("{}\n");
    QCOMPARE(point.write(artifactBytes), qint64(artifactBytes.size()));
    point.close();
    const QString artifactSha256 = QString::fromLatin1(
        QCryptographicHash::hash(artifactBytes, QCryptographicHash::Sha256).toHex());
    const QString configurationSha256 = QString(64, QLatin1Char('a'));

    const auto makeManifest = [&](const QString &kind, const QJsonArray &points) {
        const QJsonObject plan{{QStringLiteral("schema_version"), 1},
                               {QStringLiteral("kind"), kind},
                               {QStringLiteral("target_cobo"), QStringLiteral("C0")},
                               {QStringLiteral("hardware_loaded_verified"), false},
                               {QStringLiteral("requires_full_configure_per_point"), true},
                               {QStringLiteral("acquisition_event_count_unspecified"), true},
                               {QStringLiteral("gain_result_interpretation_verified"), false},
                               {QStringLiteral("points"), points}};
        QFile manifest(directory.filePath(QStringLiteral("scan-manifest.json")));
        if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QByteArray bytes = QJsonDocument(plan).toJson(QJsonDocument::Compact);
        return manifest.write(bytes) == bytes.size();
    };
    const auto overrideManifest = [&](const QString &key, const QJsonValue &value) {
        QFile manifest(directory.filePath(QStringLiteral("scan-manifest.json")));
        if (!manifest.open(QIODevice::ReadOnly)) return false;
        QJsonObject plan = QJsonDocument::fromJson(manifest.readAll()).object();
        manifest.close();
        plan.insert(key, value);
        if (!manifest.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        const QByteArray bytes = QJsonDocument(plan).toJson();
        return manifest.write(bytes) == bytes.size();
    };
    const QJsonObject pointZero{{QStringLiteral("index"), 0},
                                {QStringLiteral("artifact"), QStringLiteral("point-000.json")},
                                {QStringLiteral("configuration_sha256"), configurationSha256},
                                {QStringLiteral("artifact_sha256"), artifactSha256}};
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"), QJsonArray() << pointZero));
    ScanPlanOutputSummary summary = InspectScanPlanOutput(directory.path());
    QVERIFY(!summary.isValid()); // A digest alone cannot make a non-artifact into a plan point.

    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << QJsonObject{{QStringLiteral("index"), 0},
                                                     {QStringLiteral("artifact"), QStringLiteral("point-000.json")},
                                                     {QStringLiteral("configuration_sha256"), configurationSha256},
                                                     {QStringLiteral("artifact_sha256"), QString(64, QLatin1Char('0'))}}));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());

    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << pointZero));
    QFile tamperedPoint(directory.filePath(QStringLiteral("point-000.json")));
    QVERIFY(tamperedPoint.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(tamperedPoint.write("{ }\n"), qint64(4));
    tamperedPoint.close();
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(tamperedPoint.open(QIODevice::WriteOnly | QIODevice::Truncate));
    QCOMPARE(tamperedPoint.write(artifactBytes), qint64(artifactBytes.size()));
    tamperedPoint.close();

    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << pointZero));
    QVERIFY(overrideManifest(QStringLiteral("hardware_loaded_verified"), true));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"), QJsonArray() << pointZero));
    QVERIFY(overrideManifest(QStringLiteral("requires_full_configure_per_point"), false));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"), QJsonArray() << pointZero));
    QVERIFY(overrideManifest(QStringLiteral("acquisition_event_count_unspecified"), false));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.auto-gain-scan"), QJsonArray() << pointZero));
    QVERIFY(overrideManifest(QStringLiteral("gain_result_interpretation_verified"), true));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());

    QVERIFY(makeManifest(QStringLiteral("unknown-scan-kind"), QJsonArray() << pointZero));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << QJsonObject{{QStringLiteral("index"), 0.5},
                                                     {QStringLiteral("artifact"), QStringLiteral("point-000.json")}}));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << QJsonObject{{QStringLiteral("index"), 0},
                                                     {QStringLiteral("artifact"), QStringLiteral("../outside.json")}}));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << pointZero << pointZero));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << QJsonObject{{QStringLiteral("index"), 0},
                                                     {QStringLiteral("artifact"), QStringLiteral("missing.json")}}));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());

    QFile emptyPoint(directory.filePath(QStringLiteral("empty.json")));
    QVERIFY(emptyPoint.open(QIODevice::WriteOnly));
    emptyPoint.close();
    QVERIFY(makeManifest(QStringLiteral("getdaq.peaking-time-scan"),
                         QJsonArray() << QJsonObject{{QStringLiteral("index"), 0},
                                                     {QStringLiteral("artifact"), QStringLiteral("empty.json")}}));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());

#ifdef Q_OS_UNIX
    const QString realManifest = directory.filePath(QStringLiteral("real-manifest.json"));
    QVERIFY(QFile::rename(directory.filePath(QStringLiteral("scan-manifest.json")), realManifest));
    QVERIFY(QFile::link(realManifest, directory.filePath(QStringLiteral("scan-manifest.json"))));
    QVERIFY(!InspectScanPlanOutput(directory.path()).isValid());
#endif
}
}
