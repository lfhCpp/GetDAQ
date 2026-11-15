#include "scanplan_builder_gui_tests_p.h"

#include <QtTest>
#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTimer>
#include <QWidget>

#include <cstddef>
#include <exception>
#include <string>

#include "cobo_configure_fixture.h"
#include "configurationsha256.h"
#include "mainwindow.h"
#include "scanexecutionjournalstore.h"
#include "scanplanoutput.h"
#include "scanplanreview.h"
#include "scanplanwindow.h"
#include "systemexperimentconfiguration.h"

namespace GetDAQTestInternal {

void RunOfflineScanPlanBuilderIsAvailableFromConsole()
{
    MainWindow window;
    const auto action=window.findChild<QAction *>("NativeOfflineScanPlans");
    QVERIFY(action && action->isEnabled());
    action->trigger();
    const auto dialog=window.findChild<ScanPlanWindow *>("NativeOfflineScanPlanWindow");
    QVERIFY(dialog);
    const auto scroll=dialog->findChild<QScrollArea *>();
    QVERIFY(scroll && scroll->widget());
    QVERIFY(dialog->findChild<QLabel *>("ScanPlanHeading"));
    QVERIFY(dialog->findChild<QLabel *>("ScanPlanIntro"));
    QVERIFY(dialog->windowTitle().contains(QStringLiteral("Run control")));
    const auto scanIntro=dialog->findChild<QLabel *>("ScanPlanIntro");
    QVERIFY(scanIntro->text().contains(QStringLiteral("connect to ECC")));
    QVERIFY(scanIntro->text().contains(QStringLiteral("receiver service")));
    QVERIFY(!scanIntro->text().contains(QStringLiteral("does not connect to ECC")));
    QVERIFY(dialog->minimumWidth() >= 760 && dialog->minimumHeight() >= 600);
    const auto kind=dialog->findChild<QComboBox *>("ScanKind");
    const auto create=dialog->findChild<QPushButton *>("CreateScanPlan");
    const auto runtimeIdentity=dialog->findChild<QPushButton *>("QueryScanRuntimeIdentity");
    const auto eventBaseline=dialog->findChild<QPushButton *>("CaptureScanEventBaseline");
    const auto startScanPoint=dialog->findChild<QPushButton *>("StartScanPoint");
    const auto scanRunScope=dialog->findChild<QLabel *>("ScanRunScopeNotice");
    const auto currentPointState=dialog->findChild<QLabel *>("CurrentScanPointState");
    const auto bindRun=dialog->findChild<QPushButton *>("BindCompletedScanRun");
    const auto summarizeRuns=dialog->findChild<QPushButton *>("SummarizeCompletedScanRuns");
    QVERIFY(kind && create && create->isEnabled());
    QVERIFY(bindRun && summarizeRuns && runtimeIdentity && eventBaseline && startScanPoint && scanRunScope && currentPointState);
    QVERIFY(currentPointState->text().contains(QStringLiteral("no plan loaded")));
    QVERIFY(scanRunScope->text().contains(QStringLiteral("already be loaded")));
    QVERIFY(scanRunScope->text().contains(QStringLiteral("whole ECC system")));
    QVERIFY(scanRunScope->text().contains(QStringLiteral("does not load point settings")));
    QVERIFY(startScanPoint->text().contains(QStringLiteral("currently configured point")));
    QVERIFY(startScanPoint->toolTip().contains(QStringLiteral("Enable native_catalog")));
    dialog->SetNativeCatalogEnabled(true);
    QVERIFY(startScanPoint->toolTip().contains(QStringLiteral("does not Configure or load point settings")));
    QVERIFY(startScanPoint->toolTip().contains(QStringLiteral("advance the plan")));
    QVERIFY(!bindRun->isEnabled() && !summarizeRuns->isEnabled());
    QVERIFY(!runtimeIdentity->isEnabled() && !eventBaseline->isEnabled() && !startScanPoint->isEnabled());
    QCOMPARE(kind->count(),4);
    QVERIFY(!dialog->findChild<QLineEdit *>("ScanArtifactPath")->isReadOnly());
    QCOMPARE(window.findChild<QLabel *>("Phase")->text(),QStringLiteral("Disconnected"));
    QTemporaryDir temporary(QDir(QDir::tempPath()).filePath(
        QStringLiteral("getdaq scan UI-XXXXXX")));
    QVERIFY(temporary.isValid());
    const QString artifactPath=temporary.filePath(QStringLiteral("reviewed system artifact.json"));
    GetDAQ::SystemExperimentArtifact systemArtifact;
    systemArtifact.profileSha256=GetDAQ::ConfigurationSha256("scan-window-test-profile");
    GetDAQ::ExperimentConfigurationSource source;
    source.path="scan-window-test.xcfg";
    source.sha256=GetDAQ::ConfigurationSha256("scan-window-test-source");
    systemArtifact.sources.push_back(source);
    GetDAQ::SystemExperimentCoBo cobo;
    cobo.id="C0";
    cobo.configure=GetDAQTest::Configure(1);
    cobo.prepare.inputs.asadMask=1;
    cobo.prepare.clock.asadMask=1;
    cobo.prepare.monitoring.asadMask=1;
    cobo.prepare.calibration.asadMask=1;
    cobo.prepare.clock.allowNominalDifference=true;
    cobo.prepare.clock.boards.clear();
    GetDAQ::AsAdInputBoardSettings inputs;
    inputs.asadId=0;
    inputs.positions[0].active=true;
    cobo.prepare.inputs.boards.push_back(inputs);
    GetDAQ::AsAdBoardClockSettings clock;
    clock.asadId=0;
    clock.delayPs=-5335;
    cobo.prepare.clock.boards.push_back(clock);
    GetDAQ::AsAdMonitoringBoardSettings monitoring;
    monitoring.asadId=0;
    monitoring.externalTemperatureLower=-10;
    cobo.prepare.monitoring.boards.push_back(monitoring);
    GetDAQ::AsAdCalibrationSettings calibration;
    calibration.asadId=0;
    cobo.prepare.calibration.boards.push_back(calibration);
    cobo.prepare.calibration.chipVersions[0]=0x202;
    cobo.configure.readout.aget.allowReg2BankResetDifference=true;
    systemArtifact.settings.cobos.push_back(cobo);
    QFile artifact(artifactPath);
    QVERIFY(artifact.open(QIODevice::WriteOnly));
    const QByteArray artifactBytes=QByteArray::fromStdString(
        GetDAQ::EncodeSystemExperimentArtifact(systemArtifact));
    QCOMPARE(artifact.write(artifactBytes),qint64(artifactBytes.size()));
    artifact.close();
    const QString outputParent=temporary.filePath(QStringLiteral("output parent"));
    QVERIFY(QDir().mkpath(outputParent));
    dialog->findChild<QLineEdit *>("ScanArtifactPath")->setText(artifactPath);
    dialog->findChild<QLineEdit *>("ScanTargetCoBo")->setText(QStringLiteral("C0"));
    dialog->findChild<QLineEdit *>("ScanOutputParent")->setText(outputParent);
    dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("must not be published"));
    QString cliName=QStringLiteral("getdaq-config");
#ifdef Q_OS_WIN
    cliName+=QStringLiteral(".exe");
#endif
    const bool siblingCliAvailable=QFileInfo(QDir(QCoreApplication::applicationDirPath()).filePath(cliName)).isExecutable();
    auto validateWithNativeCli = [&](const QString &planDirectory) {
        QString checkerName=QStringLiteral("getdaq-config-check");
#ifdef Q_OS_WIN
        checkerName+=QStringLiteral(".exe");
#endif
        const QString checker=QDir(QCoreApplication::applicationDirPath()).filePath(checkerName);
        if (!QFileInfo(checker).isExecutable()) return true; // Other test entry points build the checker separately.
        QProcess process;
        process.start(checker,QStringList()<<QStringLiteral("scan-plan")<<planDirectory);
        if (!process.waitForStarted(3000) || !process.waitForFinished(10000)) return false;
        return process.exitStatus()==QProcess::NormalExit && process.exitCode()==0 &&
               process.readAllStandardOutput().contains("\"valid\":true");
    };
    create->click();
    if(siblingCliAvailable) {
        QLabel *status=dialog->findChild<QLabel *>("ScanPlanStatus");
        const QString outputDirectory=QDir(outputParent).filePath(QStringLiteral("must not be published"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("The plan contains offline settings only.")),5000);
        QVERIFY(status->text().contains(QStringLiteral("Scan kind: getdaq.peaking-time-scan")));
        QVERIFY(status->text().contains(QStringLiteral("Target CoBo: C0")));
        QVERIFY(status->text().contains(QStringLiteral("Configuration points: 16")));
        QVERIFY(status->text().contains(QStringLiteral("Point files present: 16/16")));
        QVERIFY2(currentPointState->text().contains(QStringLiteral("confirm review to create its execution journal")),
                 qPrintable(currentPointState->text()));
        QVERIFY(!bindRun->isEnabled() && summarizeRuns->isEnabled());
        QVERIFY(QFileInfo(QDir(outputDirectory).filePath(QStringLiteral("scan-manifest.json"))).isFile());
        QVERIFY(QFileInfo(QDir(outputDirectory).filePath(QStringLiteral("point-00.json"))).isFile());
        QVERIFY(QFileInfo(QDir(outputDirectory).filePath(QStringLiteral("point-15.json"))).isFile());
        QVERIFY(validateWithNativeCli(outputDirectory));
        const ScanPlanOutputSummary validatedPlan=InspectScanPlanOutput(outputDirectory);

        QPushButton *createPolicy=dialog->findChild<QPushButton *>("CreateScanExecutionPolicy");
        QSpinBox *policyEvents=dialog->findChild<QSpinBox *>("PolicyEventsPerPoint");
        QSpinBox *policyDuration=dialog->findChild<QSpinBox *>("PolicyMaximumDurationMs");
        QVERIFY(createPolicy && createPolicy->isEnabled());
        QVERIFY(policyEvents && policyDuration);
        QVERIFY(dialog->findChild<QWidget *>("EventCountPolicyControls")->isVisible());
        QCOMPARE(policyEvents->value(),0);
        QCOMPARE(policyDuration->value(),0);
        createPolicy->click();
        QVERIFY(status->text().contains(QStringLiteral("Choose an event count")));
        QVERIFY(!QFileInfo(QDir(outputDirectory).filePath(
            QStringLiteral("scan-execution-policy.json"))).exists());
        policyEvents->setValue(100000);
        policyDuration->setValue(3600000);
        QTimer::singleShot(0, [] {
            QMessageBox *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if(box) box->button(QMessageBox::Yes)->click();
        });
        createPolicy->click();
        const QString policyPath=QDir(outputDirectory).filePath(QStringLiteral("scan-execution-policy.json"));
        QElapsedTimer policyWait;
        policyWait.start();
        while (!status->text().contains(QStringLiteral("created and independently validated")) &&
               policyWait.elapsed() < 12000) {
            QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
            QTest::qWait(10);
        }
        QVERIFY2(status->text().contains(QStringLiteral("created and independently validated")),
                 qPrintable(status->text()));
        QVERIFY(QFileInfo(policyPath).isFile());
        QVERIFY(!createPolicy->isEnabled());
        QFile policyFile(policyPath);
        QVERIFY(policyFile.open(QIODevice::ReadOnly));
        const QJsonObject policyObject=QJsonDocument::fromJson(policyFile.readAll()).object();
        QCOMPARE(policyObject.value(QStringLiteral("stopping_strategy")).toString(),
                 QStringLiteral("event_count_per_point"));
        QCOMPARE(policyObject.value(QStringLiteral("events_per_point")).toInt(),100000);
        QCOMPARE(policyObject.value(QStringLiteral("maximum_duration_ms_per_point")).toInt(),3600000);
        QCOMPARE(policyObject.value(QStringLiteral("source_plan_sha256")).toString(),validatedPlan.planSha256);
        QVERIFY(status->text().contains(QStringLiteral("no Configure or Run command was sent")));

        const QString snapshotPath=QDir(outputDirectory).filePath(QStringLiteral("scan-execution-snapshot.json"));
        QProcess snapshotCreator;
        snapshotCreator.start(QDir(QCoreApplication::applicationDirPath()).filePath(cliName),
            QStringList()<<QStringLiteral("create-scan-snapshot")<<outputDirectory<<policyPath
                <<QStringLiteral("ui-scan-test-001")<<snapshotPath);
        QVERIFY(snapshotCreator.waitForStarted(3000));
        QVERIFY(snapshotCreator.waitForFinished(10000));
        QCOMPARE(snapshotCreator.exitStatus(),QProcess::NormalExit);
        QCOMPARE(snapshotCreator.exitCode(),0);
        QVERIFY(QFileInfo(snapshotPath).isFile());
        const QJsonObject snapshotCreated=QJsonDocument::fromJson(
            snapshotCreator.readAllStandardOutput()).object();
        QVERIFY(snapshotCreated.value(QStringLiteral("created")).toBool());
        QCOMPARE(snapshotCreated.value(QStringLiteral("execution_id")).toString(),
                 QStringLiteral("ui-scan-test-001"));
        QCOMPARE(snapshotCreated.value(QStringLiteral("point_count")).toInt(),16);
        QVERIFY(!snapshotCreated.value(QStringLiteral("hardware_command_sent")).toBool());
        QFile snapshotFile(snapshotPath);
        QVERIFY(snapshotFile.open(QIODevice::ReadOnly));
        const QByteArray snapshotBeforeDuplicate=snapshotFile.readAll();
        snapshotFile.close();
        QProcess duplicateSnapshotCreator;
        duplicateSnapshotCreator.start(QDir(QCoreApplication::applicationDirPath()).filePath(cliName),
            QStringList()<<QStringLiteral("create-scan-snapshot")<<outputDirectory<<policyPath
                <<QStringLiteral("ui-scan-test-002")<<snapshotPath);
        QVERIFY(duplicateSnapshotCreator.waitForStarted(3000));
        QVERIFY(duplicateSnapshotCreator.waitForFinished(10000));
        QVERIFY(duplicateSnapshotCreator.exitCode()!=0);
        QVERIFY(snapshotFile.open(QIODevice::ReadOnly));
        QCOMPARE(snapshotFile.readAll(),snapshotBeforeDuplicate);
        snapshotFile.close();
        QString checkerName=QStringLiteral("getdaq-config-check");
#ifdef Q_OS_WIN
        checkerName+=QStringLiteral(".exe");
#endif
        const QString checkerPath=QDir(QCoreApplication::applicationDirPath()).filePath(checkerName);
        if(QFileInfo(checkerPath).isExecutable()) {
            QProcess snapshotChecker;
            snapshotChecker.start(checkerPath,QStringList()<<QStringLiteral("scan-execution-snapshot")
                <<outputDirectory<<policyPath<<snapshotPath);
            QVERIFY(snapshotChecker.waitForStarted(3000));
            QVERIFY(snapshotChecker.waitForFinished(10000));
            QCOMPARE(snapshotChecker.exitStatus(),QProcess::NormalExit);
            QCOMPARE(snapshotChecker.exitCode(),0);
            QVERIFY(snapshotChecker.readAllStandardOutput().contains("\"valid\":true"));
        }

        QPushButton *recordReview=dialog->findChild<QPushButton *>("RecordScanPlanReview");
        QVERIFY(recordReview && recordReview->isEnabled());
        const QString reviewStorage=temporary.filePath(QStringLiteral("operator data"));
        dialog->SetReviewStorageDirectory(reviewStorage);
        bool stalePlanRejected=false;
        try {
            RecordScanPlanReview(outputDirectory,reviewStorage,
                QString(64,QLatin1Char('0')),16);
        } catch (const std::exception &) { stalePlanRejected=true; }
        QVERIFY(stalePlanRejected);
        QTimer::singleShot(0, [] {
            QMessageBox *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if(box) box->button(QMessageBox::Yes)->click();
        });
        recordReview->click();
        QVERIFY2(status->text().contains(QStringLiteral("Plan review recorded.")),
                 qPrintable(status->text()));
        const ScanPlanReviewResult review=RecordScanPlanReview(outputDirectory,
            reviewStorage,validatedPlan.planSha256,16);
        QVERIFY(review.alreadyReviewed);
        QVERIFY(QFileInfo(review.journalPath).isFile());
        auto reviewJournal=GetDAQ::ScanExecutionJournalStore::Open(
            review.journalPath.toUtf8().toStdString());
        QCOMPARE(reviewJournal->Current().State(),GetDAQ::ScanPointState::Reviewed);
        QCOMPARE(reviewJournal->Current().PointCount(),16U);
        QCOMPARE(reviewJournal->Current().SourceSnapshotSha256().size(),std::size_t(64));
        QVERIFY(currentPointState->text().contains(QStringLiteral("Current point 1 of 16 — Reviewed")));
        const QString privateSnapshot=QDir(reviewStorage).filePath(
            QStringLiteral("scan-journals/snapshot-%1.json")
                .arg(validatedPlan.planSha256.left(56)));
        QVERIFY(QFileInfo(privateSnapshot).isFile());
        reviewJournal.reset();
        const ScanPlanReviewResult duplicateReview=RecordScanPlanReview(outputDirectory,
            reviewStorage,validatedPlan.planSha256,16);
        QVERIFY(duplicateReview.alreadyReviewed);
        const std::string journalBeforeRuntimeContext =
            GetDAQ::ScanExecutionJournalStore::Open(
                review.journalPath.toUtf8().toStdString())->Current().Encode();
        const ScanPlanRuntimeContext runtimeContext = LoadScanPlanRuntimeContext(
            outputDirectory,reviewStorage,validatedPlan.planSha256,16);
        QVERIFY(runtimeContext.hasJournal && !runtimeContext.hasPendingOperation &&
                !runtimeContext.runtimeIdentityBound &&
                !runtimeContext.eventCountBaselineBound &&
                runtimeContext.snapshotBytes.size() > 0);
        QCOMPARE(runtimeContext.state,QStringLiteral("Reviewed"));
        QCOMPARE(runtimeContext.pointIndex,0);
        QCOMPARE(GetDAQ::ScanExecutionJournalStore::Open(
            review.journalPath.toUtf8().toStdString())->Current().Encode(),
            journalBeforeRuntimeContext);

        QSignalSpy adoptionRequested(dialog,
            &ScanPlanWindow::QueryScanRuntimeIdentityRequested);
        QTimer::singleShot(0, [] {
            QMessageBox *box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if(box)box->button(QMessageBox::Yes)->click();
        });
        runtimeIdentity->click();
        QTRY_COMPARE(adoptionRequested.count(),1);
        QCOMPARE(adoptionRequested.takeFirst().at(0).toString(),runtimeContext.journalPath);
        QVERIFY(status->text().contains(QStringLiteral("Verifying the externally started ECC")) ||
                status->text().contains(QStringLiteral("not accepted")));
        QVERIFY2(status->text().contains(QStringLiteral("point 1.")),
                 qPrintable(status->text()));
        QCOMPARE(LoadScanPlanRuntimeContext(outputDirectory,reviewStorage,
            validatedPlan.planSha256,16).state,QStringLiteral("Reviewed"));

        {
            auto journalStore=GetDAQ::ScanExecutionJournalStore::Open(
                review.journalPath.toUtf8().toStdString());
            const auto pending=journalStore->Current().WithOperationIntent(
                "gui-recovery-check", "start-ecc", QString(64,'b').toStdString());
            journalStore->Persist(pending);
        }
        const ScanPlanRecoveryStatus unknown=InspectScanPlanRecovery(
            outputDirectory,reviewStorage,validatedPlan.planSha256,16);
        QVERIFY(unknown.hasJournal && unknown.pendingUnknown);
        QCOMPARE(unknown.operation,QStringLiteral("start-ecc"));
        QCOMPARE(unknown.operationId,QStringLiteral("gui-recovery-check"));
        QVERIFY_EXCEPTION_THROWN(
            ReconcileScanPlanOperation(outputDirectory,reviewStorage,
                validatedPlan.planSha256,16,unknown.operationId,
                QStringLiteral("operator-1"),false,QString()),
            std::exception);
        QVERIFY(InspectScanPlanRecovery(outputDirectory,reviewStorage,
            validatedPlan.planSha256,16).pendingUnknown);
        const ScanPlanRecoveryStatus reconciled=ReconcileScanPlanOperation(
            outputDirectory,reviewStorage,validatedPlan.planSha256,16,
            unknown.operationId,QStringLiteral("operator-1"),false,
            QStringLiteral("Independent ECC request log showed no submission."));
        QVERIFY(!reconciled.pendingUnknown);
        QCOMPARE(reconciled.state,QStringLiteral("Reviewed"));
        QVERIFY_EXCEPTION_THROWN(
            ReconcileScanPlanOperation(outputDirectory,reviewStorage,
                validatedPlan.planSha256,16,unknown.operationId,
                QStringLiteral("operator-1"),true,QStringLiteral("stale retry")),
            std::exception);

        {
            auto journalStore=GetDAQ::ScanExecutionJournalStore::Open(
                review.journalPath.toUtf8().toStdString());
            journalStore->Persist(journalStore->Current().WithOperationIntent(
                "gui-applied-check", "start-ecc", QString(64,'d').toStdString()));
        }

        ScanPlanWindow reopenedDialog;
        reopenedDialog.SetReviewStorageDirectory(reviewStorage);
        reopenedDialog.show();
        QPushButton *openExisting=reopenedDialog.findChild<QPushButton *>("OpenExistingScanPlan");
        QVERIFY(openExisting && openExisting->isEnabled());
        auto choosePlanDirectory=[](const QString &directory) {
            QTimer::singleShot(0, [directory] {
            QFileDialog *chooser=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if(chooser) {
                chooser->setDirectory(directory);
                chooser->selectFile(directory);
                QMetaObject::invokeMethod(chooser,"accept",Qt::QueuedConnection);
            }
            });
        };
        choosePlanDirectory(outputDirectory);
        openExisting->click();
        QLabel *reopenedStatus=reopenedDialog.findChild<QLabel *>("ScanPlanStatus");
        QPushButton *reopenedReview=reopenedDialog.findChild<QPushButton *>("RecordScanPlanReview");
        QPushButton *recovery=reopenedDialog.findChild<QPushButton *>("ReconcilePendingScanOperation");
        QLabel *reopenedPointState=reopenedDialog.findChild<QLabel *>("CurrentScanPointState");
        QVERIFY(reopenedStatus && reopenedReview);
        QVERIFY(recovery && recovery->isEnabled() && reopenedPointState);
        QCOMPARE(reopenedPointState->property("state").toString(),QStringLiteral("error"));
        QVERIFY(reopenedPointState->text().contains(QStringLiteral("needs reconciliation")));
        QVERIFY(reopenedStatus->text().contains(QStringLiteral("Existing plan opened and fully validated")));
        QVERIFY(reopenedStatus->text().contains(validatedPlan.planSha256));
        QVERIFY(reopenedReview->isEnabled());
        QTimer recoveryDialogDriver;
        recoveryDialogDriver.setInterval(5);
        QObject::connect(&recoveryDialogDriver, &QTimer::timeout, &reopenedDialog, [&]() {
            QWidget *active = QApplication::activeModalWidget();
            if (QMessageBox *box = qobject_cast<QMessageBox *>(active)) {
                for (QAbstractButton *button : box->buttons()) {
                    if (button->text() == QStringLiteral("Confirmed completed")) {
                        button->click();
                        return;
                    }
                }
            } else if (QInputDialog *input = qobject_cast<QInputDialog *>(active)) {
                input->setTextValue(input->windowTitle() ==
                    QStringLiteral("Operator identification")
                    ? QStringLiteral("operator-gui")
                    : QStringLiteral("Independent ECC audit evidence"));
                input->accept();
                return;
            }
            if (reopenedStatus->text().contains(
                    QStringLiteral("Manual reconciliation recorded.")))
                recoveryDialogDriver.stop();
        });
        recoveryDialogDriver.start();
        recovery->click();
        QTRY_VERIFY_WITH_TIMEOUT(reopenedStatus->text().contains(
            QStringLiteral("Manual reconciliation recorded.")),3000);
        recoveryDialogDriver.stop();
        QCOMPARE(InspectScanPlanRecovery(outputDirectory,reviewStorage,
            validatedPlan.planSha256,16).state,QStringLiteral("ECCStarted"));
        const QString invalidPlanDirectory=temporary.filePath(QStringLiteral("invalid-plan"));
        QVERIFY(QDir().mkpath(invalidPlanDirectory));
        choosePlanDirectory(invalidPlanDirectory);
        openExisting->click();
        QVERIFY(!reopenedReview->isEnabled());
        QVERIFY(reopenedStatus->text().contains(QStringLiteral("validation failed")));

        const QByteArray originalManifest=[&]() {
            QFile manifest(QDir(outputDirectory).filePath(QStringLiteral("scan-manifest.json")));
            if(!manifest.open(QIODevice::ReadOnly))return QByteArray();
            return manifest.readAll();
        }();
        QVERIFY(!originalManifest.isEmpty());
        create->click();
        QTRY_VERIFY_WITH_TIMEOUT(status->text().startsWith(QStringLiteral("Plan was not created.")),5000);
        QFile manifest(QDir(outputDirectory).filePath(QStringLiteral("scan-manifest.json")));
        QVERIFY(manifest.open(QIODevice::ReadOnly));
        QCOMPARE(manifest.readAll(),originalManifest);

        QComboBox *scanKind=dialog->findChild<QComboBox *>("ScanKind");
        scanKind->setCurrentIndex(1);
        QCOMPARE(scanKind->currentData().toString(),QStringLiteral("pulser"));
        auto *start=dialog->findChild<QSpinBox *>("ScanStartMv");
        auto *stop=dialog->findChild<QSpinBox *>("ScanStopMv");
        auto *step=dialog->findChild<QSpinBox *>("ScanStepMv");
        QVERIFY(start && stop && step && start->isEnabled() && stop->isEnabled() && step->isEnabled());
        start->setValue(-1000);
        stop->setValue(-900);
        step->setValue(50);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("three-point pulser plan"));
        create->click();
        const QString pulserDirectory=QDir(outputParent).filePath(QStringLiteral("three-point pulser plan"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("The plan contains offline settings only.")),5000);
        QVERIFY(status->text().contains(QStringLiteral("Configuration points: 3")));
        QVERIFY(status->text().contains(QStringLiteral("Point files present: 3/3")));
        QFile pulserManifest(QDir(pulserDirectory).filePath(QStringLiteral("scan-manifest.json")));
        QVERIFY(pulserManifest.open(QIODevice::ReadOnly));
        const QJsonObject pulserPlan=QJsonDocument::fromJson(pulserManifest.readAll()).object();
        pulserManifest.close();
        QCOMPARE(pulserPlan.value(QStringLiteral("kind")).toString(),QStringLiteral("getdaq.pulser-amplitude-scan"));
        const QJsonArray pulserPoints=pulserPlan.value(QStringLiteral("points")).toArray();
        QCOMPARE(pulserPoints.size(),3);
        QCOMPARE(pulserPoints.at(0).toObject().value(QStringLiteral("voltage_mv")).toInt(),-1000);
        QCOMPARE(pulserPoints.at(1).toObject().value(QStringLiteral("voltage_mv")).toInt(),-950);
        QCOMPARE(pulserPoints.at(2).toObject().value(QStringLiteral("voltage_mv")).toInt(),-900);
        QVERIFY(QFileInfo(QDir(pulserDirectory).filePath(QStringLiteral("point-002.json"))).isFile());
        const QByteArray validPulserManifest=QJsonDocument(pulserPlan).toJson(QJsonDocument::Compact);
        QJsonArray alteredPulserPoints=pulserPoints;
        QJsonObject alteredPulserPoint=alteredPulserPoints.at(1).toObject();
        alteredPulserPoint.insert(QStringLiteral("voltage_mv"),-940);
        alteredPulserPoints.replace(1,alteredPulserPoint);
        QJsonObject alteredPulserPlan=pulserPlan;
        alteredPulserPlan.insert(QStringLiteral("points"),alteredPulserPoints);
        QVERIFY(pulserManifest.open(QIODevice::WriteOnly|QIODevice::Truncate));
        QCOMPARE(pulserManifest.write(QJsonDocument(alteredPulserPlan).toJson()),
                 qint64(QJsonDocument(alteredPulserPlan).toJson().size()));
        pulserManifest.close();
        QVERIFY(!InspectScanPlanOutput(pulserDirectory).isValid());
        QVERIFY(!validateWithNativeCli(pulserDirectory));
        QVERIFY(pulserManifest.open(QIODevice::WriteOnly|QIODevice::Truncate));
        QCOMPARE(pulserManifest.write(validPulserManifest),qint64(validPulserManifest.size()));
        pulserManifest.close();
        QVERIFY2(InspectScanPlanOutput(pulserDirectory).isValid(),
                 qPrintable(InspectScanPlanOutput(pulserDirectory).error));
        QVERIFY(validateWithNativeCli(pulserDirectory));

        step->setValue(60);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("invalid pulser plan"));
        create->click();
        QVERIFY(dialog->findChild<QLabel *>("ScanPlanStatus")->text().contains(
            QStringLiteral("must land exactly on the stop value")));
        QVERIFY(!QFileInfo::exists(QDir(outputParent).filePath(QStringLiteral("invalid pulser plan"))));

        kind->setCurrentIndex(2);
        QCOMPARE(kind->currentData().toString(),QStringLiteral("threshold"));
        auto *asad=dialog->findChild<QSpinBox *>("ScanTargetAsAd");
        auto *aget=dialog->findChild<QSpinBox *>("ScanTargetAget");
        QVERIFY(asad && aget && asad->isVisible() && aget->isVisible());
        asad->setValue(0);
        aget->setValue(0);
        start->setValue(0);
        stop->setValue(8);
        step->setValue(4);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("three-point threshold plan"));
        create->click();
        const QString thresholdDirectory=QDir(outputParent).filePath(QStringLiteral("three-point threshold plan"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("The plan contains offline settings only.")),5000);
        QVERIFY(status->text().contains(QStringLiteral("Scan kind: getdaq.aget-threshold-scan")));
        QVERIFY(status->text().contains(QStringLiteral("Point files present: 3/3")));
        QFile thresholdManifest(QDir(thresholdDirectory).filePath(QStringLiteral("scan-manifest.json")));
        QVERIFY(thresholdManifest.open(QIODevice::ReadOnly));
        const QJsonObject thresholdPlan=QJsonDocument::fromJson(thresholdManifest.readAll()).object();
        QCOMPARE(thresholdPlan.value(QStringLiteral("kind")).toString(),QStringLiteral("getdaq.aget-threshold-scan"));
        QCOMPARE(thresholdPlan.value(QStringLiteral("target_cobo")).toString(),QStringLiteral("C0"));
        QCOMPARE(thresholdPlan.value(QStringLiteral("target_asad")).toInt(),0);
        QCOMPARE(thresholdPlan.value(QStringLiteral("target_aget")).toInt(),0);
        const QJsonArray thresholdPoints=thresholdPlan.value(QStringLiteral("points")).toArray();
        QCOMPARE(thresholdPoints.size(),3);
        QCOMPARE(thresholdPoints.at(0).toObject().value(QStringLiteral("threshold_code")).toInt(),0);
        QCOMPARE(thresholdPoints.at(1).toObject().value(QStringLiteral("threshold_code")).toInt(),4);
        QCOMPARE(thresholdPoints.at(2).toObject().value(QStringLiteral("threshold_code")).toInt(),8);
        QVERIFY(QFileInfo(QDir(thresholdDirectory).filePath(QStringLiteral("point-002.json"))).isFile());
        QVERIFY(validateWithNativeCli(thresholdDirectory));
        QPushButton *thresholdPolicyButton=dialog->findChild<QPushButton *>("CreateScanExecutionPolicy");
        QPushButton *thresholdRunButton=dialog->findChild<QPushButton *>("StartScanPoint");
        QLabel *thresholdManualNotice=dialog->findChild<QLabel *>("ThresholdManualOnlyNotice");
        QVERIFY(thresholdPolicyButton && !thresholdPolicyButton->isEnabled());
        QVERIFY(thresholdRunButton && !thresholdRunButton->isEnabled());
        QVERIFY(thresholdManualNotice && thresholdManualNotice->isVisible());
        QVERIFY(thresholdManualNotice->text().contains(QStringLiteral("manual-only")));

        kind->setCurrentIndex(3);
        QCOMPARE(kind->currentData().toString(),QStringLiteral("auto-gain"));
        QVERIFY(asad->isVisible() && aget->isVisible());
        auto *firstChannel=dialog->findChild<QSpinBox *>("AutoGainFirstChannel");
        auto *lastChannel=dialog->findChild<QSpinBox *>("AutoGainLastChannel");
        QVERIFY(firstChannel && lastChannel && firstChannel->isVisible() && lastChannel->isVisible());
        firstChannel->setValue(0);
        lastChannel->setValue(1);
        start->setValue(-1000);
        stop->setValue(-900);
        step->setValue(50);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("autogain channel plan"));
        create->click();
        const QString autogainDirectory=QDir(outputParent).filePath(QStringLiteral("autogain channel plan"));
        QTRY_VERIFY_WITH_TIMEOUT(status->text().contains(QStringLiteral("The plan contains offline settings only.")),5000);
        QVERIFY(status->text().contains(QStringLiteral("Scan kind: getdaq.auto-gain-scan")));
        QVERIFY(status->text().contains(QStringLiteral("Configuration points: 6")));
        QVERIFY(status->text().contains(QStringLiteral("Point files present: 6/6")));
        QVERIFY(validateWithNativeCli(autogainDirectory));
        QFile autogainManifest(QDir(autogainDirectory).filePath(QStringLiteral("scan-manifest.json")));
        QVERIFY(autogainManifest.open(QIODevice::ReadOnly));
        const QJsonObject autogainPlan=QJsonDocument::fromJson(autogainManifest.readAll()).object();
        QCOMPARE(autogainPlan.value(QStringLiteral("kind")).toString(),QStringLiteral("getdaq.auto-gain-scan"));
        QVERIFY(!autogainPlan.value(QStringLiteral("hardware_loaded_verified")).toBool());
        QVERIFY(autogainPlan.value(QStringLiteral("requires_full_configure_per_point")).toBool());
        QVERIFY(!autogainPlan.value(QStringLiteral("gain_result_interpretation_verified")).toBool());
        const QJsonArray autogainPoints=autogainPlan.value(QStringLiteral("points")).toArray();
        QCOMPARE(autogainPoints.size(),6);
        QCOMPARE(autogainPoints.at(0).toObject().value(QStringLiteral("channel_id")).toInt(),1);
        QCOMPARE(autogainPoints.at(0).toObject().value(QStringLiteral("voltage_mv")).toInt(),-1000);
        QCOMPARE(autogainPoints.at(1).toObject().value(QStringLiteral("voltage_mv")).toInt(),-950);
        QCOMPARE(autogainPoints.at(3).toObject().value(QStringLiteral("channel_id")).toInt(),0);
        QVERIFY(QFileInfo(QDir(autogainDirectory).filePath(QStringLiteral("point-005.json"))).isFile());

        step->setValue(60);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("invalid autogain plan"));
        create->click();
        QVERIFY(status->text().contains(QStringLiteral("must be valid and land exactly on the stop value")));
        QVERIFY(!QFileInfo::exists(QDir(outputParent).filePath(QStringLiteral("invalid autogain plan"))));

        kind->setCurrentIndex(2);
        start->setValue(0);
        stop->setValue(8);
        step->setValue(3);
        dialog->findChild<QLineEdit *>("ScanOutputFolder")->setText(QStringLiteral("invalid threshold plan"));
        create->click();
        QVERIFY(status->text().contains(QStringLiteral("must land exactly on the stop value")));
        QVERIFY(!QFileInfo::exists(QDir(outputParent).filePath(QStringLiteral("invalid threshold plan"))));
    } else {
        QVERIFY(dialog->findChild<QLabel *>("ScanPlanStatus")->text().contains(QStringLiteral("sibling getdaq-config program is missing")));
    }
    QVERIFY(create->isEnabled());
    dialog->close();
}

} // namespace GetDAQTestInternal
