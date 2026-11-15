#include "native_scan_run_tests_p.h"

#include <QtTest>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <cstdint>
#include <exception>
#include <map>
#include <memory>
#include <sstream>
#include <string>

#include "cobo_configure_fixture.h"
#include "configurationsha256.h"
#include "getdaq_test_support_p.h"
#include "native_scan_run_fixture.h"
#include "nativeruncontrol.h"
#include "runconfig.h"
#include "scanexecutionjournalstore.h"
#include "scanexecutionpolicy.h"
#include "scanexecutionsnapshot.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "systemexperimentconfiguration.h"
#include "systemscanplan.h"
#include "pulseramplitudescan.h"
#include "../common/experiment/scanexecutionjournal.h"

namespace GetDAQTestInternal {

void RunNativeScanRunFinishesReceiverAndCatalogAfterEventTarget()
{
    NativeScanRunFixture fixture;
    QTemporaryDir directory;
    QVERIFY(directory.isValid());

    GetDAQ::SystemExperimentArtifact baseline;
    baseline.profileSha256=GetDAQ::ConfigurationSha256("native-scan-e2e-profile");
    GetDAQ::ExperimentConfigurationSource source;
    source.path="native-scan-e2e.xcfg";
    source.sha256=GetDAQ::ConfigurationSha256("native-scan-e2e-source");
    baseline.sources.push_back(source);
    GetDAQ::SystemExperimentCoBo cobo;
    cobo.id="C0";
    cobo.configure=GetDAQTest::Configure(1);
    cobo.prepare.inputs.asadMask=cobo.prepare.clock.asadMask=
        cobo.prepare.monitoring.asadMask=cobo.prepare.calibration.asadMask=1;
    cobo.prepare.clock.allowNominalDifference=true;
    GetDAQ::AsAdInputBoardSettings inputs;inputs.asadId=0;inputs.positions[0].active=true;
    cobo.prepare.inputs.boards.push_back(inputs);
    GetDAQ::AsAdBoardClockSettings clock;clock.asadId=0;clock.delayPs=-5335;
    cobo.prepare.clock.boards.push_back(clock);
    GetDAQ::AsAdMonitoringBoardSettings monitoring;monitoring.asadId=0;
    monitoring.externalTemperatureLower=-10;cobo.prepare.monitoring.boards.push_back(monitoring);
    GetDAQ::AsAdCalibrationSettings calibration;calibration.asadId=0;
    cobo.prepare.calibration.boards.push_back(calibration);
    cobo.prepare.calibration.chipVersions[0]=0x202;
    cobo.configure.readout.aget.allowReg2BankResetDifference=true;
    baseline.settings.cobos.push_back(cobo);

    const auto points=GetDAQ::BuildPulserAmplitudeScan(baseline,"C0",-1000,-999,1);
    QCOMPARE(points.size(),std::size_t(2));
    std::map<std::string,std::string> artifacts;
    std::ostringstream manifest;
    manifest << "{\"schema_version\":1,\"kind\":\"getdaq.pulser-amplitude-scan\","
             << "\"target_cobo\":\"C0\",\"start_mv\":-1000,\"stop_mv\":-999,\"step_mv\":1,"
             << "\"hardware_loaded_verified\":false,\"requires_full_configure_per_point\":true,"
             << "\"acquisition_event_count_unspecified\":true,\"points\":[";
    for(std::size_t i=0;i<points.size();++i) {
        const std::string name="point-"+std::to_string(i)+".json";
        const std::string bytes=GetDAQ::EncodeSystemExperimentArtifact(points[i].artifact);
        artifacts[name]=bytes;
        if(i)manifest << ',';
        manifest << "{\"index\":" << i << ",\"voltage_mv\":" << points[i].voltageMv
            << ",\"artifact\":" << GetDAQ::EncodeJsonString(name)
            << ",\"configuration_sha256\":" << GetDAQ::EncodeJsonString(points[i].artifact.configurationSha256)
            << ",\"artifact_sha256\":" << GetDAQ::EncodeJsonString(GetDAQ::ConfigurationSha256(bytes)) << "}";
    }
    manifest << "]}";
    const std::string planBytes=manifest.str();
    const auto loadArtifact=[&artifacts](const std::string &name){return artifacts.at(name);};
    const auto summary=GetDAQ::ValidateSystemScanPlan(planBytes,loadArtifact);
    const auto policy=GetDAQ::ScanExecutionPolicy::ForEventCount(planBytes,summary,2,10000);
    const auto snapshot=GetDAQ::ScanExecutionSnapshot::Create(
        std::string("review-")+GetDAQ::ConfigurationSha256(planBytes).substr(0,56),
        planBytes,policy.Encode(),loadArtifact);
    const std::string snapshotBytes=snapshot.Encode();
    fixture.configuration=QString::fromStdString(snapshot.Points().at(0).configurationSha256);
    fixture.artifact=QString::fromStdString(snapshot.Points().at(0).artifactSha256);
    const QString planDirectory=directory.filePath("plan");
    QVERIFY(QDir().mkpath(planDirectory));
    auto writeFile=[](const QString &path,const QByteArray &bytes) {
        QFile file(path);
        return file.open(QIODevice::WriteOnly|QIODevice::NewOnly) &&
            file.write(bytes)==bytes.size() && file.flush();
    };
    QVERIFY(writeFile(QDir(planDirectory).filePath("scan-manifest.json"),QByteArray::fromStdString(planBytes)));
    QVERIFY(writeFile(QDir(planDirectory).filePath("scan-execution-policy.json"),QByteArray::fromStdString(policy.Encode())));
    for(const auto &entry:artifacts)
        QVERIFY(writeFile(QDir(planDirectory).filePath(QString::fromStdString(entry.first)),QByteArray::fromStdString(entry.second)));

    const QString reviewStorage=directory.filePath("scan-review");
    const ScanPlanReviewResult scanReview=RecordScanPlanReview(
        planDirectory,reviewStorage,
        QString::fromStdString(GetDAQ::ConfigurationSha256(planBytes)),
        static_cast<int>(summary.pointCount));
    const QString journalPath=scanReview.journalPath;
    QCOMPARE(LoadScanPlanRuntimeContext(planDirectory,reviewStorage,
        QString::fromStdString(GetDAQ::ConfigurationSha256(planBytes)),
        static_cast<int>(summary.pointCount)).state,QStringLiteral("Reviewed"));

    RunConfig config=GetDAQTestSupport::Config(directory.path());
    config.nodes.removeLast();
    config.nodes[0].name="CoBo0";config.nodes[0].nativeSenderId="C0";config.nodes[0].nativeAgentId="A0";
    config.simulation=false;config.nativeEcc=true;config.runNumber=1;
    config.nativeCatalog=true;
    config.outputDir=directory.filePath("runs");config.configDir=directory.filePath("configs");
    config.nativeRecordDir=directory.filePath("native-records");
    config.nativeClient.address="127.0.0.1";config.nativeClient.port=fixture.server.serverPort();
    config.nativeClient.serviceId="scan-ecc";config.nativeClient.clientId="scan-console";
    config.nativeClient.clientEpoch=fixture.clientEpoch;config.nativeClient.snapshotSha256=fixture.snapshot;
    config.nativeClient.inputBindingSha256=fixture.inputBinding;
    config.nativeClient.credentialFile=fixture.CredentialPath();
    config.nativeClient.receiverStatusEnabled=true;config.nativeClient.timeoutMs=500;
    config.nativeClient.statusMaxAgeMs=5000;config.pollMs=500;config.stepTimeoutMs=3000;
    QVERIFY(QDir().mkpath(config.outputDir));QVERIFY(QDir().mkpath(config.configDir));
    QString configError;QVERIFY2(config.Validate(&configError),qPrintable(configError));

    NativeRunControl control;
    QSignalSpy controlChanges(&control,&NativeRunControl::ControlSignal);
    QSignalSpy identityBound(&control,&NativeRunControl::ScanRuntimeIdentityBoundSignal);
    QSignalSpy baselineBound(&control,&NativeRunControl::ScanEventCountBaselineBoundSignal);
    QSignalSpy runStatus(&control,&NativeRunControl::ScanPointRunStatusSignal);
    QSignalSpy runFinished(&control,&NativeRunControl::RunFinishedSignal);
    QSignalSpy logs(&control,&NativeRunControl::LogSignal);
    bool receiverObserved=false;
    QObject::connect(&control,&NativeRunControl::StatusSignal,&control,[&receiverObserved](const QVector<NodeStatus> &rows) {
        for(const auto &row:rows) {
            receiverObserved=receiverObserved || (row.nativeReceiverObserved && row.nativePid==4242);
        }
    });
    control.Connect(config);
    QTRY_VERIFY_WITH_TIMEOUT(!controlChanges.isEmpty() &&
        controlChanges.last().at(1).toBool() && !controlChanges.last().at(2).toBool(),5000);
    QTRY_VERIFY_WITH_TIMEOUT(fixture.receiverObservationRequests>0,5000);
    QTest::qWait(25);

    const auto queryFreshScanIdentity=[&]() {
        QElapsedTimer wait;wait.start();
        while(wait.elapsed()<5000) {
            const int previous=identityBound.size();
            control.QueryScanRuntimeIdentity(journalPath,planDirectory,QByteArray::fromStdString(snapshotBytes),0);
            while(wait.elapsed()<5000 && identityBound.size()==previous)QTest::qWait(20);
            if(identityBound.size()>previous && identityBound.last().at(2).toString().isEmpty())return true;
            QTest::qWait(25);
        }
        return false;
    };
    QVERIFY(queryFreshScanIdentity());
    QStringList logMessages;for(const auto &row:logs)logMessages.append(row.at(0).toString());
    const QString identityError=identityBound.last().at(2).toString();
    QVERIFY2(identityError.isEmpty(),qPrintable(identityError+QStringLiteral(" | ")+logMessages.join(QStringLiteral(" | "))));
    QCOMPARE(fixture.runtimeIdentityRequests,1);

    const QStringList lifecycleCommands{QStringLiteral("describe"),QStringLiteral("prepare"),QStringLiteral("configure")};
    for (int i=0;i<lifecycleCommands.size();++i) {
        const int previousStatusObservations=fixture.statusObservationRequests;
        const int previousReceiverObservations=fixture.receiverObservationRequests;
        QVERIFY2(queryFreshScanIdentity(),qPrintable(identityBound.last().at(2).toString()));
        const int previousStatusCount=runStatus.size();
        control.ExecuteScanPointLifecycleOperation(journalPath,lifecycleCommands.at(i));
        QTRY_VERIFY_WITH_TIMEOUT(runStatus.size()>previousStatusCount &&
            runStatus.last().at(0).toString().contains(QStringLiteral("completed and was confirmed by fresh ECC state")),5000);
        QTRY_VERIFY_WITH_TIMEOUT(fixture.statusObservationRequests>previousStatusObservations &&
            fixture.receiverObservationRequests>previousReceiverObservations,5000);
    }
    QCOMPARE(fixture.commands,lifecycleCommands);
    QCOMPARE(fixture.controllerState,4U);
    QTRY_VERIFY_WITH_TIMEOUT(receiverObserved,5000);

    QVERIFY2(queryFreshScanIdentity(),qPrintable(identityBound.last().at(2).toString()));
    control.QueryScanEventCountBaseline(journalPath);
    QTRY_COMPARE_WITH_TIMEOUT(baselineBound.size(),1,5000);
    QVERIFY2(baselineBound.last().at(2).toString().isEmpty(),qPrintable(baselineBound.last().at(2).toString()));
    QCOMPARE(GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString())->Current().EventCountBaseline().acquisitionEvents,std::uint64_t(100));

    control.StartScanPoint(journalPath);
    QElapsedTimer finalizationWait;finalizationWait.start();
    const auto finalStatusReady=[&runStatus]() {
        return !runStatus.isEmpty() && !runStatus.last().at(1).toBool() &&
            runStatus.last().at(0).toString().contains(QStringLiteral("receiver seal and configured catalog are complete"));
    };
    while(!finalStatusReady() && finalizationWait.elapsed()<15000)QTest::qWait(25);
    if(!finalStatusReady()) {
        QStringList diagnostic;
        for(const auto &row:runStatus)diagnostic.append(QStringLiteral("scan: ")+row.at(0).toString());
        for(const auto &row:logs)diagnostic.append(QStringLiteral("log: ")+row.at(0).toString());
        QFAIL(qPrintable(diagnostic.join(QStringLiteral("\n"))));
    }
    QCOMPARE(fixture.runStarts,1);QCOMPARE(fixture.runStops,1);
    QCOMPARE(fixture.commands,QStringList({QStringLiteral("describe"),QStringLiteral("prepare"),
        QStringLiteral("configure"),QStringLiteral("start"),QStringLiteral("stop"),
        QStringLiteral("breakup"),QStringLiteral("catalog")}));
    QVERIFY(fixture.receiverFinished);QVERIFY(fixture.catalogued);
    QCOMPARE(runFinished.size(),1);QCOMPARE(runFinished.last().at(0).toInt(),2);
    QVERIFY(fixture.eventRequests>=2);
    control.Shutdown();
    const auto completed=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString())->Current();
    QCOMPARE(completed.State(),GetDAQ::ScanPointState::CatalogVerified);
    QVERIFY(!completed.HasPendingOperation());QVERIFY(!completed.PendingOperationUnknown());
    const QString bindingCatalog=directory.filePath("binding-catalog");
    const QString bindingDirectory=directory.filePath("bindings");
    QVERIFY(QDir().mkpath(bindingCatalog));QVERIFY(QDir().mkpath(bindingDirectory));
    const QString bindingPath=QDir(bindingDirectory).filePath("point-000.json");
    const QString bindingOperation=QStringLiteral("scan-bind-test-001");
    const QString planSha=QString::fromStdString(GetDAQ::ConfigurationSha256(planBytes));
    std::unique_ptr<ScanPointBindingSession> bindingSession=BeginScanPointBinding(
        planDirectory,reviewStorage,planSha,static_cast<int>(summary.pointCount),
        0,bindingCatalog,bindingPath,bindingOperation);
    const ScanPlanRecoveryStatus bindingIntent=bindingSession->Current();
    QCOMPARE(bindingIntent.state,QStringLiteral("CatalogVerified"));
    QCOMPARE(bindingIntent.operationId,bindingOperation);
    const ScanPlanOutputSummary bindingPlan=InspectScanPlanOutput(planDirectory);
    const QByteArray bindingDigest=QByteArray::fromStdString(
        snapshot.Points().at(0).artifactSha256);
    const QByteArray configurationDigest=QByteArray::fromStdString(
        snapshot.Points().at(0).configurationSha256);
    const QJsonObject bindingObject{{QStringLiteral("schema"),1},
        {QStringLiteral("kind"),QStringLiteral("getdaq.scan-run-binding")},
        {QStringLiteral("scan_kind"),bindingPlan.kind},
        {QStringLiteral("target_cobo"),bindingPlan.targetCoBo},
        {QStringLiteral("point_index"),0},
        {QStringLiteral("point_artifact"),QStringLiteral("point-000.json")},
        {QStringLiteral("configuration_sha256"),QString::fromLatin1(configurationDigest)},
        {QStringLiteral("artifact_sha256"),QString::fromLatin1(bindingDigest)},
        {QStringLiteral("scan_manifest_sha256"),planSha},
        {QStringLiteral("experiment"),QStringLiteral("scan-fixture")},
        {QStringLiteral("run_number"),1},
        {QStringLiteral("session_id"),fixture.session},
        {QStringLiteral("catalog_directory"),bindingCatalog},
        {QStringLiteral("catalog_request_key"),QString::fromStdString(fixture.catalog.requestKey)},
        {QStringLiteral("catalog_manifest_sha256"),QString::fromStdString(fixture.catalog.manifestSha256)},
        {QStringLiteral("catalog_receipt_sha256"),QString::fromStdString(
            GetDAQ::ConfigurationSha256("fixture-catalog-receipt"))},
        {QStringLiteral("hardware_loaded_verified"),false}};
    const QByteArray bindingBytes=QJsonDocument(bindingObject).toJson(QJsonDocument::Compact)+"\n";
    QFile bindingFile(bindingPath);
    QVERIFY(bindingFile.open(QIODevice::WriteOnly|QIODevice::NewOnly));
    QCOMPARE(bindingFile.write(bindingBytes),qint64(bindingBytes.size()));
    QVERIFY(bindingFile.flush());bindingFile.close();
    const ScanPlanRecoveryStatus bindingComplete=bindingSession->Complete(bindingBytes);
    QCOMPARE(bindingComplete.state,QStringLiteral("Bound"));
    bindingSession.reset();
    const auto bound=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString())->Current();
    QCOMPARE(bound.State(),GetDAQ::ScanPointState::Bound);
    QVERIFY(!bound.HasPendingOperation());
    bool duplicateBindRejected=false;
    try { BeginScanPointBinding(planDirectory,reviewStorage,planSha,
        static_cast<int>(summary.pointCount),0,bindingCatalog,
        directory.filePath("bindings/duplicate.json"),QStringLiteral("scan-bind-test-002")); }
    catch (const std::exception &) { duplicateBindRejected=true; }
    QVERIFY(duplicateBindRejected);
    QFile runJournal(QDir(config.RunDirectory()).filePath("journal.json"));
    QVERIFY(runJournal.open(QIODevice::ReadOnly));
    const auto runRecord=QJsonDocument::fromJson(runJournal.readAll()).object();
    QVERIFY(runRecord.value("scan_point_event_target_reached").toBool());
    QCOMPARE(runRecord.value("phase").toString(),QStringLiteral("Complete"));
}

} // namespace GetDAQTestInternal
