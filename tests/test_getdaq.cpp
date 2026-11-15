#include <QtTest>
#include "plantransitionwindow.h"
#include "consolepresentation.h"
#include "offlinedatatoolswindow.h"
#include <QListWidget>
#include <QStandardItemModel>
#include "autorunwidget.h"
#include "diagnostics.h"
#include <QTextBlock>
#include <QTextLayout>
#include "failedhandoffwindow.h"
#include "cobomaintenancewindow.h"
#include "cobo_maintenance_fixture.h"
#include "selectionfile.h"
#include <QComboBox>
#include "plan_transition_fixture.h"
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>
#include <QCoreApplication>
#include <QApplication>
#include <QStandardPaths>
#include <QDir>
#include <QXmlStreamReader>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QSpinBox>
#include <QTableWidget>
#include <QToolButton>
#include <QScrollBar>
#include <QHeaderView>
#include <QCheckBox>
#include <QScrollArea>
#include <QAction>
#include <QMenuBar>
#include <QMenu>
#include <QFrame>
#include <QSplitter>
#include <QLayout>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QMessageBox>
#include <QInputDialog>
#include <QAbstractButton>
#include "hardwaredescriptionwindow.h"
#include <QTreeWidget>
#include <QFileDialog>
#include <QProcess>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QImage>
#include <QTimer>
#include <sstream>
#include <limits>
#include <vector>
#include "runconfig.h"
#include "protocol.h"
#include "eccclient.h"
#include "runcontrol.h"
#include "nativeruncontrol.h"
#include "nativestatusprojection.h"
#include "nativestatusobservations.h"
#include "nativereceiverfinalizationpolicy.h"
#include "nativerecoverysnapshotpolicy.h"
#include "nativerecoveryevidencepolicy.h"
#include "nativecatalogrecoverypolicy.h"
#include "nativealarmrecoverypolicy.h"
#include "nativecobomaintenancepolicy.h"
#include "nativeplanretirementprojection.h"
#include "runrecorder.h"
#include "mainwindow.h"
#include "configurationreadbackwindow.h"
#include "configuration_readback_fixture.h"
#include <QSignalBlocker>
#include <QJsonArray>
#include "ipwindow.h"
#include "widgetplot2d.h"
#include "sentryservice.h"
#include "simulator.h"
#include "xcfgdocument.h"
#include "experimentreader.h"
#include "systemexperimentconfiguration.h"
#include "cobo_configure_fixture.h"
#include "configurationsha256.h"
#include "hardwarecompiler.h"
#include "agetcompiler.h"
#include "clockcompiler.h"
#include "monitoringcompiler.h"
#include "calibrationcompiler.h"
#include "mutantcompiler.h"
#include "configwindow.h"
#include "binaryconfigurationwindow.h"
#include "configurationworkspacewindow.h"
#include "workspacerecoverywindow.h"
#include "workspacemigrationwindow.h"
#include "scanplanwindow.h"
#include "scanplanreview.h"
#include "scanexecutionjournalstore.h"
#include "scanplanoutput.h"
#include "systemscanplan.h"
#include "pulseramplitudescan.h"
#include "scanexecutionsnapshot.h"
#include "scanexecutionpolicy.h"
#include "scanruntimeidentity.h"
#include "hardwarestatewindow.h"
#include "hardwarestateprotocol.h"
#include "hardware_state_fixture.h"
#include "httpserver.h"
#include "native_scan_run_fixture.h"
#include "xcfgsavejournal.h"
#include "configurationrepositorywindow.h"
#include "../common/experiment/scanexecutionjournal.h"
#include "waveformviewer_testfixtures_p.h"
#include "waveformviewer_tests_p.h"
#include "autorun_tests_p.h"
#include "getdaq_test_support_p.h"
#include "consolelayout_tests_p.h"
#include "offline_tools_tests_p.h"
#include "native_recovery_policy_tests_p.h"
#include "configuration_compiler_tests_p.h"
#include "mocknode_fixture_p.h"
#include "legacy_runcontrol_tests_p.h"
#include "scanplan_builder_gui_tests_p.h"
#include "native_scan_run_tests_p.h"
#include "native_gui_tests_p.h"
#include "scanplan_output_tests_p.h"
#include "native_status_tests_p.h"
#include "operation_log_tests_p.h"
#include "soap_protocol_tests_p.h"
#include "sentry_service_tests_p.h"
#include "runrecorder_tests_p.h"
using GetDAQTestFixtures::MockNode;
using GetDAQTestInternal::MakePulseType1Frame;
using GetDAQTestInternal::MakeType1Frame;
using GetDAQTestInternal::MakeType2Frame;
using GetDAQTestInternal::Put16BigEndian;
using GetDAQTestInternal::Put24BigEndian;
using GetDAQTestInternal::Put32BigEndian;
using GetDAQTestInternal::WriteWaveformFixture;
#ifdef Q_OS_UNIX
#include "../common/storage/directorylease.h"
#endif

class TestGetDAQ : public QObject
{
    Q_OBJECT
private:
    RunConfig Config(const QString &dir)
    {
        return GetDAQTestSupport::Config(dir);
    }
    bool At(const QSignalSpy &spy, int state)
    {
        return GetDAQTestSupport::At(spy, state);
    }
    QString Phase(const QSignalSpy &spy) { return GetDAQTestSupport::Phase(spy); }
    void MakeFiles(const RunConfig &c)
    {
        GetDAQTestSupport::MakeFiles(c);
    }
private slots:
    void nativeReceiverFinalizationPolicyRequiresCompleteBoundEvidence()
    {
        GetDAQTestInternal::RunNativeReceiverFinalizationPolicyRequiresCompleteBoundEvidence();
    }
    void nativeRecoverySnapshotPolicyRequiresExactDeploymentIdentity()
    {
        GetDAQTestInternal::RunNativeRecoverySnapshotPolicyRequiresExactDeploymentIdentity();
    }
    void nativeRecoveryEvidencePolicyBindsInstanceAndReceiverSessions()
    {
        GetDAQTestInternal::RunNativeRecoveryEvidencePolicyBindsInstanceAndReceiverSessions();
    }
    void nativeCatalogRecoveryPolicyRequiresExactLivePreparedReplacement()
    {
        GetDAQTestInternal::RunNativeCatalogRecoveryPolicyRequiresExactLivePreparedReplacement();
    }
    void nativeAlarmRecoveryPolicyBindsOriginalOperationAndOutcome()
    {
        GetDAQTestInternal::RunNativeAlarmRecoveryPolicyBindsOriginalOperationAndOutcome();
    }
    void nativeCoBoMaintenancePolicySharesPreparedIdentityGate()
    {
        GetDAQTestInternal::RunNativeCoBoMaintenancePolicySharesPreparedIdentityGate();
    }
    void nativePlanRetirementProjectionPreservesOriginalInstanceGates()
    {
        GetDAQTestInternal::RunNativePlanRetirementProjectionPreservesOriginalInstanceGates();
    }
    void scanExecutionJournalRoundTripQuarantineAndReconciliation()
    {
        using namespace GetDAQ;
        const ScanExecutionJournal initial("scan-win-check", std::string(64, 'a'),
                                           std::string(64, 'f'), 1);
        const ScanExecutionJournal reviewed = initial.WithTransition(
            ScanPointState::Planned, ScanPointState::Reviewed);
        const ScanExecutionJournal intent = reviewed.WithOperationIntent(
            "operation-win-check", "start-ecc", std::string(64, 'b'));
        QVERIFY(intent.HasPendingOperation());
        QCOMPARE(intent.State(), ScanPointState::Reviewed);
        QCOMPARE(intent.PendingOperationTarget(), ScanPointState::EccStarted);

        // Reopening durable intent without a result must fail closed.
        const ScanExecutionJournal afterRestart = ScanExecutionJournal::Decode(intent.Encode());
        QCOMPARE(afterRestart.State(), ScanPointState::NeedsReconciliation);
        QVERIFY(afterRestart.PendingOperationUnknown());
        QCOMPARE(afterRestart.PendingOperationId(), std::string("operation-win-check"));

        const ScanExecutionJournal reconciled = afterRestart.WithOperationReconciled(
            "operation-win-check", "operator-1",
            ScanOperationDisposition::ConfirmedNotSent, std::string(64, 'c'));
        QCOMPARE(reconciled.State(), ScanPointState::Reviewed);
        QVERIFY(!reconciled.HasPendingOperation());
        QVERIFY_EXCEPTION_THROWN(
            reconciled.WithOperationIntent("operation-win-check", "start-ecc",
                                           std::string(64, 'd')),
            std::invalid_argument);

        const ScanExecutionJournal secondIntent = reconciled.WithOperationIntent(
            "operation-win-check-2", "start-ecc", std::string(64, 'f'));
        const ScanExecutionJournal secondUnknown =
            ScanExecutionJournal::Decode(secondIntent.Encode());
        const ScanExecutionJournal applied = secondUnknown.WithOperationReconciled(
            "operation-win-check-2", "operator-1",
            ScanOperationDisposition::ConfirmedApplied, std::string(64, 'a'));
        QCOMPARE(applied.State(), ScanPointState::EccStarted);
        QVERIFY(!applied.HasPendingOperation());

        const ScanExecutionJournal completed = intent.WithOperationComplete(
            "operation-win-check", std::string(64, 'e'), ScanPointState::EccStarted);
        const ScanExecutionJournal restored = ScanExecutionJournal::Decode(completed.Encode());
        QCOMPARE(restored.State(), ScanPointState::EccStarted);
        QVERIFY(!restored.HasPendingOperation());
        QVERIFY_EXCEPTION_THROWN(
            ScanExecutionJournal::Decode(completed.Encode().substr(0, completed.Encode().size() - 1)),
            std::exception);
    }

    void scanPlanOutputInspectionRejectsMalformedPlans()
    {
        GetDAQTestInternal::RunScanPlanOutputInspectionRejectsMalformedPlans();
    }

    void offlineScanPlanBuilderIsAvailableFromConsole()
    {
        GetDAQTestInternal::RunOfflineScanPlanBuilderIsAvailableFromConsole();
    }

    void waveformViewerIndexesRawAndLayeredFrames()
    {
        GetDAQTestInternal::RunWaveformViewerIndexesRawAndLayeredFrames();
    }

    void waveformViewerDecodesCompactCoboFrames()
    {
        GetDAQTestInternal::RunWaveformViewerDecodesCompactCoboFrames();
    }

    void waveformViewerRejectsTruncatedMfmInput()
    {
        GetDAQTestInternal::RunWaveformViewerRejectsTruncatedMfmInput();
    }

    void waveformViewerComputesLegacyPulseMetrics()
    {
        GetDAQTestInternal::RunWaveformViewerComputesLegacyPulseMetrics();
    }

    void waveformViewerSummarizesPeakingTimeScan()
    {
        GetDAQTestInternal::RunWaveformViewerSummarizesPeakingTimeScan();
    }

    void waveformViewerSummarizesPulserResponse()
    {
        GetDAQTestInternal::RunWaveformViewerSummarizesPulserResponse();
    }

    void waveformViewerNavigatesAndWrapsFrames()
    {
        GetDAQTestInternal::RunWaveformViewerNavigatesAndWrapsFrames();
    }

    void waveformViewerExportsSelectedSamplesAsCsv()
    {
        GetDAQTestInternal::RunWaveformViewerExportsSelectedSamplesAsCsv();
    }

    void coboMaintenanceRequiresDefaultCancelAndBindsOriginalTarget() {
        CoBoMaintenanceWindow preferred;preferred.SetControlState(true,CoBoMaintenanceFixture::Status(),PlanTransitionFixture::Identity(),"Prepared");
        preferred.PreferTarget("C1");preferred.ShowTargets(CoBoMaintenanceFixture::Targets(),{});
        QCOMPARE(preferred.findChild<QComboBox *>("Target")->currentText(),QString("C1"));
        CoBoMaintenanceWindow window;window.SetControlState(true,CoBoMaintenanceFixture::Status(),PlanTransitionFixture::Identity(),"Prepared");
        window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});window.show();QSignalSpy submitted(&window,&CoBoMaintenanceWindow::SubmitSignal);
        QVERIFY(window.findChild<QLabel *>("LifecycleState")->text().contains("Prepared"));
        auto button=window.findChild<QPushButton *>("Submit");auto configure=window.findChild<QPushButton *>("Configure");
        QVERIFY(button && button->isEnabled());QVERIFY(configure && configure->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);QVERIFY(box->defaultButton()==box->button(QMessageBox::Cancel));box->button(QMessageBox::Cancel)->click();});
        button->click();QCOMPARE(submitted.size(),0);QVERIFY(button->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});button->click();
        QCOMPARE(submitted.size(),1);QCOMPARE(submitted[0][0].toString(),QString("C0"));QCOMPARE(submitted[0][1].toString(),QString(64,'a'));QCOMPARE(submitted[0][2].toString(),QString(64,'b'));
        button->click();QCOMPARE(submitted.size(),1);QVERIFY(!button->isEnabled());
        window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(CoBoMaintenanceFixture::Complete("C1"))),{});QVERIFY(!button->isEnabled());
        window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(CoBoMaintenanceFixture::Complete())),{});
        QVERIFY(window.findChild<QLabel *>("Status")->text().contains("completed"));
        window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});
        QVERIFY(window.findChild<QLabel *>("Status")->text().contains("completed"));
    }
    void coboMaintenanceFlipRequiresCancelAndBindsExactGpioBit() {
        CoBoMaintenanceWindow window;window.SetControlState(true,CoBoMaintenanceFixture::Status(),PlanTransitionFixture::Identity(),"Prepared");
        window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});window.show();
        auto button=window.findChild<QPushButton *>("FlipLed");auto selection=window.findChild<QComboBox *>("LedFlipTarget");
        QVERIFY(button && selection);QSignalSpy submitted(&window,&CoBoMaintenanceWindow::FlipLedSignal);
        const unsigned int ids[]={10,16};
        for(unsigned int ledId:ids) {
            const int index=selection->findData(ledId);QVERIFY(index>=0);selection->setCurrentIndex(index);QVERIFY(button->isEnabled());
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);QVERIFY(box->defaultButton()==box->button(QMessageBox::Cancel));box->button(QMessageBox::Cancel)->click();});
            button->click();QCOMPARE(submitted.size(),int(ledId==10 ? 0 : 1));QVERIFY(button->isEnabled());
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Yes)->click();});
            button->click();QCOMPARE(submitted.size(),int(ledId==10 ? 1 : 2));
            QCOMPARE(submitted.last()[0].toString(),QString("C0"));QCOMPARE(submitted.last()[1].toUInt(),ledId);
            QCOMPARE(submitted.last()[2].toString(),QString(64,'a'));QCOMPARE(submitted.last()[3].toString(),QString(64,'b'));
            button->click();QCOMPARE(submitted.size(),int(ledId==10 ? 1 : 2));QVERIFY(!button->isEnabled());
            auto complete=CoBoMaintenanceFixture::Complete();complete.request.command="flip-cobo-led";complete.request.ledFlipId=ledId;
            complete.request.operationId=ledId==10 ? "gui-flip-one" : "gui-flip-all";complete.requestKey=GetDAQ::EccControlRequestKey(complete.request);
            if(ledId==10) {
                auto wrong=complete;wrong.request.ledFlipId=11;wrong.requestKey=GetDAQ::EccControlRequestKey(wrong.request);
                window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(wrong)),{});
                QVERIFY(window.findChild<QLabel *>("Status")->text().contains("exact submitted bit"));QVERIFY(!button->isEnabled());
            }
            window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(complete)),{});
            QVERIFY(window.findChild<QLabel *>("Status")->text().contains(ledId==16 ? "GPIO bit all" : "GPIO bit 10"));
            if(ledId==10)QVERIFY(button->isEnabled());
        }
    }
    void coboMaintenancePulseRequiresCancelAndBindsExactPeriod() {
        CoBoMaintenanceWindow window;window.SetControlState(true,CoBoMaintenanceFixture::Status(),PlanTransitionFixture::Identity(),"Prepared");
        window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});window.show();
        auto button=window.findChild<QPushButton *>("PulseLed");auto selection=window.findChild<QComboBox *>("LedFlipTarget");
        auto period=window.findChild<QSpinBox *>("LedPulsePeriodMs");QVERIFY(button && selection && period);
        QSignalSpy submitted(&window,&CoBoMaintenanceWindow::PulseLedSignal);
        const unsigned int ids[]={10,16};const unsigned int periods[]={725,0};
        for(int i=0;i<2;++i) {
            const auto ledId=ids[i],periodMs=periods[i];
            const int index=selection->findData(ledId);QVERIFY(index>=0);selection->setCurrentIndex(index);period->setValue(int(periodMs));QVERIFY(button->isEnabled());
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);QVERIFY(box->defaultButton()==box->button(QMessageBox::Cancel));box->button(QMessageBox::Cancel)->click();});
            button->click();QCOMPARE(submitted.size(),i);QVERIFY(button->isEnabled());
            QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());QVERIFY(box);box->button(QMessageBox::Yes)->click();});
            button->click();QCOMPARE(submitted.size(),i+1);
            QCOMPARE(submitted.last()[0].toString(),QString("C0"));QCOMPARE(submitted.last()[1].toUInt(),ledId);QCOMPARE(submitted.last()[2].toUInt(),periodMs);
            QCOMPARE(submitted.last()[3].toString(),QString(64,'a'));QCOMPARE(submitted.last()[4].toString(),QString(64,'b'));
            button->click();QCOMPARE(submitted.size(),i+1);QVERIFY(!button->isEnabled());
            auto complete=CoBoMaintenanceFixture::Complete();complete.request.command="pulse-cobo-led";complete.request.ledPulseId=ledId;complete.request.ledPulsePeriodMs=periodMs;
            complete.request.operationId=i==0 ? "gui-pulse-led" : "gui-stop-led-pulse";complete.requestKey=GetDAQ::EccControlRequestKey(complete.request);
            if(i==0) {
                auto wrong=complete;wrong.request.ledPulsePeriodMs=726;wrong.requestKey=GetDAQ::EccControlRequestKey(wrong.request);
                window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(wrong)),{});
                QVERIFY(window.findChild<QLabel *>("Status")->text().contains("exact submitted LED or period"));QVERIFY(!button->isEnabled());
            }
            window.ShowResult(QByteArray::fromStdString(GetDAQ::EncodeEccControlOperation(complete)),{});
            QVERIFY(window.findChild<QLabel *>("Status")->text().contains(periodMs==0 ? "pulser stop acknowledged" : "period 725 ms acknowledged"));
            if(i==0)QVERIFY(button->isEnabled());
        }
    }
    void coboMaintenanceWithdrawsWrongStateAndForeignMetadata() {
        CoBoMaintenanceWindow window;window.SetControlState(true,CoBoMaintenanceFixture::Status(4),PlanTransitionFixture::Identity(),"Ready");window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});
        auto button=window.findChild<QPushButton *>("Submit");auto configure=window.findChild<QPushButton *>("Configure");QVERIFY(button && configure && !button->isEnabled() && !configure->isEnabled());
        window.SetControlState(true,CoBoMaintenanceFixture::Status(),PlanTransitionFixture::Identity(),"Prepared");QVERIFY(button->isEnabled() && configure->isEnabled());
        auto body=QJsonDocument::fromJson(CoBoMaintenanceFixture::Targets()).object();body["instance_id"]=QString(64,'9');window.ShowTargets(QJsonDocument(body).toJson(QJsonDocument::Compact),{});
        QVERIFY(!button->isEnabled() && !configure->isEnabled());QCOMPARE(window.findChild<QComboBox *>("Target")->count(),0);
        window.ShowTargets(CoBoMaintenanceFixture::Targets(),{});QVERIFY(button->isEnabled() && configure->isEnabled());window.ShowTargets({},"metadata unavailable");QVERIFY(!button->isEnabled() && !configure->isEnabled());
        window.SetControlState(false,{},{},"Disconnected");QVERIFY(!button->isEnabled());
    }
    void planTransitionReviewRequiresConfirmationAndRejectsChangedFiles() {
        QTemporaryDir directory;const auto successor=directory.filePath("successor-copy.json"),targets=directory.filePath("targets.json");
        {QFile file(successor);QVERIFY(file.open(QIODevice::WriteOnly));file.write(PlanTransitionFixture::Successor());}
        {QFile file(targets);QVERIFY(file.open(QIODevice::WriteOnly));file.write(PlanTransitionFixture::Targets());}
        PlanTransitionWindow window;window.SetControlState(true,false,PlanTransitionFixture::Status(),PlanTransitionFixture::Identity(),"Normal sealed original cycle");window.show();
        QSignalSpy submitted(&window,&PlanTransitionWindow::SubmitSignal);QString error;QVERIFY2(window.LoadReview(successor,targets,&error),qPrintable(error));
        auto button=window.findChild<QPushButton *>("Submit");QVERIFY(button && button->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});button->click();QCOMPARE(submitted.size(),0);
        {QFile file(targets);QVERIFY(file.open(QIODevice::WriteOnly|QIODevice::Append));file.write(" ");}
        button->click();QCOMPARE(submitted.size(),0);QVERIFY(!button->isEnabled());
        QVERIFY(window.LoadReview(successor,targets,&error));
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});button->click();QCOMPARE(submitted.size(),1);QVERIFY(!button->isEnabled());
        QCOMPARE(submitted.last()[0].toByteArray(),PlanTransitionFixture::Successor());QCOMPARE(submitted.last()[1].toByteArray(),PlanTransitionFixture::Targets()+" ");button->click();QCOMPARE(submitted.size(),1);
    }
    void partialRetirementConfirmationBindsSavedOriginalAndKeepsOldControlFenced() {
        PlanTransitionWindow window;window.SetControlState(false,false,PlanTransitionFixture::Status(true),PlanTransitionFixture::Identity(),"Partial failure");
        const auto original=PlanTransitionFixture::Request();const auto bytes=QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(original));
        const auto before=PlanTransitionFixture::Status();QSignalSpy submissions(&window,&PlanTransitionWindow::CompleteSignal),observed(&window,&PlanTransitionWindow::InspectCompletionSignal);
        auto button=window.findChild<QPushButton *>("CompletePartial"),inspect=window.findChild<QPushButton *>("InspectCompletion");QVERIFY(button);QVERIFY(inspect);
        window.SetCompletionState(true,false,bytes,before,{}, {},"Original failed operation verified");QVERIFY(button->isEnabled());QVERIFY(!inspect->isEnabled());
        bool defaultCancel=false;QTimer::singleShot(0,[&] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){defaultCancel=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});
        button->click();QVERIFY(defaultCancel);QCOMPARE(submissions.size(),0);
        QTimer::singleShot(0,[] {auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});
        button->click();QCOMPARE(submissions.size(),1);QCOMPARE(submissions[0][0].toString(),QString::fromStdString(original.operationId));QVERIFY(!button->isEnabled());button->click();QCOMPARE(submissions.size(),1);
        const auto completion=QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(PlanTransitionFixture::Completion(original)));
        window.SetCompletionState(false,true,bytes,before,completion,"accepted","Observe the saved ID");QVERIFY(!button->isEnabled());QVERIFY(inspect->isEnabled());inspect->click();QCOMPARE(observed.size(),1);
        window.SetCompletionState(true,true,bytes,before,completion,"failed","Cause repaired; explicit retry required");QVERIFY(button->isEnabled());
        window.ShowResult(PlanTransitionFixture::Receipt(original),bytes,{});QVERIFY(window.findChild<QPushButton *>("ExportReceipt")->isEnabled());
        auto fault=QJsonDocument::fromJson(PlanTransitionFixture::Status(true)).object();auto controller=fault["controller"].toObject();controller["outcome_unknown"]=true;fault["controller"]=controller;
        window.SetControlState(false,false,QJsonDocument(fault).toJson(QJsonDocument::Compact),PlanTransitionFixture::Identity(),"Current hardware unknown");
        window.ShowResult(PlanTransitionFixture::Receipt(original),bytes,{});QVERIFY(!window.findChild<QPushButton *>("ExportReceipt")->isEnabled());
        window.SetCompletionState(true,false,bytes,{}, {},{},"Missing context");QVERIFY(!button->isEnabled());
        window.SetCompletionState(true,false,bytes,PlanTransitionFixture::Status(true),{},{},"Late status cannot replace pre-state");QVERIFY(!button->isEnabled());
        auto identity=QJsonDocument::fromJson(PlanTransitionFixture::Identity()).object();identity["instance_id"]=QString(64,'9');
        window.SetControlState(false,false,{},QJsonDocument(identity).toJson(QJsonDocument::Compact),"Different instance");QVERIFY(!window.findChild<QPushButton *>("ExportReceipt")->isEnabled());
    }
    void partialRetirementRejectsStateChangeWhileConfirmationIsOpen() {
        PlanTransitionWindow window;window.SetControlState(false,false,PlanTransitionFixture::Status(true),PlanTransitionFixture::Identity(),"Original held");
        const auto original=QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(PlanTransitionFixture::Request()));
        window.SetCompletionState(true,false,original,PlanTransitionFixture::Status(),{},{},"Eligible");QSignalSpy submitted(&window,&PlanTransitionWindow::CompleteSignal);
        QTimer::singleShot(0,[&] {window.SetCompletionState(false,false,{},{},{},{},"Disconnected during confirmation");auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});
        window.findChild<QPushButton *>("CompletePartial")->click();QCOMPARE(submitted.size(),0);
    }
    void planTransitionExportsOnlyMatchedOriginalReceipt() {
        QTemporaryDir directory;PlanTransitionWindow window;window.SetControlState(false,true,PlanTransitionFixture::Status(true),PlanTransitionFixture::Identity(),"Original plans retired");
        const auto request=PlanTransitionFixture::Request();const auto requestBytes=QByteArray::fromStdString(GetDAQ::EncodeEccControlRequest(request));const auto receipt=PlanTransitionFixture::Receipt(request);
        window.ShowResult(receipt,requestBytes,{});auto exportButton=window.findChild<QPushButton *>("ExportReceipt");QVERIFY(exportButton->isEnabled());
        const auto destination=directory.filePath("receipt.json");QString error;QVERIFY(window.SaveReceipt(destination,&error));QFile file(destination);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),receipt);file.close();
        auto changed=QJsonDocument::fromJson(receipt).object();changed["successor_epoch"]=QString(64,'9');window.ShowResult(QJsonDocument(changed).toJson(QJsonDocument::Compact),requestBytes,{});
        QVERIFY(!exportButton->isEnabled());QVERIFY(!window.SaveReceipt(directory.filePath("bad.json"),&error));
        window.ShowResult(receipt,requestBytes,{});QVERIFY(exportButton->isEnabled());
        auto other=QJsonDocument::fromJson(PlanTransitionFixture::Identity()).object();other["instance_id"]=QString(64,'4');window.SetControlState(false,false,{},QJsonDocument(other).toJson(QJsonDocument::Compact),"Different instance");QVERIFY(!exportButton->isEnabled());
    }
    void binaryConfigurationWindowReviewsNewFilesAndInvalidatesChangedInput() {
        QTemporaryDir directory;const auto source=directory.filePath("source.xcfg"),binary=directory.filePath("copy.bcfg"),copy=directory.filePath("imported.xcfg");QFile file(source);QVERIFY(file.open(QIODevice::WriteOnly));const QByteArray original="<Setup><n type=\"int\">2</n></Setup>";QCOMPARE(file.write(original),qint64(original.size()));file.close();QString error;ConfigWindow config(directory.path(),false);QVERIFY(config.Open(source,&error));
        config.findChild<QPushButton *>("ConfigBinaryConversion")->click();auto window=config.findChild<BinaryConfigurationWindow *>();QVERIFY(window);window->findChild<QComboBox *>("direction")->setCurrentIndex(1);window->findChild<QLineEdit *>("destination")->setText(binary);window->findChild<QPushButton *>("preview")->click();auto save=window->findChild<QPushButton *>("save");QTRY_VERIFY(save->isEnabled());QVERIFY(window->findChild<QPlainTextEdit *>("review")->toPlainText().contains("<n"));QVERIFY(!QFileInfo::exists(binary));
        window->findChild<QLineEdit *>("destination")->setText(directory.filePath("other.bcfg"));QVERIFY(!save->isEnabled());window->findChild<QLineEdit *>("destination")->setText(binary);window->findChild<QPushButton *>("preview")->click();QTRY_VERIFY(save->isEnabled());save->click();QTRY_VERIFY(window->findChild<QPushButton *>("preview")->isEnabled());QVERIFY(QFileInfo::exists(binary));QVERIFY(!save->isEnabled());
        window->findChild<QComboBox *>("direction")->setCurrentIndex(0);window->findChild<QLineEdit *>("source")->setText(binary);window->findChild<QLineEdit *>("destination")->setText(copy);window->findChild<QPushButton *>("preview")->click();QTRY_VERIFY(save->isEnabled());save->click();QTRY_VERIFY(window->findChild<QPushButton *>("preview")->isEnabled());QVERIFY(QFileInfo::exists(copy));QFile retained(source);QVERIFY(retained.open(QIODevice::ReadOnly));QCOMPARE(retained.readAll(),original);XcfgDocument document;QVERIFY(document.Load(copy,&error));QCOMPARE(document.Nodes()[1].value,QString("2"));
    }

    void hardwareStateProtocolRejectsRecoveryClaimsAndInvalidSample() {
        const auto valid=HardwareStateFixture::Observation();const auto decoded=GetDAQ::DecodeHardwareStateObservation(valid.toStdString());QVERIFY(decoded.controllerOutcomeUnknown && decoded.consistent && decoded.nodes.front().available);QVERIFY(!decoded.coboLifecycleIsHardwareReadback);QCOMPARE(decoded.coboLifecycle.size(),std::size_t(1));QCOMPARE(QString::fromStdString(decoded.coboLifecycle.front().state),QString("running"));auto o=QJsonDocument::fromJson(valid).object();o["cobo_lifecycle_source"]="controller-baseline";auto baseline=GetDAQ::DecodeHardwareStateObservation(QJsonDocument(o).toJson().toStdString());QCOMPARE(QString::fromStdString(baseline.coboLifecycleSource),QString("controller-baseline"));o["cobo_lifecycle_source"]="unavailable";o["consistent"]=false;o["cobo_lifecycle"]=QJsonArray();o["nodes"]=QJsonArray();auto unavailable=GetDAQ::DecodeHardwareStateObservation(QJsonDocument(o).toJson().toStdString());QCOMPARE(QString::fromStdString(unavailable.coboLifecycleSource),QString("unavailable"));o["consistent"]=true;bool rejected=false;try{GetDAQ::DecodeHardwareStateObservation(QJsonDocument(o).toJson().toStdString());}catch(...){rejected=true;}QVERIFY(rejected);o=QJsonDocument::fromJson(valid).object();o["recovery_verified"]=true;rejected=false;try{GetDAQ::DecodeHardwareStateObservation(QJsonDocument(o).toJson().toStdString());}catch(...){rejected=true;}QVERIFY(rejected);
        o=QJsonDocument::fromJson(valid).object();o["consistent"]=false;rejected=false;try{GetDAQ::DecodeHardwareStateObservation(QJsonDocument(o).toJson().toStdString());}catch(...){rejected=true;}QVERIFY(rejected);const auto changed=GetDAQ::DecodeHardwareStateObservation(HardwareStateFixture::Observation(true,false).toStdString());QVERIFY(!changed.nodes.front().available && changed.nodes.front().values.empty());
    }
    void hardwareStateWindowUsesOnlyAuthenticatedGetAndWithdrawsValues() {
        QTemporaryDir directory;const auto tokenPath=directory.filePath("credential");QFile file(tokenPath);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(64,'q'));file.close();HttpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));QStringList requests;bool auth=true,bad=false;
        connect(&server,&HttpServer::RequestSignal,this,[&](const QString &method,const QString &path,const QByteArray &,HttpConnection *connection){requests.append(method+" "+path);auth=auth && connection->RequestHeader("authorization")=="Bearer "+QByteArray(64,'q');if(path=="/v1/identity")connection->Reply(200,HardwareStateFixture::Identity());else connection->Reply(bad ? 503 : 200,bad ? QByteArray("{\"error\":\"fixture unavailable\"}") : HardwareStateFixture::Observation(true,true,"controller-baseline"));});
        EccSystemClientOptions options;options.address="127.0.0.1";options.port=server.serverPort();options.credentialFile=tokenPath;options.serviceId="ecc-observer";options.snapshotSha256=QString(64,'b');HardwareStateWindow window(options);auto inspect=window.findChild<QPushButton *>("inspect");auto table=window.findChild<QTableWidget *>("nodes");QVERIFY(inspect && table);inspect->click();QTRY_COMPARE(table->rowCount(),1);QVERIFY(window.findChild<QLabel *>("status")->text().contains("UNKNOWN"));QVERIFY(window.findChild<QLabel *>("status")->text().contains("controller baseline"));QVERIFY(auth);QCOMPARE(requests,QStringList({"GET /v1/identity","GET /v1/hardware-state"}));QTest::qWait(300);QCOMPARE(requests.size(),2);
        bad=true;inspect->click();QTRY_VERIFY(inspect->isEnabled());QCOMPARE(table->rowCount(),0);bad=false;inspect->click();QTRY_COMPARE(table->rowCount(),1);window.findChild<QLineEdit *>("snapshot")->setText(QString(64,'c'));QCOMPARE(table->rowCount(),0);inspect->click();QTRY_VERIFY(inspect->isEnabled());QCOMPARE(table->rowCount(),0);QVERIFY(window.findChild<QLabel *>("status")->text().contains("differs"));for(const auto &request:requests)QVERIFY(request.startsWith("GET "));
    }
    void hardwareStateWindowExpiresValuesWithoutNetworkPolling() {
        QTemporaryDir directory;
        const auto credential=directory.filePath("credential");
        QFile file(credential);QVERIFY(file.open(QIODevice::WriteOnly));file.write(QByteArray(64,'q'));file.close();
        HttpServer server;QVERIFY(server.listen(QHostAddress::LocalHost));int requests=0;
        connect(&server,&HttpServer::RequestSignal,this,[&](const QString &method,const QString &path,const QByteArray &,HttpConnection *connection){
            ++requests;
            QCOMPARE(method,QString("GET"));
            connection->Reply(200,path=="/v1/identity" ? HardwareStateFixture::Identity() : HardwareStateFixture::Observation());
        });
        EccSystemClientOptions options;options.address="127.0.0.1";options.port=server.serverPort();options.credentialFile=credential;options.serviceId="ecc-observer";options.snapshotSha256=QString(64,'b');
        HardwareStateWindow window(options);auto table=window.findChild<QTableWidget *>("nodes");auto inspect=window.findChild<QPushButton *>("inspect");QVERIFY(table && inspect);
        inspect->click();QTRY_COMPARE(table->rowCount(),1);
        QCOMPARE(table->columnCount(),11);
        QCOMPARE(table->item(0,3)->text(),QString("running"));
        const auto observation=GetDAQ::DecodeHardwareStateObservation(HardwareStateFixture::Observation().toStdString());
        QCOMPARE(table->item(0,9)->text(),QString::number(qulonglong(observation.nodes.front().observedAtUnixMs)));
        QCOMPARE(requests,2);
        QTest::qWait(7100);
        QCOMPARE(table->rowCount(),0);
        QVERIFY(window.findChild<QLabel *>("status")->text().contains("expired"));
        QCOMPARE(requests,2);
        QVERIFY(inspect->isEnabled());
    }
    void workspaceRecoveryWindowCancelConfirmationAndReadonly() {
        QTemporaryDir directory;QString error;const auto path=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",candidate="<Setup><n type=\"int\">3</n></Setup>";QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();QMap<QString,QByteArray> before,after;before[path]=original;after[path]=candidate;QString record;{XcfgSaveJournal journal;QVERIFY(journal.Begin({path},before,after,&error));record=journal.Directory();}
        WorkspaceRecoveryWindow window(record,false);auto preview=window.findChild<QPushButton *>("preview"),apply=window.findChild<QPushButton *>("apply");QVERIFY(preview && apply);preview->click();QVERIFY2(apply->isEnabled(),qPrintable(window.findChild<QLabel *>("status")->text()));
        QCOMPARE(window.findChild<QPlainTextEdit *>("current")->toPlainText(),QString::fromUtf8(original));QCOMPARE(window.findChild<QPlainTextEdit *>("target")->toPlainText(),QString::fromUtf8(candidate));
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});apply->click();QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();QVERIFY(!QFileInfo::exists(record+"/complete.json"));
        WorkspaceRecoveryWindow readonly(record,true);readonly.findChild<QPushButton *>("preview")->click();QVERIFY(!readonly.findChild<QPushButton *>("apply")->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});apply->click();QVERIFY2(QFileInfo::exists(record+"/complete.json"),qPrintable(window.findChild<QLabel *>("status")->text()));QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),candidate);QVERIFY(!apply->isEnabled());
    }
    void workspacePathMigrationWindowCancelReadonlyAndConfirmedSave() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("moved.xcfg");
        QFile file(root);QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup><n type=\"int\">2</n></Setup>");file.close();
        XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));WorkspaceMigrationWindow window(&workspace,false);
        auto destination=window.findChild<QLineEdit *>("destination");auto preview=window.findChild<QPushButton *>("preview"),apply=window.findChild<QPushButton *>("apply");QVERIFY(destination && preview && apply);
        destination->setText(target);preview->click();QVERIFY2(apply->isEnabled(),qPrintable(window.findChild<QLabel *>("status")->text()));
        bool defaultCancel=false;QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){defaultCancel=box->defaultButton()==box->button(QMessageBox::Cancel);box->button(QMessageBox::Cancel)->click();}});
        apply->click();QVERIFY(defaultCancel);QVERIFY(QFileInfo::exists(root));QVERIFY(!QFileInfo::exists(target));QVERIFY(!workspace.RequiresReopen());
        destination->setText(directory.filePath("other.xcfg"));QVERIFY(!apply->isEnabled());destination->setText(target);preview->click();
        QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());destination->setText(directory.filePath("other.xcfg"));preview->click();if(box)box->button(QMessageBox::Yes)->click();});
        apply->click();QVERIFY(QFileInfo::exists(root));QVERIFY(!QFileInfo::exists(target));QVERIFY(!QFileInfo::exists(directory.filePath("other.xcfg")));QVERIFY(!workspace.RequiresReopen());destination->setText(target);preview->click();
        WorkspaceMigrationWindow readonly(&workspace,true);readonly.findChild<QLineEdit *>("destination")->setText(target);readonly.findChild<QPushButton *>("preview")->click();QVERIFY(!readonly.findChild<QPushButton *>("apply")->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});apply->click();
        QVERIFY(window.Result().complete);QVERIFY(!QFileInfo::exists(root));QVERIFY(QFileInfo::exists(target));QVERIFY(workspace.RequiresReopen());
    }
    void configurationWorkspacePathMigrationReopensNewSourcesAndClearsOldHistory() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("moved.xcfg");
        QFile file(root);QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup><n type=\"int\">2</n></Setup>");file.close();
        ConfigurationWorkspaceWindow window(directory.path(),false);QVERIFY(window.Open(root,&error));auto migrate=window.findChild<QPushButton *>("migration");QVERIFY(migrate && migrate->isEnabled());
        auto editor=window.findChild<QPlainTextEdit *>("WorkspaceSourceEditor");QVERIFY(editor);const auto text=editor->toPlainText();editor->setPlainText(text+"\n");QVERIFY(!migrate->isEnabled());editor->setPlainText(text);QVERIFY(migrate->isEnabled());
        bool confirmed=false;
        QTimer::singleShot(0,[&]{auto dialog=qobject_cast<WorkspaceMigrationWindow *>(QApplication::activeModalWidget());if(!dialog)return;
            dialog->findChild<QLineEdit *>("destination")->setText(target);dialog->findChild<QPushButton *>("preview")->click();
            QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box){confirmed=true;box->button(QMessageBox::Yes)->click();}});
            dialog->findChild<QPushButton *>("apply")->click();if(!dialog->Result().complete)dialog->reject();
        });migrate->click();QVERIFY(confirmed);QVERIFY(QFileInfo::exists(target));QVERIFY(!QFileInfo::exists(root));
        QVERIFY(window.findChild<QLabel *>("roots")->text().contains(target));QVERIFY(!window.findChild<QPushButton *>("undo")->isEnabled());
        editor=window.findChild<QPlainTextEdit *>("WorkspaceSourceEditor");QVERIFY(editor);QCOMPARE(editor->property("sourceFile").toString(),target);QVERIFY(!editor->isReadOnly());
        ConfigurationWorkspaceWindow readonly(directory.path(),true);QVERIFY(readonly.Open(target,&error));QVERIFY(!readonly.findChild<QPushButton *>("migration")->isEnabled());
    }
    void pathMigrationRecoveryWindowShowsMissingPathsAndRequiresConfirmation() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg"),target=directory.filePath("moved.xcfg");
        QFile file(root);QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup/>");file.close();XcfgWorkspace workspace;QVERIFY(workspace.Open(root,&error));
        XcfgSourceMigration::Request request;request.destinations[root]=target;XcfgSourceMigration::Preview planned;QVERIFY(XcfgSourceMigration::CreatePreview(workspace,request,&planned,&error));XcfgSourceMigration::Result result;
        QVERIFY(!XcfgSourceMigration::Apply(&workspace,planned,&result,&error,[](const QString &phase,const QString &){if(phase=="removed")throw std::runtime_error("owned interruption");}));
        WorkspaceRecoveryWindow window(result.recoveryDirectory,false);auto preview=window.findChild<QPushButton *>("preview"),apply=window.findChild<QPushButton *>("apply");preview->click();QVERIFY2(apply->isEnabled(),qPrintable(window.findChild<QLabel *>("status")->text()));
        auto sources=window.findChild<QComboBox *>("source");sources->setCurrentText(root);QVERIFY(window.findChild<QPlainTextEdit *>("current")->toPlainText().contains("absent"));
        WorkspaceRecoveryWindow readonly(result.recoveryDirectory,true);readonly.findChild<QPushButton *>("preview")->click();QVERIFY(!readonly.findChild<QPushButton *>("apply")->isEnabled());
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Cancel)->click();});apply->click();QVERIFY(!QFileInfo::exists(result.recoveryDirectory+"/complete.json"));
        QTimer::singleShot(0,[&]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());window.findChild<QComboBox *>("direction")->setCurrentIndex(1);preview->click();if(box)box->button(QMessageBox::Yes)->click();});
        apply->click();QVERIFY(!QFileInfo::exists(root));QVERIFY(QFileInfo::exists(target));QVERIFY(!QFileInfo::exists(result.recoveryDirectory+"/complete.json"));window.findChild<QComboBox *>("direction")->setCurrentIndex(0);preview->click();
        QTimer::singleShot(0,[]{auto box=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());if(box)box->button(QMessageBox::Yes)->click();});apply->click();QVERIFY(QFileInfo::exists(result.recoveryDirectory+"/complete.json"));
        XcfgDocument reopened;QVERIFY(reopened.Load(target,&error));QVERIFY(!QFileInfo::exists(root));
    }
    void configurationWorkspaceCrossFileMoveReviewsUndoRedoAndSaveAll() {
        QTemporaryDir directory;QString error;const auto a=directory.filePath("a.xcfg"),b=directory.filePath("b.xcfg");const QByteArray source="<Left><Item id=\"0\"><n type=\"int\">3</n></Item></Left>",target="<Right/>";
        QFile file(a);QVERIFY(file.open(QIODevice::WriteOnly));file.write(source);file.close();file.setFileName(b);QVERIFY(file.open(QIODevice::WriteOnly));file.write(target);file.close();ConfigurationWorkspaceWindow window(directory.path(),false);QVERIFY(window.Open(a,&error));QVERIFY(window.Open(b,&error));
        auto structure=window.findChild<QPushButton *>("structure"),review=window.findChild<QPushButton *>("reviewAll"),save=window.findChild<QPushButton *>("saveAll"),undo=window.findChild<QPushButton *>("undo"),redo=window.findChild<QPushButton *>("redo");QVERIFY(structure && structure->isEnabled());bool staged=false;QString failure;
        QTimer::singleShot(0,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog){failure="Missing move dialog";return;}
            auto declaration=dialog->findChild<QComboBox *>("declaration"),operation=dialog->findChild<QComboBox *>("operation"),root=dialog->findChild<QComboBox *>("targetRoot"),parent=dialog->findChild<QComboBox *>("targetParent");auto preview=dialog->findChild<QPushButton *>("preview"),apply=dialog->findChild<QPushButton *>("stage");
            if(!declaration || !operation || !root || !parent || !preview || !apply){failure="Missing move controls";dialog->reject();return;}declaration->setCurrentIndex(1);operation->setCurrentIndex(14);root->setCurrentIndex(1);parent->setCurrentIndex(0);preview->click();
            if(!apply->isEnabled()){failure=dialog->findChild<QLabel *>("status")->text();dialog->reject();return;}const auto record=QJsonDocument::fromJson(dialog->findChild<QPlainTextEdit *>("review")->toPlainText().toUtf8()).object();if(record.value("all_root_reviews").toObject().size()!=2){failure="Missing all-root move review";dialog->reject();return;}staged=true;apply->click();});
        structure->click();QVERIFY2(staged,qPrintable(failure));QPlainTextEdit *first=nullptr,*second=nullptr;for(auto editor:window.findChildren<QPlainTextEdit *>("WorkspaceSourceEditor")){if(editor->property("sourceFile").toString()==a)first=editor;if(editor->property("sourceFile").toString()==b)second=editor;}QVERIFY(first && second);QVERIFY(!first->toPlainText().contains("Item"));QVERIFY(second->toPlainText().contains("Item"));QVERIFY(!save->isEnabled());
        undo->click();QCOMPARE(first->toPlainText(),QString::fromUtf8(source));QCOMPARE(second->toPlainText(),QString::fromUtf8(target));redo->click();QVERIFY(second->toPlainText().contains("Item"));review->click();QVERIFY(save->isEnabled());save->click();file.setFileName(a);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(!file.readAll().contains("Item"));file.close();file.setFileName(b);QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("Item"));
    }
    void configurationWorkspaceStructureJoinsTextHistoryAndSaveAll() {
        QTemporaryDir directory;QString error;const auto root=directory.filePath("root.xcfg");const QByteArray original="<Setup><n type=\"int\">2</n></Setup>";
        QFile file(root);QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();ConfigurationWorkspaceWindow window(directory.path(),false);QVERIFY(window.Open(root,&error));
        auto source=window.findChild<QPlainTextEdit *>("WorkspaceSourceEditor");auto structure=window.findChild<QPushButton *>("structure"),review=window.findChild<QPushButton *>("reviewAll"),save=window.findChild<QPushButton *>("saveAll"),undo=window.findChild<QPushButton *>("undo"),redo=window.findChild<QPushButton *>("redo");QVERIFY(source && structure && structure->isEnabled());
        auto text=source->toPlainText();text.replace(">2<",">3<");source->setPlainText(text);QVERIFY(!structure->isEnabled());review->click();QVERIFY(structure->isEnabled());
        bool staged=false;QString failure;
        QTimer::singleShot(0,[&]{auto dialog=qobject_cast<QDialog *>(QApplication::activeModalWidget());if(!dialog){failure="No structure dialog";return;}
            auto declaration=dialog->findChild<QComboBox *>("declaration"),operation=dialog->findChild<QComboBox *>("operation");auto name=dialog->findChild<QLineEdit *>("name");auto preview=dialog->findChild<QPushButton *>("preview"),apply=dialog->findChild<QPushButton *>("stage");
            if(!declaration || !operation || !name || !preview || !apply){failure="Missing production structure fields";dialog->reject();return;}
            declaration->setCurrentIndex(1);operation->setCurrentIndex(XcfgStructureEditor::ChangeName);name->setText("threshold");preview->click();
            if(!apply->isEnabled()){failure=dialog->findChild<QLabel *>("status")->text();dialog->reject();return;}staged=true;apply->click();});
        structure->click();QVERIFY2(staged,qPrintable(failure));QVERIFY(source->toPlainText().contains("threshold"));QVERIFY(!save->isEnabled());QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        undo->click();QVERIFY(source->toPlainText().contains("<n"));QVERIFY(source->toPlainText().contains(">3<"));undo->click();QCOMPARE(source->toPlainText(),QString::fromUtf8(original));
        redo->click();redo->click();QVERIFY(source->toPlainText().contains("threshold"));review->click();QVERIFY(save->isEnabled());save->click();QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("threshold"));file.close();
    }
    void configurationWorkspaceStagesReviewsAndSavesSharedSource() {
        QTemporaryDir directory;QString error;
        const auto first=directory.filePath("first.xcfg"),second=directory.filePath("second.xcfg"),shared=directory.filePath("shared.xcfg");
        auto write=[](const QString &path,const QByteArray &bytes) {QFile file(path);return file.open(QIODevice::WriteOnly)&&file.write(bytes)==bytes.size();};
        QVERIFY(write(first,"<Setup><A id=\"0\" include=\"shared.xcfg\"/></Setup>"));
        QVERIFY(write(second,"<Setup><A id=\"1\" include=\"shared.xcfg\"/></Setup>"));
        const QByteArray original="<Template><!-- keep --><n type=\"int\">2</n></Template>\r\n";QVERIFY(write(shared,original));
        ConfigurationWorkspaceWindow window(directory.path(),false);QVERIFY(window.Open(first,&error));QVERIFY(window.Open(second,&error));
        auto review=window.findChild<QPushButton *>("reviewAll"),save=window.findChild<QPushButton *>("saveAll");QVERIFY(review&&save);
        review->click();QVERIFY(!save->isEnabled()); // Opening CRLF sources must not dirty them.
        QPlainTextEdit *source=nullptr;for(auto editor:window.findChildren<QPlainTextEdit *>("WorkspaceSourceEditor"))
            if(editor->property("sourceFile").toString()==QFileInfo(shared).canonicalFilePath()) source=editor;
        QVERIFY(source);auto text=source->toPlainText();text.replace(">2<",">7<");source->setPlainText(text);
        QVERIFY(!save->isEnabled());QFile disk(shared);QVERIFY(disk.open(QIODevice::ReadOnly));QCOMPARE(disk.readAll(),original);disk.close();
        review->click();QVERIFY2(save->isEnabled(),qPrintable(window.findChild<QLabel *>("status")->text()));
        auto report=window.findChild<QPlainTextEdit *>("review");QVERIFY(report->toPlainText().contains(first));QVERIFY(report->toPlainText().contains(second));
        source->appendPlainText("<!-- new -->");QVERIFY(!save->isEnabled());review->click();QVERIFY(save->isEnabled());
        save->click();QVERIFY(!save->isEnabled());QVERIFY(disk.open(QIODevice::ReadOnly));QVERIFY(disk.readAll().contains(">7<"));disk.close();
        source->setPlainText("<broken>");review->click();QVERIFY(!save->isEnabled());QVERIFY(disk.open(QIODevice::ReadOnly));QVERIFY(disk.readAll().contains(">7<"));
    }
    void configurationWorkspaceUndoRedoGroupsSourcesAndRequiresFreshReview() {
        QTemporaryDir directory;const auto root=directory.filePath("root.xcfg"),meta=directory.filePath("meta.xcfg");
        const QByteArray original="<Setup><A id=\"*\" include=\"meta.xcfg\"/><A id=\"0\"><n type=\"int\">2</n></A></Setup>",range="<Template><n type=\"int\" range=\"[0,9]\">2</n></Template>";
        QFile file(root);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(original),qint64(original.size()));file.close();
        file.setFileName(meta);QVERIFY(file.open(QIODevice::WriteOnly));QCOMPARE(file.write(range),qint64(range.size()));file.close();
        ConfigurationWorkspaceWindow window(directory.path(),false);QString error;QVERIFY(window.Open(root,&error));
        auto undo=window.findChild<QPushButton *>("undo"),redo=window.findChild<QPushButton *>("redo"),review=window.findChild<QPushButton *>("reviewAll"),save=window.findChild<QPushButton *>("saveAll");
        QVERIFY(undo && redo && review && save);QVERIFY(!undo->isEnabled());
        QPlainTextEdit *rootEditor=nullptr,*metaEditor=nullptr;
        for(auto editor:window.findChildren<QPlainTextEdit *>("WorkspaceSourceEditor")) {
            if(editor->property("sourceFile").toString()==QFileInfo(root).canonicalFilePath())rootEditor=editor;
            if(editor->property("sourceFile").toString()==QFileInfo(meta).canonicalFilePath())metaEditor=editor;
        }
        QVERIFY(rootEditor && metaEditor);auto changed=original,wider=range;changed.replace(">2<",">12<");wider.replace("[0,9]","[0,20]");
        rootEditor->setPlainText(QString::fromUtf8(changed));metaEditor->setPlainText(QString::fromUtf8(wider));QVERIFY(!undo->isEnabled());
        QTest::mouseClick(review,Qt::LeftButton);QVERIFY(save->isEnabled());QVERIFY(undo->isEnabled());
        QTest::mouseClick(undo,Qt::LeftButton);QCOMPARE(rootEditor->toPlainText(),QString::fromUtf8(original));QCOMPARE(metaEditor->toPlainText(),QString::fromUtf8(range));
        QVERIFY(!save->isEnabled());QVERIFY(redo->isEnabled());QTest::mouseClick(redo,Qt::LeftButton);QCOMPARE(rootEditor->toPlainText(),QString::fromUtf8(changed));QVERIFY(!save->isEnabled());
        QTest::mouseClick(review,Qt::LeftButton);QTest::mouseClick(save,Qt::LeftButton);QVERIFY(undo->isEnabled());
        QTest::mouseClick(undo,Qt::LeftButton);QVERIFY(!save->isEnabled());file.setFileName(root);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),changed);file.close();
        rootEditor->moveCursor(QTextCursor::End);rootEditor->insertPlainText("<invalid");QVERIFY(!undo->isEnabled());QVERIFY(!redo->isEnabled());
        rootEditor->undo();QVERIFY(redo->isEnabled());
    }
    void configurationWorkspaceConnectedModeIsReadonly() {
        QTemporaryDir directory;const auto path=directory.filePath("one.xcfg");QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup><n>2</n></Setup>");file.close();
        ConfigurationWorkspaceWindow window(directory.path(),true);QString error;QVERIFY(window.Open(path,&error));
        QVERIFY(!window.findChild<QPushButton *>("undo")->isEnabled());QVERIFY(!window.findChild<QPushButton *>("redo")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("structure")->isEnabled());
        QVERIFY(!window.findChild<QPushButton *>("reviewAll")->isEnabled());QVERIFY(!window.findChild<QPushButton *>("saveAll")->isEnabled());
        for(auto editor:window.findChildren<QPlainTextEdit *>("WorkspaceSourceEditor")) QVERIFY(editor->isReadOnly());
    }
    void hardwareDescriptionViewerIsExactReadonlyAndRejectsStaleDisplay()
    {
        QTemporaryDir directory;QFile file(directory.filePath("hardware.json"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes="{\"schema_version\":1,\"kind\":\"getdaq.hardware-description\",\"node_id\":\"test\",\"sources\":[],\"warnings\":[],\"devices\":[{\"name\":\"dev\",\"access\":\"memory\",\"base_address\":\"18446744073709551615\",\"register_bytes\":8,\"registers\":[{\"name\":\"reg\",\"offset\":\"0\",\"read_only\":true,\"fields\":[{\"name\":\"all\",\"offset_bits\":0,\"width_bits\":64,\"read_only\":false}]}]}]}";
        file.write(bytes);file.close();HardwareDescriptionWindow view;QString error;QVERIFY2(view.Open(file.fileName(),&error),qPrintable(error));
        auto tree=view.findChild<QTreeWidget *>("HardwareDescriptionTree");QVERIFY(tree);QCOMPARE(tree->editTriggers(),QAbstractItemView::NoEditTriggers);
        auto device=tree->topLevelItem(0);QCOMPARE(device->text(1),QString("0xffffffffffffffff"));
        device->setExpanded(true);QCOMPARE(device->childCount(),1);auto reg=device->child(0);QCOMPARE(reg->text(1),QString("0xffffffffffffffff"));
        reg->setExpanded(true);QCOMPARE(reg->child(0)->text(2),QString("64"));QCOMPARE(reg->child(0)->text(3),QString("Host read-only"));
        reg->setExpanded(false);reg->setExpanded(true);QCOMPARE(reg->childCount(),1);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),bytes);file.close();
        QVERIFY(!view.Open(directory.filePath("missing.json"),&error));QCOMPARE(tree->topLevelItemCount(),0);
        ConfigWindow readonly(directory.path(),true);QVERIFY(readonly.findChild<QPushButton *>("ViewHardwareDescription")->isEnabled());
    }
    void mutantRepository()
    {
        GetDAQTestInternal::RunMutantRepository();
    }
    void mutantRejectedParameters()
    {
        GetDAQTestInternal::RunMutantRejectedParameters();
    }
    void coboCalibrationRepository()
    {
        GetDAQTestInternal::RunCoboCalibrationRepository();
    }
    void coboCalibrationPoliciesAndCoverage()
    {
        GetDAQTestInternal::RunCoboCalibrationPoliciesAndCoverage();
    }
    void coboCalibrationFailurePreservesOutput()
    {
        GetDAQTestInternal::RunCoboCalibrationFailurePreservesOutput();
    }
    void coboMonitoringRepository()
    {
        GetDAQTestInternal::RunCoboMonitoringRepository();
    }
    void coboMonitoringModeMapping()
    {
        GetDAQTestInternal::RunCoboMonitoringModeMapping();
    }
    void coboMonitoringAlarmPolicy()
    {
        GetDAQTestInternal::RunCoboMonitoringAlarmPolicy();
    }
    void coboMonitoringValidationPreservesOutputs()
    {
        GetDAQTestInternal::RunCoboMonitoringValidationPreservesOutputs();
    }
    void coboClockConfigurationRepository()
    {
        GetDAQTestInternal::RunCoboClockConfigurationRepository();
    }
    void coboClockValidationPreservesOutputs()
    {
        GetDAQTestInternal::RunCoboClockValidationPreservesOutputs();
    }
    void coboConfigureParametersRepository()
    {
        GetDAQTestInternal::RunCoboConfigureParametersRepository();
    }
    void coboConfigureParametersRejectBeforeOutput()
    {
        GetDAQTestInternal::RunCoboConfigureParametersRejectBeforeOutput();
    }
    void coboReadoutConfigurationRepository()
    {
        GetDAQTestInternal::RunCoboReadoutConfigurationRepository();
    }
    void coboReadoutConfigurationRejectsGlobalConflicts()
    {
        GetDAQTestInternal::RunCoboReadoutConfigurationRejectsGlobalConflicts();
    }
    void coboAgetConfigurationRepository()
    {
        GetDAQTestInternal::RunCoboAgetConfigurationRepository();
    }
    void coboAgetConfigurationMixedVersionAndOffsetConflict()
    {
        GetDAQTestInternal::RunCoboAgetConfigurationMixedVersionAndOffsetConflict();
    }
    void agetConfigurationRepository()
    {
        GetDAQTestInternal::RunAgetConfigurationRepository();
    }
    void agetConfigurationRejectsInvalidSelection()
    {
        GetDAQTestInternal::RunAgetConfigurationRejectsInvalidSelection();
    }
    void agetConfigurationIncludeAndNewOutput()
    {
        GetDAQTestInternal::RunAgetConfigurationIncludeAndNewOutput();
    }
    void hardwareDescriptionDefaultsAndNames()
    {
        GetDAQTestInternal::RunHardwareDescriptionDefaultsAndNames();
    }
    void hardwareDescriptionInvalidIsTransactional()
    {
        GetDAQTestInternal::RunHardwareDescriptionInvalidIsTransactional();
    }
    void hardwareDescriptionRepository()
    {
        GetDAQTestInternal::RunHardwareDescriptionRepository();
    }
    void recoveryRejectsChangedRouting()
    {
        QTemporaryDir dir; RunConfig original = Config(dir.path()); QString error;
        QVERIFY(RunRecorder::Reserve(original, &error));
        for (int variant = 0; variant < 5; ++variant) {
            RunConfig changed = original;
            if (variant == 0) changed.nodes[0].sentryHost = "different-storage";
            if (variant == 1) changed.nodes[0].routerPort += 1;
            if (variant == 2) changed.nodes[0].configureId = "different-settings";
            if (variant == 3) changed.nodes[0].soapInlineXml = true;
            if (variant == 4) changed.configDir += "-other";
            RunControl control;
            QSignalSpy logs(&control, &RunControl::LogSignal), phase(&control, &RunControl::ControlSignal);
            control.ConnectSlot(changed); control.RecoverSlot();
            QVERIFY(logs.last().at(0).toString().contains("run.yml"));
            QVERIFY(!phase.last().at(3).toBool());
            control.ShutdownSlot();
        }
    }
    void configurationSnapshotClosureAndRetry()
    {
        QTemporaryDir dir; RunConfig c = Config(dir.path()); c.simulation = false;
        c.nodes[0].configureId = "special"; MakeFiles(c);
        QFile external(dir.filePath("external.xcfg")); QVERIFY(external.open(QIODevice::WriteOnly));
        const QByteArray original = "<Setup><!-- preserve --><gain>42</gain></Setup>";
        external.write(original); external.close();
        QFile root(c.configDir + "/configure-special.xcfg"); QVERIFY(root.open(QIODevice::WriteOnly));
        root.write("<Setup include=\"../external.xcfg\"/>"); root.close();
        QVERIFY(QFile::rename(c.configDir + "/prepare-" + c.experiment + ".xcfg", c.configDir + "/prepare-" + c.experiment + ".xml"));
        QString error; QVERIFY(RunRecorder::Reserve(c, &error));
        const QString snapshotError = RunRecorder::Finish(c, 1.0);
        QVERIFY2(snapshotError.isEmpty(), qPrintable(snapshotError));
        QFile manifestFile(c.RunDirectory() + "/configuration/manifest.json"); QVERIFY(manifestFile.open(QIODevice::ReadOnly));
        const QByteArray manifestBytes = manifestFile.readAll();
        const auto manifest = QJsonDocument::fromJson(manifestBytes).object();
        QCOMPARE(manifest.value("capture_phase").toString(), QString("finalize_local_mirror"));
        QString stored;
        for (const auto &value : manifest.value("files").toArray()) {
            const auto entry = value.toObject();
            if (entry.value("source").toString() == QFileInfo(external).canonicalFilePath()) {
                QVERIFY(entry.value("external").toBool()); stored = entry.value("stored").toString();
            }
        }
        QVERIFY(!stored.isEmpty()); QFile saved(c.RunDirectory() + "/configuration/" + stored);
        QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), original); saved.close();
        // 发布目录后标记未落下：新的源文件不能替换已捕获的快照。
        QVERIFY(QFile::remove(c.RunDirectory() + "/backup.complete"));
        QVERIFY(external.open(QIODevice::WriteOnly)); external.write("<Setup/>"); external.close();
        QVERIFY(RunRecorder::Finish(c, 2.0).isEmpty());
        QVERIFY(saved.open(QIODevice::ReadOnly)); QCOMPARE(saved.readAll(), original); saved.close();
        QVERIFY(QFile::remove(c.RunDirectory() + "/backup.complete"));
        QVERIFY(saved.open(QIODevice::WriteOnly)); saved.write("corrupt"); saved.close();
        QVERIFY(!RunRecorder::Finish(c, 3.0).isEmpty());
        QVERIFY(!QFileInfo::exists(c.RunDirectory() + "/backup.complete"));
    }
    void configurationSnapshotMissingInclude()
    {
        QTemporaryDir dir; RunConfig c = Config(dir.path()); c.simulation = false; MakeFiles(c);
        QFile root(c.configDir + "/configure-" + c.experiment + ".xcfg");
        QVERIFY(root.open(QIODevice::WriteOnly)); root.write("<Setup include=\"missing.xcfg\"/>"); root.close();
        QString error; QVERIFY(RunRecorder::Reserve(c, &error));
        QVERIFY(!RunRecorder::Finish(c, 1.0).isEmpty());
        QVERIFY(!QFileInfo::exists(c.RunDirectory() + "/configuration"));
        QVERIFY(!QFileInfo::exists(c.RunDirectory() + "/backup.complete"));
    }
    void configurationDefaultLookup()
    {
        QTemporaryDir dir; QFile file(dir.filePath("defaults.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Setup><Board id=\"*\"><Channel id=\"*\"><gain>10</gain></Channel><Channel id=\"0\"/><Channel id=\"1\"/></Board>"
                   "<Board id=\"0\"><Channel id=\"1\"><gain>30</gain></Channel></Board><Board id=\"1\"><Channel id=\"*\"><gain>20</gain></Channel></Board></Setup>"); file.close();
        XcfgDocument document; QString error; QVERIFY(document.Load(file.fileName(), &error));
        for (const auto &item : QVector<QPair<QString, QString>>{{"Setup/Board[0]/Channel[0]/gain", "10"},
                {"Setup/Board[0]/Channel[1]/gain", "30"}, {"Setup/Board[1]/Channel[1]/gain", "10"}}) {
            const int found = document.Lookup(item.first, &error); QVERIFY2(found >= 0, qPrintable(error));
            QCOMPARE(document.Nodes()[found].value, item.second);
        }
        QVERIFY(document.Lookup("Setup/Board[99]/Channel[0]/gain", &error) < 0);
        QVERIFY(document.Lookup("Setup/Board[0]/Channel[99]/gain", &error) < 0);
        // 对照旧 CConfigTest::testSearchNestedBack3：未显式出现的 D[0] 不能先取同层 D[*] 的 x。
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<cfg><A id=\"*\"><B id=\"0\"><C id=\"0\"><D id=\"0\"><x>4.5</x></D></C></B></A>"
                   "<A id=\"0\"><B id=\"*\"><C id=\"*\"><D id=\"0\"><y>2.5</y></D></C></B>"
                   "<B id=\"0\"><C id=\"0\"><D id=\"*\"><x>0</x></D></C></B></A></cfg>"); file.close();
        QVERIFY(document.Load(file.fileName(), &error));
        const int x = document.Lookup("cfg/A[0]/B[0]/C[0]/D[0]/x", &error);
        QVERIFY2(x >= 0, qPrintable(error)); QCOMPARE(document.Nodes()[x].value, QString("4.5"));
        const int y = document.Lookup("cfg/A[0]/B[0]/C[0]/D[0]/y", &error);
        QVERIFY2(y >= 0, qPrintable(error)); QCOMPARE(document.Nodes()[y].value, QString("2.5"));
    }
    void configurationRepositorySamples()
    {
        const QString formats = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/format/CoboFormats.xcfg");
        const QString experiment = QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-pedestals.xcfg");
        QVERIFY(!formats.isEmpty() && !experiment.isEmpty());
        XcfgDocument document; QString error;
        QVERIFY2(document.Load(formats, &error), qPrintable(error)); QVERIFY(document.Sources().size() >= 8);
        QVERIFY2(document.Load(experiment, &error), qPrintable(error)); QVERIFY(document.Nodes().size() > 100);
    }
    void configurationSelectionFilePublicationIsAtomicAndNoReplace()
    {
        QTemporaryDir temporaryDirectory;
        QVERIFY(temporaryDirectory.isValid());
        const QByteArray selection = "{\"schema_version\":1,\"selection\":true}\n";
        const QString destination = temporaryDirectory.filePath("selection.json");
        QString error;
        QVERIFY2(PublishNewSelectionFile(destination, selection, &error), qPrintable(error));
        QFile published(destination);
        QVERIFY(published.open(QIODevice::ReadOnly));
        QCOMPARE(published.readAll(), selection);
        const QFile::Permissions permissions = published.permissions();
        QVERIFY(!(permissions & (QFile::WriteOwner | QFile::WriteUser |
                                 QFile::WriteGroup | QFile::WriteOther)));
        published.close();

        const QByteArray prior = "existing selection must stay intact";
        QFile existing(destination);
        QVERIFY(!PublishNewSelectionFile(destination, prior, &error));
        QVERIFY(existing.open(QIODevice::ReadOnly));
        QCOMPARE(existing.readAll(), selection);
        existing.close();

        QVERIFY(QDir(temporaryDirectory.path()).mkdir("occupied"));
        const QStringList filesBefore = QDir(temporaryDirectory.path()).entryList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
        QVERIFY(!PublishNewSelectionFile(temporaryDirectory.filePath("occupied"), prior, &error));
        const QStringList filesAfter = QDir(temporaryDirectory.path()).entryList(
            QDir::Files | QDir::Hidden | QDir::NoDotAndDotDot, QDir::Name);
        QCOMPARE(filesAfter, filesBefore);
        QVERIFY(!PublishNewSelectionFile(temporaryDirectory.filePath("empty.json"), QByteArray(), &error));
        QVERIFY(!QFileInfo::exists(temporaryDirectory.filePath("empty.json")));
    }
    void configurationIncludesAndLosslessEdit()
    {
        QTemporaryDir dir;
        const QByteArray original = QString::fromUtf8("<?xml version=\"1.0\"?>\n<Setup unknown=\"保留\">\n<!-- keep exactly -->\n<Node id=\"*\" include=\"child.xcfg\"/>\n<label type=\"string\">旧值</label>\n</Setup>\n").toUtf8();
        QFile root(dir.filePath("root.xcfg")); QVERIFY(root.open(QIODevice::WriteOnly)); root.write(original); root.close();
        QFile child(dir.filePath("child.xcfg")); QVERIFY(child.open(QIODevice::WriteOnly));
        const QByteArray childBytes = "<Node><gain unit=\"fC\" range=\"[0, 100]\"> 20 </gain><enabled>true</enabled></Node>\n";
        child.write(childBytes); child.close();
        XcfgDocument document; QString error; QVERIFY2(document.Load(root.fileName(), &error), qPrintable(error));
        QCOMPARE(document.Sources().size(), 2);
        int gain = -1, label = -1;
        for (int i = 0; i < document.Nodes().size(); ++i) {
            if (document.Nodes()[i].name == "gain") gain = i;
            if (document.Nodes()[i].name == "label") label = i;
        }
        QVERIFY(gain >= 0 && label >= 0);
        QVERIFY(!document.SetValue(gain, "101", &error));
        QVERIFY2(document.SetValue(gain, "30", &error), qPrintable(error));
        QVERIFY(child.open(QIODevice::ReadOnly)); QCOMPARE(child.readAll(), QByteArray(childBytes).replace(" 20 ", " 30 ")); child.close();
        QVERIFY(root.open(QIODevice::ReadOnly)); QCOMPARE(root.readAll(), original); root.close();
        // 重载后的索引重新查找；修改中文和 XML 特殊字符，其他字节仍完全相同。
        for (int i = 0; i < document.Nodes().size(); ++i) if (document.Nodes()[i].name == "label") label = i;
        QVERIFY2(document.SetValue(label, "A<&B", &error), qPrintable(error));
        QVERIFY(root.open(QIODevice::ReadOnly)); QCOMPARE(root.readAll(), QByteArray(original).replace(QString::fromUtf8("旧值").toUtf8(), "A&lt;&amp;B")); root.close();
        ConfigWindow view(dir.path(), true); QVERIFY(view.Open(root.fileName(), &error));
        QVERIFY(!view.findChild<QLineEdit *>("ConfigValue")->isEnabled());
    }
    void configurationCycleAndExternalChange()
    {
        QTemporaryDir dir; QFile root(dir.filePath("root.xcfg"));
        QVERIFY(root.open(QIODevice::WriteOnly)); root.write("<Root><a>1</a></Root>"); root.close();
        XcfgDocument document; QString error; QVERIFY(document.Load(root.fileName(), &error));
        QVERIFY(root.open(QIODevice::WriteOnly)); root.write("<Root><a>2</a></Root>"); root.close();
        QVERIFY(!document.SetValue(1, "3", &error)); QVERIFY(error.contains("changed"));
        QVERIFY(root.open(QIODevice::WriteOnly)); root.write("<Root include=\"root.xcfg\"/>"); root.close();
        QVERIFY(!document.Load(root.fileName(), &error)); QVERIFY(error.contains("Recursive"));
        QCOMPARE(document.Nodes()[1].value, QString("1")); // 失败加载不替换此前模型。
    }
    void configurationDeclaredRanges()
    {
        XcfgDocument::Node n; n.scalar = true; n.value = "10"; QString error;
        n.attributes["range"] = "]0, 10[";
        QVERIFY(!XcfgDocument::CheckValue(n, "0", &error)); QVERIFY(!XcfgDocument::CheckValue(n, "10", &error));
        QVERIFY(XcfgDocument::CheckValue(n, "9", &error));
        n.attributes["range"] = "[-inf, +inf]"; QVERIFY(XcfgDocument::CheckValue(n, "-5", &error));
        n.attributes["type"] = "real"; QVERIFY(!XcfgDocument::CheckValue(n, "nan", &error));
        n.attributes["type"] = "string"; n.attributes["range"] = "{fast, slow}";
        QVERIFY(XcfgDocument::CheckValue(n, "fast", &error)); QVERIFY(!XcfgDocument::CheckValue(n, "other", &error));
        n.attributes["type"] = "hex"; n.attributes["range"] = "[0x0, 0xFF]";
        QVERIFY(XcfgDocument::CheckValue(n, "FF", &error)); QVERIFY(!XcfgDocument::CheckValue(n, "100", &error));
    }
    void configurationSharedClassPropertiesAndEdit()
    {
        QTemporaryDir dir; QFile file(dir.filePath("shared.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray bytes="<Setup><Board id=\"*\"><gain unit=\"V\" range=\"[0,10]\">2</gain><other unit=\"m\">3</other></Board>"
            "<Board id=\"0\"><gain>4</gain><gain id=\"1\">5</gain></Board>"
            "<Board id=\"1\"><gain unit=\"mV\">6</gain></Board></Setup>";
        file.write(bytes); file.close(); XcfgDocument document; QString error;
        QVERIFY(document.Load(file.fileName(),&error)); XcfgDocument::Node value; XcfgDocument::ScalarProperties properties;
        QVERIFY2(document.ResolveScalar("Setup/Board[0]/gain[1]",&value,&properties,&error),qPrintable(error));
        QCOMPARE(value.value,QString("5")); QCOMPARE(properties.classPath,QString("Setup/Board/gain"));
        QCOMPARE(properties.unit,QString("mV")); QCOMPARE(properties.range,QString("[0,10]"));
        QCOMPARE(document.Nodes()[properties.unitSource].value,QString("6")); QCOMPARE(document.Nodes()[properties.rangeSource].value,QString("2"));
        const int selected=document.Lookup("Setup/Board[0]/gain",&error); QVERIFY(selected>=0);
        QVERIFY(!document.Nodes()[selected].attributes.contains("range")); QVERIFY(!document.SetValue(selected,"11",&error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),bytes); file.close();
        QVERIFY2(document.SetValue(selected,"8",&error),qPrintable(error));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),QByteArray(bytes).replace("<gain>4</gain>","<gain>8</gain>")); file.close();
        QVERIFY(document.ResolveScalar("Setup/Board[0]/other",&value,&properties,&error)); QCOMPARE(properties.unit,QString("m"));
        ExperimentReader reader(document);
        QCOMPARE(reader.Scalar("Setup/Board[0]/gain",QString(),"mV"),QString("8"));
        QVERIFY(reader.parameters.last().toObject().contains("shared_metadata"));
        ConfigWindow view(dir.path(),true); QVERIFY(view.Open(file.fileName(),&error));
        view.findChild<QLineEdit *>("ConfigLookupPath")->setText("Setup/Board[0]/gain");
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigLookupButton"),Qt::LeftButton);
        const auto text=view.findChild<QLabel *>("ConfigStatus")->text();
        QVERIFY(text.contains("Effective unit: mV")); QVERIFY(text.contains("Effective range: [0,10]"));
        QVERIFY(text.contains("Board[1]/gain")); QVERIFY(text.contains("Board[*]/gain"));
    }
    void configurationSharedClassesThroughIncludes()
    {
        QTemporaryDir dir; QFile child(dir.filePath("child.xcfg")); QVERIFY(child.open(QIODevice::WriteOnly));
        child.write("<Template><gain range=\"[0,10]\">2</gain></Template>"); child.close();
        QFile root(dir.filePath("root.xcfg")); QVERIFY(root.open(QIODevice::WriteOnly));
        root.write("<Setup><Board id=\"*\" include=\"child.xcfg\"/><Board id=\"0\"><gain unit=\"V\">5</gain></Board>"
            "<Other include=\"child.xcfg\"><gainOverride>1</gainOverride></Other></Setup>"); root.close();
        XcfgDocument document; QString error; QVERIFY(document.Load(root.fileName(),&error));
        XcfgDocument::Node value; XcfgDocument::ScalarProperties properties;
        QVERIFY2(document.ResolveScalar("Setup/Board[0]/gain",&value,&properties,&error),qPrintable(error));
        QCOMPARE(properties.unit,QString("V")); QCOMPARE(properties.range,QString("[0,10]"));
        QVERIFY(document.Nodes()[properties.rangeSource].file.endsWith("child.xcfg"));
        const int source=document.Lookup("Setup/Board[*]/gain",&error); QVector<XcfgDocument::ScalarProperties> contexts;
        QVERIFY(document.DeclarationProperties(source,&contexts,&error)); QCOMPARE(contexts.size(),2);
        QVERIFY(!document.SetValue(source,"11",&error));
        QVERIFY(root.open(QIODevice::WriteOnly));
        root.write("<Setup><Board id=\"0\"><gain>5</gain></Board><Board id=\"1\" include=\"child.xcfg\"/></Setup>"); root.close();
        QVERIFY(document.Load(root.fileName(),&error));
        QVERIFY2(document.ResolveScalar("Setup/Board[0]/gain",&value,&properties,&error),qPrintable(error));
        QCOMPARE(value.value,QString("5"));QCOMPARE(properties.range,QString("[0,10]"));
        QCOMPARE(properties.unit,QString());QVERIFY(properties.rangeSource>=0);
    }
    void configurationInstanceOverrideIsReviewedAndReadOnlyIsEnforced()
    {
        QTemporaryDir directory; QFile file(directory.filePath("override.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original="<Setup><Board id=\"*\"><gain range=\"[0,10]\">2</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>";
        file.write(original); file.close(); QString error; ConfigWindow view(directory.path(),false); QVERIFY(view.Open(file.fileName(),&error));
        const auto path=view.findChild<QLineEdit *>("ConfigLookupPath"),value=view.findChild<QLineEdit *>("ConfigOverrideValue");
        auto preview=view.findChild<QPushButton *>("ConfigOverridePreview"),apply=view.findChild<QPushButton *>("ConfigOverrideApply");
        path->setText("Setup/Board[0]/gain"); value->setText("4"); QVERIFY(!apply->isEnabled());
        QTest::mouseClick(preview,Qt::LeftButton); QVERIFY(apply->isEnabled());
        QVERIFY(view.findChild<QLabel *>("ConfigStatus")->text().contains("Preview only"));
        QVERIFY(file.open(QIODevice::ReadOnly)); QCOMPARE(file.readAll(),original); file.close();
        value->setText("5"); QVERIFY(!apply->isEnabled()); value->setText("4"); QTest::mouseClick(preview,Qt::LeftButton);
        QTest::mouseClick(apply,Qt::LeftButton); QVERIFY(!apply->isEnabled());
        XcfgDocument document; QVERIFY(document.Load(file.fileName(),&error)); ExperimentReader reader(document);
        QCOMPARE(reader.Scalar("Setup/Board[0]/gain"),QString("4")); QCOMPARE(reader.Scalar("Setup/Board[1]/gain"),QString("2"));
        ConfigWindow readonly(directory.path(),true); QVERIFY(readonly.Open(file.fileName(),&error));
        QVERIFY(!readonly.findChild<QLineEdit *>("ConfigOverrideValue")->isEnabled());
        QVERIFY(!readonly.findChild<QPushButton *>("ConfigOverridePreview")->isEnabled());
        QVERIFY(!readonly.findChild<QPushButton *>("ConfigOverrideApply")->isEnabled());
    }
    void configurationCdataValueEditPreservesCommentsAndReviewedUndo()
    {
        QTemporaryDir directory;const auto path=directory.filePath("cdata.xcfg");const QByteArray original="<Setup><n type=\"int\" range=\"[0,9]\"><!-- keep --><![CDATA[2]]><?keep tail?></n></Setup>";
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();QString error;
        ConfigWindow view(directory.path(),false);QVERIFY2(view.Open(path,&error),qPrintable(error));auto tree=view.findChild<QTreeWidget *>("ConfigTree");QVERIFY(tree);
        tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));auto value=view.findChild<QLineEdit *>("ConfigValue");auto save=view.findChild<QPushButton *>("ConfigSaveValue");QVERIFY(value && save && save->isEnabled());
        value->setText("3");save->click();QVERIFY(file.open(QIODevice::ReadOnly));const auto changed=file.readAll();file.close();QCOMPARE(changed,QByteArray("<Setup><n type=\"int\" range=\"[0,9]\"><!-- keep --><![CDATA[3]]><?keep tail?></n></Setup>"));
        auto undo=view.findChild<QPushButton *>("ConfigUndo");QVERIFY(undo->isEnabled());undo->click();auto dialog=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(dialog);QCOMPARE(dialog->findChild<QPlainTextEdit *>("ConfigHistoryUpdated")->toPlainText(),QString::fromUtf8(original));
        dialog->findChild<QPushButton *>("ConfigHistoryApply")->click();QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        ConfigWindow readonly(directory.path(),true);QVERIFY(readonly.Open(path,&error));auto readTree=readonly.findChild<QTreeWidget *>("ConfigTree");readTree->setCurrentItem(readTree->topLevelItem(0)->child(0)->child(0));QVERIFY(!readonly.findChild<QPushButton *>("ConfigSaveValue")->isEnabled());
    }
    void configurationLegacySplitScalarIsDisplayedReadOnly()
    {
        QTemporaryDir directory;const auto path=directory.filePath("split.xcfg");const QByteArray original="<Setup><label type=\"string\">a<!-- comment -->b</label></Setup>";
        QFile file(path);QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();QString error;
        ConfigWindow view(directory.path(),false);QVERIFY2(view.Open(path,&error),qPrintable(error));auto tree=view.findChild<QTreeWidget *>("ConfigTree");QVERIFY(tree);
        tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));auto value=view.findChild<QLineEdit *>("ConfigValue");auto save=view.findChild<QPushButton *>("ConfigSaveValue");
        QVERIFY(value && save);QCOMPARE(value->text(),QString("b"));QVERIFY(!value->isEnabled());QVERIFY(!save->isEnabled());
        QVERIFY(value->toolTip().contains("split across XML markup"));QVERIFY(view.findChild<QLabel *>("ConfigStatus")->text().contains("normalize the XML text explicitly"));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void configurationHistoryReviewsUndoRedoAndRejectsStaleWindow()
    {
        QTemporaryDir directory;QFile file(directory.filePath("history.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original=R"(<Setup><!-- preserve --><n type="int" range="[0,10]">2</n></Setup>)";
        file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY2(view.Open(file.fileName(),&error),qPrintable(error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");QVERIFY(tree);
        auto undo=view.findChild<QPushButton *>("ConfigUndo"),redo=view.findChild<QPushButton *>("ConfigRedo");
        QVERIFY(!undo->isEnabled());QVERIFY(!redo->isEnabled());
        tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));
        view.findChild<QLineEdit *>("ConfigValue")->setText("3");QTest::mouseClick(view.findChild<QPushButton *>("ConfigSaveValue"),Qt::LeftButton);
        QVERIFY(undo->isEnabled());QVERIFY(!redo->isEnabled());
        QTest::mouseClick(undo,Qt::LeftButton);auto dialog=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(dialog);
        QCOMPARE(dialog->findChild<QPlainTextEdit *>("ConfigHistoryUpdated")->toPlainText(),QString::fromUtf8(original));
        QVERIFY(dialog->findChild<QTreeWidget *>("ConfigHistoryChanges")->topLevelItemCount()>0);
        QVERIFY(file.open(QIODevice::ReadOnly));const auto modified=file.readAll();file.close();QVERIFY(modified!=original);
        QTest::mouseClick(dialog->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();QVERIFY(redo->isEnabled());QVERIFY(!undo->isEnabled());
        QTest::mouseClick(redo,Qt::LeftButton);dialog=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(dialog);
        QTest::mouseClick(dialog->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),modified);file.close();
        QTest::mouseClick(undo,Qt::LeftButton);dialog=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(dialog);
        QVERIFY(view.Open(file.fileName(),&error));QVERIFY(!undo->isEnabled());QVERIFY(!redo->isEnabled());
        QTest::mouseClick(dialog->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);
        QVERIFY(dialog->findChild<QLabel *>("ConfigHistoryStatus")->text().contains("Configuration changed"));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),modified);file.close();
        ConfigWindow readonly(directory.path(),true);QVERIFY(readonly.Open(file.fileName(),&error));
        QVERIFY(!readonly.findChild<QPushButton *>("ConfigUndo")->isEnabled());QVERIFY(!readonly.findChild<QPushButton *>("ConfigRedo")->isEnabled());
    }
    void configurationStructureEditsEnterTheSameUndoHistory()
    {
        QTemporaryDir directory;QFile file(directory.filePath("history.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original=R"(<Setup><Board id="0"><n type="int">2</n></Board></Setup>)";
        file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY2(view.Open(file.fileName(),&error),qPrintable(error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        auto edit=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(edit);
        edit->findChild<QLineEdit *>("ConfigStructureName")->setText("Extra");
        QTest::mouseClick(edit->findChild<QPushButton *>("ConfigStructurePreview"),Qt::LeftButton);
        auto apply=edit->findChild<QPushButton *>("ConfigStructureApply");QVERIFY2(apply->isEnabled(),qPrintable(edit->findChild<QLabel *>("ConfigStructureStatus")->text()));
        QTest::mouseClick(apply,Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        auto undo=view.findChild<QPushButton *>("ConfigUndo");QVERIFY(undo->isEnabled());QTest::mouseClick(undo,Qt::LeftButton);
        auto review=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(review);
        QVERIFY(review->findChild<QPlainTextEdit *>("ConfigHistoryReport")->toPlainText().contains("object_changes"));
        QTest::mouseClick(review->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);
    }
    void configurationLocalIncludeImportReview_data() {
        QTest::addColumn<int>("operation");
        QTest::newRow("include")<<10;QTest::newRow("change-url")<<11;QTest::newRow("import")<<12;
    }
    void configurationLocalIncludeImportReview() {
        QFETCH(int,operation);QTemporaryDir directory;QFile main(directory.filePath("main.xcfg")),input(directory.filePath("new config.xcfg")),old(directory.filePath("old.xcfg"));
        const QByteArray original=operation==11 ? "<Setup><Board id=\"0\" include=\"old.xcfg\"><local type=\"int\">1</local></Board></Setup>" : "<Setup><Board id=\"0\"><old type=\"int\">1</old></Board></Setup>";
        const QByteArray external="<External><!-- keep import --><n type=\"int\">4</n></External>";
        QVERIFY(main.open(QIODevice::WriteOnly));main.write(original);main.close();QVERIFY(input.open(QIODevice::WriteOnly));input.write(external);input.close();
        QVERIFY(old.open(QIODevice::WriteOnly));old.write("<Old><before type=\"int\">2</before></Old>");old.close();
        ConfigWindow view(directory.path(),false);QString error;QVERIFY(view.Open(main.fileName(),&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);auto dialog=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(dialog);
        dialog->findChild<QComboBox *>("ConfigStructureOperation")->setCurrentIndex(operation);
        auto path=dialog->findChild<QLineEdit *>("ConfigStructureIncludePath");QVERIFY(path && path->isEnabled());path->setText("new config.xcfg");
        auto preview=dialog->findChild<QPushButton *>("ConfigStructurePreview"),apply=dialog->findChild<QPushButton *>("ConfigStructureApply");
        QTest::mouseClick(preview,Qt::LeftButton);QVERIFY2(apply->isEnabled(),qPrintable(dialog->findChild<QLabel *>("ConfigStructureStatus")->text()));
        QVERIFY(main.open(QIODevice::ReadOnly));QCOMPARE(main.readAll(),original);main.close();
        QVERIFY(dialog->findChild<QTreeWidget *>("ConfigStructureChangeTable")->topLevelItemCount()>0);
        auto dependencies=dialog->findChild<QTreeWidget *>("ConfigStructureDependencies");QVERIFY(dependencies && dependencies->topLevelItemCount()>=1);
        bool found=false;for(int i=0;i<dependencies->topLevelItemCount();++i)found=found || dependencies->topLevelItem(i)->text(0)==QFileInfo(input).canonicalFilePath();QVERIFY(found);
        path->setText("different.xcfg");QVERIFY(!apply->isEnabled());path->setText("new config.xcfg");QTest::mouseClick(preview,Qt::LeftButton);QVERIFY(apply->isEnabled());
        QTest::mouseClick(apply,Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        XcfgDocument document;QVERIFY(document.Load(main.fileName(),&error));QCOMPARE(document.Nodes()[document.Lookup("Setup/Board[0]/n",&error)].value,QString("4"));
        if(operation==12)QCOMPARE(document.Sources().size(),1);
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigUndo"),Qt::LeftButton);auto undo=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(undo);
        QTest::mouseClick(undo->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(main.open(QIODevice::ReadOnly));QCOMPARE(main.readAll(),original);QVERIFY(input.open(QIODevice::ReadOnly));QCOMPARE(input.readAll(),external);
    }
    void configurationStructureMaterializationIsReviewedBeforeSaving()
    {
        QTemporaryDir directory;QFile file(directory.filePath("structure.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original="<Setup><Board id=\"*\"><Module><gain range=\"[0,10]\">2</gain></Module></Board><Board id=\"0\" extra=\"keep\"/></Setup>";
        file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(file.fileName(),&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");QVERIFY(tree);
        tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(1));
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(dialog);
        auto operation=dialog->findChild<QComboBox *>("ConfigStructureOperation");operation->setCurrentIndex(4);
        auto preview=dialog->findChild<QPushButton *>("ConfigStructurePreview"),apply=dialog->findChild<QPushButton *>("ConfigStructureApply");
        QTest::mouseClick(preview,Qt::LeftButton);QVERIFY2(apply->isEnabled(),qPrintable(dialog->findChild<QLabel *>("ConfigStructureStatus")->text()));
        QVERIFY(dialog->findChild<QLabel *>("ConfigStructureStatus")->text().contains("Preview only"));
        QVERIFY(dialog->findChild<QPlainTextEdit *>("ConfigStructureChanges")->toPlainText().contains("\"effective_changed\": false"));
        QVERIFY(dialog->findChild<QTreeWidget *>("ConfigStructureChangeTable")->topLevelItemCount()>0);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QTest::mouseClick(apply,Qt::LeftButton);
        XcfgDocument document;QVERIFY(document.Load(file.fileName(),&error));
        XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[0]/Module/gain",&value,&properties,&error));QCOMPARE(value.path,QString("Setup/Board[0]/Module/gain"));
        QVERIFY(view.findChild<QLabel *>("ConfigStatus")->text().contains("No hardware command"));
    }
    void configurationStructureScalarPropertiesShowSharedChanges()
    {
        QTemporaryDir directory;QFile file(directory.filePath("properties.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original="<Setup><Board id=\"0\"><gain unit=\"V\" range=\"[0,10]\">2</gain></Board><Board id=\"1\"><gain>3</gain></Board></Setup>";
        file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(file.fileName(),&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0)->child(0));
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(dialog);
        dialog->findChild<QComboBox *>("ConfigStructureOperation")->setCurrentIndex(9);
        QCOMPARE(dialog->findChild<QLineEdit *>("ConfigStructureUnit")->text(),QString("V"));
        QVERIFY(!dialog->findChild<QLineEdit *>("ConfigStructureValue")->isEnabled());
        dialog->findChild<QLineEdit *>("ConfigStructureUnit")->setText("mV");
        QTest::mouseClick(dialog->findChild<QPushButton *>("ConfigStructurePreview"),Qt::LeftButton);
        auto apply=dialog->findChild<QPushButton *>("ConfigStructureApply");QVERIFY(apply->isEnabled());
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigStructureChangeTable")->topLevelItemCount(),2);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        QTest::mouseClick(apply,Qt::LeftButton);
        XcfgDocument document;QVERIFY(document.Load(file.fileName(),&error));XcfgDocument::Node value;XcfgDocument::ScalarProperties properties;
        QVERIFY(document.ResolveScalar("Setup/Board[1]/gain",&value,&properties,&error));QCOMPARE(properties.unit,QString("mV"));QCOMPARE(value.value,QString("3"));
    }
    void configurationDisplaysImportedSharedMetadata()
    {
        QTemporaryDir directory;
        QFile included(directory.filePath("defaults.xcfg"));QVERIFY(included.open(QIODevice::WriteOnly));
        included.write("<Template><gain unit=\"V\" range=\"[0,10]\">3</gain></Template>");included.close();
        QFile file(directory.filePath("properties.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Setup><Board id=\"0\"><gain>2</gain></Board><Board id=\"1\" include=\"defaults.xcfg\"/></Setup>");file.close();
        QString error;ConfigWindow view(directory.path(),false);QVERIFY2(view.Open(file.fileName(),&error),qPrintable(error));
        view.findChild<QLineEdit *>("ConfigLookupPath")->setText("Setup/Board[0]/gain");
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigLookupButton"),Qt::LeftButton);
        const auto status=view.findChild<QLabel *>("ConfigStatus")->text();
        QVERIFY(status.contains("Effective unit: V"));QVERIFY(status.contains("Effective range: [0,10]"));
        QVERIFY(status.contains("defaults.xcfg"));
    }
    void configurationStructurePreviewCannotCrossOpenOrReadonlyBoundary()
    {
        QTemporaryDir directory;QFile file(directory.filePath("structure.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original="<Setup><Board id=\"0\"/></Setup>";file.write(original);file.close();
        QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(file.fileName(),&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(dialog);
        dialog->findChild<QLineEdit *>("ConfigStructureName")->setText("Board");dialog->findChild<QLineEdit *>("ConfigStructureId")->setText("1");
        auto preview=dialog->findChild<QPushButton *>("ConfigStructurePreview"),apply=dialog->findChild<QPushButton *>("ConfigStructureApply");
        QTest::mouseClick(preview,Qt::LeftButton);QVERIFY(apply->isEnabled());
        dialog->findChild<QLineEdit *>("ConfigStructureId")->setText("2");QVERIFY(!apply->isEnabled());
        QTest::mouseClick(preview,Qt::LeftButton);QVERIFY(apply->isEnabled());
        QVERIFY(view.Open(file.fileName(),&error));QTest::mouseClick(apply,Qt::LeftButton);
        QVERIFY(dialog->findChild<QLabel *>("ConfigStructureStatus")->text().contains("Configuration changed"));
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();dialog->close();
        ConfigWindow readonly(directory.path(),true);QVERIFY(readonly.Open(file.fileName(),&error));
        QVERIFY(!readonly.findChild<QPushButton *>("ConfigStructureOpen")->isEnabled());
        QTest::mouseClick(readonly.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        QVERIFY(!readonly.findChild<QDialog *>("ConfigStructureDialog"));
    }
    void configurationEffectiveSnapshotsRemainAvailableWhileConnected()
    {
        QTemporaryDir directory; QFile file(directory.filePath("snapshot.xcfg")); QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("<Setup><Board id=\"*\"><gain>2</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>"); file.close();
        QString error; const auto saved=directory.filePath("baseline.json"); ConfigWindow view(directory.path(),true); QVERIFY(view.Open(file.fileName(),&error));
        QVERIFY(view.findChild<QPushButton *>("ConfigSnapshotExport")->isEnabled()); QVERIFY(view.findChild<QPushButton *>("ConfigSnapshotCompare")->isEnabled());
        QVERIFY2(view.ExportEffectiveSnapshot(saved,&error),qPrintable(error)); QVERIFY(!view.ExportEffectiveSnapshot(saved,&error));
        QVERIFY2(view.CompareEffectiveSnapshot(saved,&error),qPrintable(error));
        auto pane=view.findChild<QPlainTextEdit *>("ConfigSnapshotReview"); QVERIFY(pane->isReadOnly());
        auto report=QJsonDocument::fromJson(pane->toPlainText().toUtf8()).object(); QVERIFY(!report["effective_changed"].toBool());
        QVERIFY(file.open(QIODevice::WriteOnly)); file.write("<Setup><Board id=\"*\"><gain>3</gain></Board><Board id=\"0\"/><Board id=\"1\"/></Setup>"); file.close();
        QVERIFY(!view.CompareEffectiveSnapshot(saved,&error)); QVERIFY(pane->toPlainText().isEmpty());
        QVERIFY(view.Open(file.fileName(),&error)); QVERIFY(view.CompareEffectiveSnapshot(saved,&error));
        report=QJsonDocument::fromJson(pane->toPlainText().toUtf8()).object(); QVERIFY(report["effective_changed"].toBool()); QCOMPARE(report["scalar_changes"].toArray().size(),2);
        QVERIFY(!view.findChild<QPushButton *>("ConfigOverridePreview")->isEnabled());
    }
    void configurationSnapshotBackgroundCompletionCannotReviewAnotherDocument()
    {
        QTemporaryDir directory; const auto first=directory.filePath("first.xcfg"),second=directory.filePath("second.xcfg"),saved=directory.filePath("saved.json");
        QFile file(first); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("<Setup><gain>2</gain></Setup>"); file.close();
        file.setFileName(second); QVERIFY(file.open(QIODevice::WriteOnly)); file.write("<Setup><gain>3</gain></Setup>"); file.close();
        QString error; ConfigWindow view(directory.path(),true); QVERIFY(view.Open(first,&error));
        const bool nativeDialogsDisabled=QCoreApplication::testAttribute(Qt::AA_DontUseNativeDialogs);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,true);
        bool selected=false;
        QTimer::singleShot(0,&view,[&] {
            auto dialog=qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
            if (!dialog) return;
            selected=true; dialog->selectFile(saved); QMetaObject::invokeMethod(dialog,"accept",Qt::QueuedConnection);
        });
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigSnapshotExport"),Qt::LeftButton);
        QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs,nativeDialogsDisabled); QVERIFY(selected);
        QVERIFY(view.Open(second,&error));
        QTRY_VERIFY(view.findChild<QPushButton *>("ConfigSnapshotExport")->isEnabled());
        QVERIFY(QFile::exists(saved)); QVERIFY(view.findChild<QLabel *>("ConfigStatus")->text().contains("earlier loaded configuration"));
        QVERIFY(view.findChild<QPlainTextEdit *>("ConfigSnapshotReview")->toPlainText().isEmpty());
        QVERIFY(view.CompareEffectiveSnapshot(saved,&error));
        const auto report=QJsonDocument::fromJson(view.findChild<QPlainTextEdit *>("ConfigSnapshotReview")->toPlainText().toUtf8()).object();
        QVERIFY(report["effective_changed"].toBool());
    }
    void configurationPublicationBuildsReviewedLocalPackageWhileConnected()
    {
        QTemporaryDir directory; QString error;
        const auto source=QFINDTESTDATA("../source/get/GetBench-software-release-20210208/data/config/configure-ExternalTrigger-2CoBo_1Mutant.xcfg");
        const auto profile=QFINDTESTDATA("../config/experiment/system-profile.external-trigger.example.json");
        QVERIFY(!source.isEmpty()); QVERIFY(!profile.isEmpty()); ConfigWindow view(directory.path(),true); QVERIFY(view.Open(source,&error));
        QVERIFY(view.findChild<QPushButton *>("ConfigPublicationBuild")->isEnabled()); const auto package=directory.filePath("reviewed-v1");
        QVERIFY2(view.PublishSystemConfiguration(profile,package,&error),qPrintable(error));
        QFile manifest(package+"/publication.json"); QVERIFY(manifest.open(QIODevice::ReadOnly)); const auto before=manifest.readAll(); manifest.close();
        QVERIFY(!view.PublishSystemConfiguration(profile,package,&error)); QVERIFY(manifest.open(QIODevice::ReadOnly)); QCOMPARE(manifest.readAll(),before);
        QVERIFY(view.findChild<QLabel *>("ConfigStatus")->text().contains("Hardware loading is not verified"));
        QVERIFY(!view.findChild<QPushButton *>("ConfigOverridePreview")->isEnabled());
        QVERIFY(QFile::exists(package+"/profile.json")); QVERIFY(QFile::exists(package+"/effective.json")); QVERIFY(QFile::exists(package+"/experiment.json"));
    }
    void configurationRepositoryWindowIsSeparateAndRequiresEndpointCredential()
    {
        QTemporaryDir directory;ConfigWindow configuration(directory.path(),true);
        QTest::mouseClick(configuration.findChild<QPushButton *>("OpenConfigurationRepository"),Qt::LeftButton);
        auto window=configuration.findChild<ConfigurationRepositoryWindow *>();QVERIFY(window);
        QCOMPARE(window->findChild<QLineEdit *>("RepositoryAddress")->text(),QString("127.0.0.1"));
        QVERIFY(window->findChild<QPlainTextEdit *>("RepositoryResult")->isReadOnly());
        QVERIFY(!window->findChild<QPushButton *>("RepositoryResume")->isEnabled());
        QTest::mouseClick(window->findChild<QPushButton *>("RepositoryQueryIdentity"),Qt::LeftButton);
        QVERIFY(window->findChild<QPlainTextEdit *>("RepositoryResult")->toPlainText().contains("endpoint"));
        QVERIFY(window->findChild<QPushButton *>("RepositoryQueryIdentity")->isEnabled());window->close();
    }
    void configureReadbackWindowPreservesHistoryAndWithdrawsStaleRows()
    {
        ConfigurationReadbackWindow window;window.Bind(QString(64,'b'),1,true);window.show();QSignalBlocker block(&window);
        auto board=window.findChild<QTableWidget *>("ReadbackHardware");auto observation=window.findChild<QLabel *>("ReadbackObservation");
        QVERIFY(board && observation);QVERIFY(board->editTriggers()==QAbstractItemView::NoEditTriggers);
        const auto body=QByteArray::fromStdString(ConfigurationReadbackFixture::Cache());window.Observe(body,{},1);
        QCOMPARE(board->rowCount(),1);QCOMPARE(board->item(0,2)->text(),QString("2"));QCOMPARE(board->item(0,3)->text(),QString("1"));QCOMPARE(board->item(0,4)->text(),QString("1"));
        QVERIFY(board->item(0,0)->toolTip().contains("9007199254740993"));QVERIFY(observation->text().contains("unverified"));
        QTest::qWait(8200);QVERIFY(observation->text().contains("expired"));QCOMPARE(board->rowCount(),0);
        window.Observe(QByteArray::fromStdString(ConfigurationReadbackFixture::Cache(true)),{},1);QCOMPARE(board->rowCount(),1);QVERIFY(observation->text().contains("reconciliation"));
        window.Observe({},"Unavailable",1);QCOMPARE(board->rowCount(),0);QCOMPARE(observation->text(),QString("Unavailable"));
        window.Bind(QString(64,'b'),2,false);window.Observe(body,{},1);QCOMPARE(board->rowCount(),0);QVERIFY(observation->text().contains("disconnected"));
        window.Bind(QString(64,'c'),3,true);window.Observe(body,{},3);QCOMPARE(board->rowCount(),0);QVERIFY(observation->text().contains("Invalid"));
    }
    void nativeOwnershipViewExpiresAndRejectsLateConnectionReplies()
    {
        MainWindow window;RunConfig config=RunConfig::Default();config.nativeEcc=true;config.simulation=false;
        config.nativeClient.snapshotSha256=QString(64,'b');config.nativeClient.inputBindingSha256=QString(64,'c');window.SetConfig(config);
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Q_ARG(QString,QString("Native monitoring")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
        QSignalSpy inspected(&window,&MainWindow::InspectOwnershipSignal);
        auto action=window.findChild<QAction *>("NativeOwnershipInfo");QVERIFY(action && action->isVisible() && action->isEnabled());action->trigger();
        QTRY_VERIFY(window.findChild<QTableWidget *>("NativeOwnershipTable"));QTest::qWait(50);QVERIFY(!inspected.isEmpty());
        const quint64 generation=inspected.last()[0].toULongLong();QSignalBlocker block(&window);
        const auto table=window.findChild<QTableWidget *>("NativeOwnershipTable");QVERIFY(table->editTriggers()==QAbstractItemView::NoEditTriggers);
        QJsonObject ownership{{"enabled",true},{"scope","single-host-cooperating-ecc"},{"directory","/locks"},
            {"held_at_last_check",true},{"fault_latched",false},{"cached",true},{"board_global_lock_verified",false},
            {"cross_host_lock_verified",false},{"resources",QJsonArray{"127.0.0.1"}}};
        QJsonObject result{{"schema",1},{"kind","getdaq.ecc-ownership"},{"instance_id",QString(64,'a')},
            {"snapshot_sha256",QString(64,'b')},{"ownership",ownership}};
        const auto show=[&](const QByteArray &body,const QString &error) {
            return QMetaObject::invokeMethod(&window,"ShowOwnershipSlot",Q_ARG(QByteArray,body),Q_ARG(QString,error),Q_ARG(quint64,generation));
        };
        const auto row=[&](const QString &name) {
            for (int i=0;i<table->rowCount();++i) if (table->item(i,0)->text()==name) return table->item(i,1)->text();
            return QString();
        };
        const auto body=QJsonDocument(result).toJson(QJsonDocument::Compact);QVERIFY(show(body,QString()));
        QCOMPARE(row("Observation"),QString("Host reservation held at the last ECC check"));
        QCOMPARE(row("Board-global / cross-host control"),QString("Not verified"));
        QCOMPARE(row("Reserved target addresses"),QString("127.0.0.1"));
        QTest::qWait(8200);QVERIFY(row("Observation").contains("expired"));
        ownership["held_at_last_check"]=false;ownership["fault_latched"]=true;result["ownership"]=ownership;
        QVERIFY(show(QJsonDocument(result).toJson(QJsonDocument::Compact),QString()));QVERIFY(row("Observation").contains("reconciliation"));
        QVERIFY(show({},"Query unavailable"));QCOMPARE(row("Observation"),QString("Query unavailable"));QVERIFY(row("Reserved target addresses").isEmpty());
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Q_ARG(QString,QString("Disconnected")),Q_ARG(bool,false),Q_ARG(bool,false),Q_ARG(bool,false)));
        QVERIFY(show(body,QString()));QCOMPARE(row("Observation"),QString("Native ECC is disconnected"));QVERIFY(row("Reserved target addresses").isEmpty());
    }
    void failedDataHandoffCompletionIsReachableButInitiallyReadOnly()
    {
        MainWindow window;
        auto action=window.findChild<QAction *>("NativeFailedHandoffCompletion");QVERIFY(action && action->isEnabled());
        action->trigger();
        auto dialog=window.findChild<FailedHandoffWindow *>("NativeFailedHandoffCompletionWindow");QVERIFY(dialog);
        QVERIFY(!dialog->findChild<QPushButton *>("submit")->isEnabled());
        QVERIFY(!dialog->findChild<QPushButton *>("observe")->isEnabled());
        QVERIFY(dialog->findChild<QPlainTextEdit *>("requestView")->isReadOnly());
        QVERIFY(dialog->findChild<QPlainTextEdit *>("resultView")->isReadOnly());
        QVERIFY(dialog->findChild<QLabel *>("instructions")->text().contains("never retries"));
        dialog->close();window.close();
    }
    void failedDataHandoffDisablesOperationsWhenNativeQueryHelperIsMissing()
    {
        const QString absent=QDir::tempPath()+QStringLiteral("/getdaq-supervisor-query-intentionally-absent");
        FailedHandoffWindow dialog(nullptr,absent);
        QVERIFY(!dialog.findChild<QPushButton *>("generate")->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>("submit")->isEnabled());
        QVERIFY(!dialog.findChild<QPushButton *>("observe")->isEnabled());
        QVERIFY(!dialog.findChild<QLabel *>("queryAvailability")->isHidden());
        QVERIFY(dialog.findChild<QLabel *>("queryAvailability")->text().contains("getdaq-supervisor-query"));
    }
    void nativeConfigurationViewSeparatesSelectionAndHardwareProof()
    {
        MainWindow window;RunConfig config=RunConfig::Default();config.nativeEcc=true;config.simulation=false;
        config.nativeClient.snapshotSha256=QString(64,'b');config.nativeClient.inputBindingSha256=QString(64,'c');window.SetConfig(config);
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Q_ARG(QString,QString("Native monitoring")),Q_ARG(bool,true),Q_ARG(bool,false),Q_ARG(bool,false)));
        auto action=window.findChild<QAction *>("NativeConfigurationInfo");QVERIFY(action && action->isVisible() && action->isEnabled());action->trigger();
        QTRY_VERIFY(window.findChild<QTableWidget *>("NativeConfigurationTable"));QTest::qWait(50);
        GetDAQ::EccControlConfiguration c;c.instanceId=std::string(64,'a');c.snapshotSha256=std::string(64,'b');c.inputBindingSha256=std::string(64,'c');
        c.configurationSha256=std::string(64,'1');c.artifactSha256=std::string(64,'2');c.hasSelection=true;c.selection.registrationSha256=std::string(64,'3');
        auto &r=c.selection.registration;r.repositoryId=std::string(64,'4');r.publicationSha256=r.effectiveSha256=r.snapshotSha256=std::string(64,'5');
        r.configurationSha256=c.configurationSha256;r.artifactSha256=c.artifactSha256;r.manifestFile="/repository/version/publication.json";
        const auto body=QByteArray::fromStdString(GetDAQ::EncodeEccControlConfiguration(c));
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowConfigurationSlot",Q_ARG(QByteArray,body),Q_ARG(QString,QString())));
        const auto table=window.findChild<QTableWidget *>("NativeConfigurationTable");QVERIFY(table->editTriggers()==QAbstractItemView::NoEditTriggers);
        QMap<QString,QString> rows;for (int i=0;i<table->rowCount();++i) rows[table->item(i,0)->text()]=table->item(i,1)->text();
        QCOMPARE(rows.value("Repository selection"),QString("Accepted at startup"));QCOMPARE(rows.value("Hardware loading"),QString("Not verified"));
        QVERIFY(rows.value("Live file health").contains("Not checked"));QCOMPARE(rows.value("Manifest on ECC host"),QString::fromStdString(r.manifestFile));
        QVERIFY(window.findChild<QLabel *>("NativeConfigurationSummary")->text().contains("hardware load unverified"));
        QVERIFY(QMetaObject::invokeMethod(&window,"ShowControlSlot",Q_ARG(QString,QString("Disconnected")),Q_ARG(bool,false),Q_ARG(bool,false),Q_ARG(bool,false)));
        QVERIFY(window.findChild<QLabel *>("NativeConfigurationSummary")->text().contains("not observed"));QCOMPARE(table->rowCount(),1);window.close();
    }
    void pausedHardwareCanBeStopped()
    {
        GetDAQTestInternal::RunPausedHardwareCanBeStopped();
    }
    void nativeReceiverArchiveGuard()
    {
#ifdef Q_OS_UNIX
        QTemporaryDir dir; SentryConfig c; c.dataDir = dir.path(); c.checkProcess = false;
        QFile marker(dir.filePath(".getdaq-native-receiver")); QVERIFY(marker.open(QIODevice::WriteOnly)); marker.close();
        QFile raw(dir.filePath("CoBo0.graw")); QVERIFY(raw.open(QIODevice::WriteOnly)); raw.write("abcd"); raw.close();
        QCOMPARE(SentryFiles::Catalog(c, "Exp", 1).code, 409); // 进程死了也不能把没有凭据的数据当完整数据。
        QFile seal(dir.filePath(".getdaq-receiver-seal.json")); QVERIFY(seal.open(QIODevice::WriteOnly));
        seal.write(R"({"schema":1,"complete":true,"durable":true,"received_bytes":"4","files":[{"name":"CoBo0.graw","bytes":"4"}]})"); seal.close();
        {
            GetDAQ::DirectoryLease receiver(dir.path().toStdString());
            QCOMPARE(SentryFiles::Catalog(c, "Exp", 1).code, 409); // 即使凭据在，也不能移动活进程文件。
        }
        QVERIFY(raw.open(QIODevice::Append)); raw.write("e"); raw.close();
        QCOMPARE(SentryFiles::Catalog(c, "Exp", 1).code, 409);
        QVERIFY(raw.open(QIODevice::WriteOnly)); QVERIFY(raw.resize(4)); raw.close();
        QCOMPARE(SentryFiles::Catalog(c, "Exp", 1).code, 200);
        QVERIFY(QFileInfo::exists(dir.filePath("Exp/run_0001/receiver-seal.json")));
        QVERIFY(!QFileInfo::exists(seal.fileName()));
#else
        // Qt 5.14's variadic QSKIP macro needs a GNU empty-argument extension.
        QTest::qSkip("Native receiver directory locks are local Linux/macOS operations.",__FILE__,__LINE__);
        return;
#endif
    }
    void soapStringParametersAndTopology()
    {
        GetDAQTestInternal::RunSoapStringParametersAndTopology();
    }
#ifdef GETDAQ_GSOAP_INTEROP
    void legacyGsoapProcessInterop()
    {
        GetDAQTestInternal::RunLegacyGsoapProcessInterop();
    }
#endif
    void singleCoboLifecycle()
    {
        QTemporaryDir dir; RunConfig c = Config(dir.path()); c.nodes.removeLast();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(At(status, 1));
        for (int target : {2,3,4}) { control.ProgressSlot(); QTRY_VERIFY(At(status, target)); QTRY_COMPARE(Phase(phase), QString("Monitoring")); }
        control.StartRunSlot(); QTRY_COMPARE(Phase(phase), QString("Running"));
        control.StopRunSlot(); QTRY_COMPARE(done.count(), 1); control.ShutdownSlot();
    }
    void legacySelectedNodeProgressAndRegressRemainIndependent()
    {
        GetDAQTestInternal::RunLegacySelectedNodeProgressAndRegressRemainIndependent();
    }
    void readyDuringTransitionCannotStart()
    {
        GetDAQTestInternal::RunReadyDuringTransitionCannotStart();
    }
    void initTestCase()
    {
        qRegisterMetaType<RunConfig>(); qRegisterMetaType<NodeStatus>(); qRegisterMetaType<QVector<NodeStatus> >();
        QStandardPaths::setTestModeEnabled(true);
    }
    void yamlCompatibility()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("old.yml");
        QFile f(path); QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("experiment: Test\nrun_number: 42\ndescription: |\n  first line\n  second line\nfields:\n  'Beam: name': '\xE4\xB8\xAD\xE6\x96\x87'\n  Energy: '12.5'\n"); f.close();
        RunConfig c; QString error;
        QVERIFY2(c.Load(path, &error), qPrintable(error));
        QCOMPARE(c.runNumber, 42); QVERIFY(c.simulation);
        QCOMPARE(c.description, QString("first line\nsecond line\n"));
        QCOMPARE(c.fields.value("Beam: name"), QString::fromUtf8("中文"));
        QVERIFY(c.Save(dir.filePath("new.yml"), &error));
        RunConfig loaded; QVERIFY(loaded.Load(dir.filePath("new.yml"), &error));
        QCOMPARE(loaded.fields, c.fields); QCOMPARE(loaded.description, c.description);
        const QString original = c.experiment;
        f.open(QIODevice::WriteOnly | QIODevice::Truncate); f.write("experiment: ../escape\nrun_number: 0\n"); f.close();
        QVERIFY(!c.Load(path, &error)); QCOMPARE(c.experiment, original);
    }
    void protocolValidation()
    {
        GetDAQTestInternal::RunProtocolValidation();
    }
    void legacySoapHttpRequestSendsQuotedEmptySoapAction()
    {
        GetDAQTestInternal::RunLegacySoapHttpRequestSendsQuotedEmptySoapAction();
    }
    void soapStringAndInlineXmlParameterForms()
    {
        GetDAQTestInternal::RunSoapStringAndInlineXmlParameterForms();
    }
    void soapNamespacedResponseEnvelopes()
    {
        GetDAQTestInternal::RunSoapNamespacedResponseEnvelopes();
    }
    void simulatedLifecycle()
    {
        QTemporaryDir dir; RunConfig c = Config(dir.path());
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(At(status, 1));
        for (int target : {2, 3, 4}) { control.ProgressSlot(); QTRY_VERIFY(At(status, target)); QTRY_COMPARE(Phase(phase), QString("Monitoring")); }
        control.StartRunSlot(); QTRY_COMPARE(Phase(phase), QString("Running"));
        // Starting acquisition reserves the selected Run, but the GUI's next
        // Run number must not advance until the stopped data has been
        // cataloged and the local completion journal is durable.
        QCOMPARE(done.count(), 0);
        QTest::qWait(200);
        control.StopRunSlot(); QTRY_COMPARE(done.count(), 1);
        QCOMPARE(done.last().at(0).toInt(), 1);
        QVERIFY(QFileInfo::exists(c.RunDirectory() + "/run.csv"));
        QJsonObject journal; QString error;
        QVERIFY(RunRecorder::ReadJournal(c, &journal, &error)); QCOMPARE(journal.value("phase").toString(), QString("Complete"));
        QVERIFY(!RunRecorder::Reserve(c, &error));
        control.StartRunSlot(); QTRY_COMPARE(Phase(phase), QString("Running"));
        QCOMPARE(done.count(), 1);
        control.StopRunSlot(); QTRY_COMPARE(done.count(), 2);
        QCOMPARE(done.last().at(0).toInt(), 2);
        control.DisconnectSlot(); QCOMPARE(Phase(phase), QString("Disconnected"));
        control.ShutdownSlot();
    }
    void runReservationFailureDoesNotPublishPartialDirectory()
    {
        GetDAQTestInternal::RunRunReservationFailureDoesNotPublishPartialDirectory();
    }
    void realHttpOrdering()
    {
        GetDAQTestInternal::RunRealHttpOrdering();
    }
    void failedStartNeverStartsMutant()
    {
        GetDAQTestInternal::RunFailedStartNeverStartsMutant();
    }
    void stateConfirmationDeadline()
    {
        GetDAQTestInternal::RunStateConfirmationDeadline();
    }
    void catalogTimeoutNotRepeated()
    {
        GetDAQTestInternal::RunCatalogTimeoutNotRepeated();
    }
    void stopWaitsForMutantState()
    {
        GetDAQTestInternal::RunStopWaitsForMutantState();
    }
    void runRecorderExplicitReplacementRetainsOldRunUntilFinalization()
    {
        GetDAQTestInternal::RunRunRecorderExplicitReplacementRetainsOldRunUntilFinalization();
    }
    void runRecorderWritesNativeReferencesWithOptionalCatalogMirror()
    {
        GetDAQTestInternal::RunRunRecorderWritesNativeReferencesWithOptionalCatalogMirror();
    }
    void runRecorderCompletesNativeRunInDurableOrder()
    {
        GetDAQTestInternal::RunRunRecorderCompletesNativeRunInDurableOrder();
    }
    void runRecorderReplacementRejectsIncompleteOrMismatchedRun()
    {
        GetDAQTestInternal::RunRunRecorderReplacementRejectsIncompleteOrMismatchedRun();
    }
    void interruptedRecovery()
    {
        GetDAQTestInternal::RunInterruptedRecovery();
    }
    void backupRetryDoesNotRecatalog()
    {
        GetDAQTestInternal::RunBackupRetryDoesNotRecatalog();
    }
    void runReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement()
    {
        GetDAQTestInternal::RunRunReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement();
    }
    void nativeStatusProjectionRequiresMatchingLiveReceiverEvidence()
    {
        GetDAQTestInternal::RunNativeStatusProjectionRequiresMatchingLiveReceiverEvidence();
    }

    void nativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates()
    {
        GetDAQTestInternal::RunNativeSystemStatusRowsPreserveReceiverOwnershipAndStateGates();
    }

    void nativeStatusObservationCacheOwnsDecodeFreshnessAndReset()
    {
        GetDAQTestInternal::RunNativeStatusObservationCacheOwnsDecodeFreshnessAndReset();
    }

    void nativeGuiMetadataAndWholeSystemControls()
    {
        GetDAQTestInternal::RunNativeGuiMetadataAndWholeSystemControls();
    }
    void nativeSingleCoBoLifecycleUsesWholeEccTransitions()
    {
        GetDAQTestInternal::RunNativeSingleCoBoLifecycleUsesWholeEccTransitions();
    }
    void scanEventBaselineRequiresConnectedNativeController()
    {
        NativeRunControl control;
        QSignalSpy result(&control,&NativeRunControl::ScanEventCountBaselineBoundSignal);
        control.QueryScanEventCountBaseline(QStringLiteral("must-not-be-created.json"));
        QCOMPARE(result.size(),1);
        const QList<QVariant> arguments=result.takeFirst();
        QVERIFY(arguments.at(0).toByteArray().isEmpty());
        QVERIFY(arguments.at(1).toByteArray().isEmpty());
        QVERIFY(arguments.at(2).toString().contains(QStringLiteral("idle connected native controller")));
    }
    void scanRunStartRequiresConnectedNativeController()
    {
        NativeRunControl control;
        QSignalSpy result(&control,&NativeRunControl::ScanPointRunStatusSignal);
        control.StartScanPoint(QStringLiteral("must-not-be-created.json"));
        QCOMPARE(result.size(),1);
        const QList<QVariant> arguments=result.takeFirst();
        QVERIFY(arguments.at(0).toString().contains(QStringLiteral("validated event policy")));
        QVERIFY(!arguments.at(1).toBool());
        QVERIFY(!QFileInfo::exists(QStringLiteral("must-not-be-created.json")));
    }
    void nativeScanRunFinishesReceiverAndCatalogAfterEventTarget()
    {
        GetDAQTestInternal::RunNativeScanRunFinishesReceiverAndCatalogAfterEventTarget();
    }
    void configurationWorkspaceCopyReviewOpensIsolatedSources()
    {
        QTemporaryDir directory;QFile main(directory.filePath("main.xcfg")),shared(directory.filePath("shared.xcfg"));
        const QByteArray original="<Setup><Board id=\"0\" include=\"shared.xcfg\"/><Board id=\"1\" include=\"shared.xcfg\"/></Setup>",external="<Template><n>2</n></Template>";
        QVERIFY(main.open(QIODevice::WriteOnly));main.write(original);main.close();QVERIFY(shared.open(QIODevice::WriteOnly));shared.write(external);shared.close();
        QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(main.fileName(),&error));QTest::mouseClick(view.findChild<QPushButton *>("ConfigWorkspaceCopy"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigWorkspaceCopyDialog");QVERIFY(dialog);
        auto mode=dialog->findChild<QComboBox *>("ConfigWorkspaceCopyMode");QCOMPARE(mode->currentIndex(),1);
        auto target=dialog->findChild<QLineEdit *>("ConfigWorkspaceCopyDirectory");const auto output=directory.filePath("isolated");target->setText(output);
        auto preview=dialog->findChild<QPushButton *>("ConfigWorkspaceCopyPreview"),save=dialog->findChild<QPushButton *>("ConfigWorkspaceCopySave");
        QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY2(save->isEnabled(),qPrintable(dialog->findChild<QLabel *>("ConfigWorkspaceCopyStatus")->text()));
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigWorkspaceCopyFiles")->topLevelItemCount(),3);QVERIFY(!QFileInfo::exists(output));
        mode->setCurrentIndex(0);QVERIFY(!save->isEnabled());QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigWorkspaceCopyFiles")->topLevelItemCount(),2);
        mode->setCurrentIndex(1);QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());dialog->findChild<QCheckBox *>("ConfigWorkspaceCopyOpenSaved")->setChecked(true);
        QTest::mouseClick(save,Qt::LeftButton);QTRY_VERIFY2(dialog->findChild<QLabel *>("ConfigWorkspaceCopyStatus")->text().contains("Opened copied"),qPrintable(dialog->findChild<QLabel *>("ConfigWorkspaceCopyStatus")->text()));
        QCOMPARE(view.findChild<QTreeWidget *>("ConfigTree")->topLevelItemCount(),3);QVERIFY(QFileInfo::exists(output+"/workspace.json"));QVERIFY(!QFileInfo::exists(output+"/workspace.incomplete"));
        QVERIFY(!view.findChild<QPushButton *>("ConfigUndo")->isEnabled());QVERIFY(main.open(QIODevice::ReadOnly));QCOMPARE(main.readAll(),original);main.close();QVERIFY(shared.open(QIODevice::ReadOnly));QCOMPARE(shared.readAll(),external);
    }
    void configurationReadonlyWorkspaceCopyRejectsStaleReview()
    {
        QTemporaryDir directory;QFile file(directory.filePath("main.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup><n>2</n></Setup>");file.close();
        QString error;ConfigWindow view(directory.path(),true);QVERIFY(view.Open(file.fileName(),&error));QTest::mouseClick(view.findChild<QPushButton *>("ConfigWorkspaceCopy"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigWorkspaceCopyDialog");QVERIFY(dialog);QVERIFY(!dialog->findChild<QCheckBox *>("ConfigWorkspaceCopyOpenSaved")->isEnabled());
        const auto target=directory.filePath("copy");dialog->findChild<QLineEdit *>("ConfigWorkspaceCopyDirectory")->setText(target);
        auto preview=dialog->findChild<QPushButton *>("ConfigWorkspaceCopyPreview"),save=dialog->findChild<QPushButton *>("ConfigWorkspaceCopySave");QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());
        QVERIFY(view.Open(file.fileName(),&error));QTest::mouseClick(save,Qt::LeftButton);QVERIFY(!QFileInfo::exists(target));QVERIFY(dialog->findChild<QLabel *>("ConfigWorkspaceCopyStatus")->text().contains("Configuration changed"));
        QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());QTest::mouseClick(save,Qt::LeftButton);QTRY_VERIFY(dialog->findChild<QLabel *>("ConfigWorkspaceCopyStatus")->text().contains("Created editable"));
        QCOMPARE(view.findChild<QTreeWidget *>("ConfigTree")->topLevelItem(0)->text(0),QFileInfo(file.fileName()).canonicalFilePath());
    }
    void configurationGuiShallowObjectOverrideHasReviewAndUndo()
    {
        QTemporaryDir directory;const auto input=directory.filePath("objects.xcfg");QFile file(input);
        const QByteArray original="<Setup><Board id=\"*\"><Module id=\"*\"><gain>2</gain></Module><Module id=\"1\"/><Module id=\"2\"/></Board><Board id=\"0\"/></Setup>";
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(input,&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");tree->setCurrentItem(tree->topLevelItem(0)->child(0));
        view.findChild<QLineEdit *>("ConfigLookupPath")->setText("Setup/Board[0]/Module[*]");
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigStructureOpen"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigStructureDialog");QVERIFY(dialog);
        dialog->findChild<QComboBox *>("ConfigStructureOperation")->setCurrentIndex(13);
        auto objectPath=dialog->findChild<QLineEdit *>("ConfigStructureObjectPath");QVERIFY(objectPath->isEnabled());
        QCOMPARE(objectPath->text(),QString("Setup/Board[0]/Module[*]"));
        auto preview=dialog->findChild<QPushButton *>("ConfigStructurePreview"),apply=dialog->findChild<QPushButton *>("ConfigStructureApply");
        QTest::mouseClick(preview,Qt::LeftButton);QVERIFY2(apply->isEnabled(),qPrintable(dialog->findChild<QLabel *>("ConfigStructureStatus")->text()));
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigStructureCreatedObjects")->topLevelItemCount(),3);
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigStructureLocalizedObjects")->topLevelItemCount(),2);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        objectPath->setText("Setup/Board[0]/Module[1]");QVERIFY(!apply->isEnabled());QTest::mouseClick(preview,Qt::LeftButton);QVERIFY(apply->isEnabled());
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigStructureCreatedObjects")->topLevelItemCount(),1);
        QTest::mouseClick(apply,Qt::LeftButton);QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        XcfgDocument copy;QVERIFY(copy.Load(input,&error));const int own=copy.Lookup("Setup/Board[0]/Module[1]",&error);
        QCOMPARE(copy.Nodes()[own].path,QString("Setup/Board[0]/Module[1]"));QVERIFY(copy.Nodes()[own].children.isEmpty());
        auto undo=view.findChild<QPushButton *>("ConfigUndo");QVERIFY(undo->isEnabled());QTest::mouseClick(undo,Qt::LeftButton);
        auto history=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(history);QTest::mouseClick(history->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
        ConfigWindow readonly(directory.path(),true);QVERIFY(readonly.Open(input,&error));QVERIFY(!readonly.findChild<QPushButton *>("ConfigStructureOpen")->isEnabled());
    }
    void configurationGuiPreservesExactStringSpaces()
    {
        QTemporaryDir directory;QFile file(directory.filePath("spaces.xcfg"));QVERIFY(file.open(QIODevice::WriteOnly));
        const QByteArray original="<Setup><label type=\"string\">old</label></Setup>";file.write(original);file.close();
        QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(file.fileName(),&error));
        auto tree=view.findChild<QTreeWidget *>("ConfigTree");QVERIFY(tree);tree->setCurrentItem(tree->topLevelItem(0)->child(0)->child(0));
        const QString intended="  a   b  ";view.findChild<QLineEdit *>("ConfigValue")->setText(intended);
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigSaveValue"),Qt::LeftButton);
        XcfgDocument loaded;QVERIFY2(loaded.Load(file.fileName(),&error),qPrintable(error));
        QCOMPARE(loaded.Nodes()[loaded.Lookup("Setup/label",&error)].value,intended);
        QVERIFY(file.open(QIODevice::ReadOnly));QVERIFY(file.readAll().contains("&#32;&#32;a&#32;&#32;&#32;b&#32;&#32;"));file.close();
        auto undo=view.findChild<QPushButton *>("ConfigUndo");QVERIFY(undo->isEnabled());QTest::mouseClick(undo,Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigHistoryDialog");QVERIFY(dialog);
        QTest::mouseClick(dialog->findChild<QPushButton *>("ConfigHistoryApply"),Qt::LeftButton);
        QCoreApplication::sendPostedEvents(nullptr,QEvent::DeferredDelete);
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
    }
    void configurationXmlExportReviewAndSaveAs()
    {
        QTemporaryDir directory;const auto input=directory.filePath("input.xcfg"),output=directory.filePath("output.xcfg");QFile file(input);
        const QByteArray original="<Setup><Board id=\"*\"><label type=\"string\">&#32;&#32;a&#32;&#32;b&#32;</label></Board><Board id=\"0\"/></Setup>";
        QVERIFY(file.open(QIODevice::WriteOnly));file.write(original);file.close();QString error;ConfigWindow view(directory.path(),false);QVERIFY(view.Open(input,&error));
        view.findChild<QLineEdit *>("ConfigLookupPath")->setText("Setup/Board[0]");
        QTest::mouseClick(view.findChild<QPushButton *>("ConfigExportXml"),Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigXmlExportDialog");QVERIFY(dialog);
        dialog->findChild<QComboBox *>("ConfigXmlExportMode")->setCurrentIndex(1);
        dialog->findChild<QLineEdit *>("ConfigXmlExportDestination")->setText(output);
        auto preview=dialog->findChild<QPushButton *>("ConfigXmlExportPreview"),save=dialog->findChild<QPushButton *>("ConfigXmlExportSave");
        QVERIFY(!save->isEnabled());QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());QVERIFY(!QFileInfo::exists(output));
        QVERIFY(dialog->findChild<QPlainTextEdit *>("ConfigXmlExportText")->toPlainText().contains("&#32;&#32;a&#32;&#32;b&#32;"));
        QCOMPARE(dialog->findChild<QTreeWidget *>("ConfigXmlExportSources")->topLevelItemCount(),1);
        dialog->findChild<QLineEdit *>("ConfigXmlExportDestination")->setText(directory.filePath("changed.xcfg"));QVERIFY(!save->isEnabled());
        dialog->findChild<QLineEdit *>("ConfigXmlExportDestination")->setText(output);QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());
        dialog->findChild<QCheckBox *>("ConfigXmlExportOpenSaved")->setChecked(true);QTest::mouseClick(save,Qt::LeftButton);
        QTRY_VERIFY(dialog->findChild<QLabel *>("ConfigXmlExportStatus")->text().contains("Opened the new copy"));
        XcfgDocument copy;QVERIFY(copy.Load(output,&error));QCOMPARE(copy.Nodes()[copy.Lookup("Board[0]/label",&error)].value,QString("  a  b "));
        QVERIFY(!view.findChild<QPushButton *>("ConfigUndo")->isEnabled());QVERIFY(!view.findChild<QPushButton *>("ConfigRedo")->isEnabled());
        QCOMPARE(view.findChild<QTreeWidget *>("ConfigTree")->topLevelItem(0)->text(0),QFileInfo(output).canonicalFilePath());
        QVERIFY(file.open(QIODevice::ReadOnly));QCOMPARE(file.readAll(),original);file.close();
    }
    void configurationXmlReadOnlyExportRejectsStaleWindow()
    {
        QTemporaryDir directory;const auto input=directory.filePath("input.xcfg"),output=directory.filePath("output.xcfg");QFile file(input);
        QVERIFY(file.open(QIODevice::WriteOnly));file.write("<Setup><n>2</n></Setup>");file.close();QString error;ConfigWindow view(directory.path(),true);QVERIFY(view.Open(input,&error));
        auto button=view.findChild<QPushButton *>("ConfigExportXml");QVERIFY(button->isEnabled());QTest::mouseClick(button,Qt::LeftButton);
        auto dialog=view.findChild<QDialog *>("ConfigXmlExportDialog");QVERIFY(dialog);QVERIFY(!dialog->findChild<QCheckBox *>("ConfigXmlExportOpenSaved")->isEnabled());
        dialog->findChild<QLineEdit *>("ConfigXmlExportDestination")->setText(output);
        auto preview=dialog->findChild<QPushButton *>("ConfigXmlExportPreview"),save=dialog->findChild<QPushButton *>("ConfigXmlExportSave");
        QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());QVERIFY(view.Open(input,&error));QTest::mouseClick(save,Qt::LeftButton);
        QVERIFY(dialog->findChild<QLabel *>("ConfigXmlExportStatus")->text().contains("Configuration changed"));QVERIFY(!QFileInfo::exists(output));
        QTest::mouseClick(preview,Qt::LeftButton);QTRY_VERIFY(save->isEnabled());QTest::mouseClick(save,Qt::LeftButton);
        QTRY_VERIFY(dialog->findChild<QLabel *>("ConfigXmlExportStatus")->text().contains("Saved "));QVERIFY(QFileInfo::exists(output));
        QCOMPARE(view.findChild<QTreeWidget *>("ConfigTree")->topLevelItem(0)->text(0),QFileInfo(input).canonicalFilePath());
    }
    void eachConnectionGetsAnIndependentPersistentOperationLog()
    {
        GetDAQTestInternal::RunEachConnectionGetsAnIndependentPersistentOperationLog();
    }
    void visibleOperationLogCanBeExported()
    {
        GetDAQTestInternal::RunVisibleOperationLogCanBeExported();
    }
    void manualRunNumberRollbackRemainsAvailableUntilRunStarts()
    {
        GetDAQTestInternal::RunManualRunNumberRollbackRemainsAvailableUntilRunStarts();
    }
    void elapsedShowsHoursMinutesSeconds()
    {
        GetDAQTestInternal::RunElapsedShowsHoursMinutesSeconds();
    }
    void waveformToolBlocksRunUntilItsWindowIsClosed()
    {
        GetDAQTestInternal::RunWaveformToolBlocksRunUntilItsWindowIsClosed();
    }
    void externallyObservedRunClosesIntegratedWaveformViewer()
    {
        GetDAQTestInternal::RunExternallyObservedRunClosesIntegratedWaveformViewer();
    }
    void offlineDataToolsRunExistingProgramsAndPreserveInput()
    {
        GetDAQTestInternal::RunOfflineDataToolsRunExistingProgramsAndPreserveInput();
    }
    void offlineDataToolsMissingProgramsAndCloseDuringProcess_data()
    {
        GetDAQTestInternal::RunOfflineDataToolsMissingProgramsAndCloseDuringProcess_data();
    }
    void offlineDataToolsMissingProgramsAndCloseDuringProcess()
    {
        GetDAQTestInternal::RunOfflineDataToolsMissingProgramsAndCloseDuringProcess();
    }
    void offlineDataToolsAllCloseControls_data()
    {
        GetDAQTestInternal::RunOfflineDataToolsAllCloseControls_data();
    }
    void offlineDataToolsAllCloseControls()
    {
        GetDAQTestInternal::RunOfflineDataToolsAllCloseControls();
    }
    void offlineDataToolsMenuWindowClosesAndCanReopen_data()
    {
        GetDAQTestInternal::RunOfflineDataToolsMenuWindowClosesAndCanReopen_data();
    }
    void offlineDataToolsMenuWindowClosesAndCanReopen()
    {
        GetDAQTestInternal::RunOfflineDataToolsMenuWindowClosesAndCanReopen();
    }
    void offlineDataToolsClosePendingLocksMenuUntilProcessExits()
    {
        GetDAQTestInternal::RunOfflineDataToolsClosePendingLocksMenuUntilProcessExits();
    }
    void multipleOfflineWindowsKeepRunLockedUntilBothClose()
    {
        GetDAQTestInternal::RunMultipleOfflineWindowsKeepRunLockedUntilBothClose();
    }
    void automaticRunSettingsRoundTrip()
    {
        GetDAQTestInternal::RunAutomaticRunSettingsRoundTrip();
    }
    void automaticRunPolicyWaitsForCompleteAndReady()
    {
        GetDAQTestInternal::RunAutomaticRunPolicyWaitsForCompleteAndReady();
    }
    void automaticRunEitherThresholdAndGap()
    {
        GetDAQTestInternal::RunAutomaticRunEitherThresholdAndGap();
    }
    void automaticRunPolicyPauseAndFreshBytes()
    {
        GetDAQTestInternal::RunAutomaticRunPolicyPauseAndFreshBytes();
    }
    void automaticRunSimulationCyclesAndStops()
    {
        GetDAQTestInternal::RunAutomaticRunSimulationCyclesAndStops();
    }
    void automaticRunManualPauseResumeStop()
    {
        GetDAQTestInternal::RunAutomaticRunManualPauseResumeStop();
    }
    void automaticRunCollisionAndFinalizationFailure()
    {
        GetDAQTestInternal::RunAutomaticRunCollisionAndFinalizationFailure();
    }
    void eventLogColorsPreserveOriginalText()
    {
        GetDAQTestInternal::RunEventLogColorsPreserveOriginalText();
    }
    void automaticRunUiPlacementAndScreenshot()
    {
        GetDAQTestInternal::RunAutomaticRunUiPlacementAndScreenshot();
    }
    void automaticRunUiLocksSettingsThroughRunAndInterval()
    {
        GetDAQTestInternal::RunAutomaticRunUiLocksSettingsThroughRunAndInterval();
    }
    void consoleOverviewUsesConfiguredFreshCoBos()
    {
        GetDAQTestInternal::RunConsoleOverviewUsesConfiguredFreshCoBos();
    }
    void consoleReferenceLayoutAndLiveSimulation()
    {
        GetDAQTestInternal::RunConsoleReferenceLayoutAndLiveSimulation();
    }
    void compactConsoleLayoutKeepsParametersAndNodeRowsVisible()
    {
        GetDAQTestInternal::RunCompactConsoleLayoutKeepsParametersAndNodeRowsVisible();
    }
    void statusTableUsesOnlyConfiguredNodeHeight()
    {
        GetDAQTestInternal::RunStatusTableUsesOnlyConfiguredNodeHeight();
    }
    void consoleReflowsWhenConfiguredNodesShrink()
    {
        GetDAQTestInternal::RunConsoleReflowsWhenConfiguredNodesShrink();
    }
    void guiLifecycleAndClose()
    {
        QTemporaryDir dir;
        MainWindow window; window.SetConfig(Config(dir.path())); window.show();
        auto button = [&window](const char *name) { return window.findChild<QPushButton *>(name); };
        auto table = window.findChild<QTableWidget *>("StatusTable");
        auto phase = window.findChild<QLabel *>("Phase");
        auto run = window.findChild<QSpinBox *>("RunNumber");
        auto pause = window.findChild<QAction *>("NativePause");
        auto resume = window.findChild<QAction *>("NativeResume");
        QVERIFY(pause && resume && pause->isVisible() && resume->isVisible());
        button("Connect")->click(); QTRY_COMPARE(table->item(0, 1)->text(), QString("Idle"));
        run->setValue(5);
        window.findChild<QLineEdit *>("Description")->setText(QStringLiteral("edited after connect"));
        auto *fields=window.findChild<QTableWidget *>("Fields");QVERIFY(fields && fields->rowCount()>0);
        fields->item(0,1)->setText(QStringLiteral("updated after connect"));
        for (const QString &state : {QString("Described"), QString("Prepared"), QString("Ready")}) {
            button("Progress")->click(); QTRY_COMPARE(table->item(0, 1)->text(), state); QTRY_VERIFY(button("StartRun")->isEnabled());
        }
        button("StartRun")->click(); QTRY_COMPARE(phase->text(), QString("Running"));
        QVERIFY(pause->isEnabled()); pause->trigger(); QTRY_COMPARE(phase->text(), QString("Paused"));
        QTRY_COMPARE(table->item(0, 1)->text(), QString("Paused")); QVERIFY(resume->isEnabled());
        QTest::qWait(60); resume->trigger(); QTRY_COMPARE(phase->text(), QString("Running"));
        QTest::qWait(1000);
        window.grab().save(dir.filePath("running.png"));
        if (qEnvironmentVariableIsSet("GETDAQ_TEST_SCREENSHOT")) window.grab().save(qEnvironmentVariable("GETDAQ_TEST_SCREENSHOT"));
        button("StopRun")->click(); QTRY_COMPARE(run->value(), 6);
        QFile runRecord(QDir(dir.path()).filePath("simulation/Exp/run_0005/run.csv"));
        QVERIFY(runRecord.open(QIODevice::ReadOnly));
        const QByteArray runCsv=runRecord.readAll();
        QVERIFY(runCsv.contains("edited after connect"));
        QVERIFY(runCsv.contains("updated after connect"));
        window.close(); QTRY_VERIFY(!window.isVisible());
    }
    void sentryCatalogFiles()
    {
        GetDAQTestInternal::RunSentryCatalogFiles();
    }
    void sentryCatalogExplicitReplacementDefersOldDataCleanup()
    {
        GetDAQTestInternal::RunSentryCatalogExplicitReplacementDefersOldDataCleanup();
    }
    void sentryCatalogReplacementRejectsIncompleteOldRun()
    {
        GetDAQTestInternal::RunSentryCatalogReplacementRejectsIncompleteOldRun();
    }
    void sentryReplacementResumesAfterMergedManifestBeforeBackupCleanup()
    {
        GetDAQTestInternal::RunSentryReplacementResumesAfterMergedManifestBeforeBackupCleanup();
    }
    void sentryConfigStrictFields()
    {
        GetDAQTestInternal::RunSentryConfigStrictFields();
    }
    void sentryHttpEndpoints()
    {
        GetDAQTestInternal::RunSentryHttpEndpoints();
    }
    void httpClientCancelIsIdempotent()
    {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost, 0));
        HttpClient client;
        int callbacks = 0;
        const QUrl url(QString("http://127.0.0.1:%1/pending").arg(server.serverPort()));
        client.Request(url, {}, {}, 5000, [&callbacks](const QByteArray &, const QString &) { ++callbacks; });
        QTRY_VERIFY(server.hasPendingConnections());

        // Keep the peer open without sending a response so the request remains active.
        QScopedPointer<QTcpSocket> peer(server.nextPendingConnection());
        QVERIFY(peer);
        client.Cancel();
        client.Cancel();
        QTest::qWait(50);
        QCOMPARE(callbacks, 0);
    }
    void standaloneSimulatorIntegration()
    {
        // 让实际服务直接绑定系统分配的端口，避免预探测/释放后的竞争及未检查邻接端口。
        QTemporaryDir dir; Simulator simulator; QString error;
        QVERIFY2(simulator.Start(dir.path(), 1, 0, 0, -1, 30, 1024 * 1024, &error), qPrintable(error));
        RunConfig c; QVERIFY2(c.Load(dir.filePath("envoy.yml"), &error), qPrintable(error));
        QVERIFY(c.nodes[0].eccPort > 0 && c.nodes[1].eccPort > 0 && c.nodes[0].sentryPort > 0);
        QVERIFY(c.nodes[0].eccPort != c.nodes[1].eccPort && c.nodes[0].eccPort != c.nodes[0].sentryPort);
        c.pollMs = 60; c.timeoutMs = 1000; c.stepTimeoutMs = 4000;
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(At(status, 1));
        for (int target : {2, 3, 4}) { control.ProgressSlot(); QTRY_VERIFY(At(status, target)); QTRY_COMPARE(Phase(phase), QString("Monitoring")); }
        control.StartRunSlot(); QTRY_COMPARE(Phase(phase), QString("Running"));
        QTest::qWait(600);
        control.StopRunSlot(); QTRY_COMPARE(done.count(), 1);
        QFile file(dir.filePath("node0/incoming/Demo/run_0000/simulated.graw"));
        QVERIFY(file.open(QIODevice::ReadOnly)); QVERIFY(file.read(100).startsWith("GETDAQ SIMULATED BYTES"));
        QVERIFY(QFileInfo::exists(c.RunDirectory() + "/backup.complete"));
        control.ShutdownSlot();
    }
    void simulatorGeneratedDataBudgetIsHard()
    {
        QTemporaryDir dir; Simulator simulator; QString error;
        const quint64 budget = 128 * 1024;
        QVERIFY2(simulator.Start(dir.path(), 1, 0, 0, -1, 0, budget, &error), qPrintable(error));
        RunConfig c; QVERIFY2(c.Load(dir.filePath("envoy.yml"), &error), qPrintable(error));
        c.pollMs = 60; c.timeoutMs = 1000; c.stepTimeoutMs = 4000;
        RunControl control; QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        control.ConnectSlot(c); QTRY_VERIFY(At(status, 1));
        for (int target : {2, 3, 4}) { control.ProgressSlot(); QTRY_VERIFY(At(status, target)); QTRY_COMPARE(Phase(phase), QString("Monitoring")); }
        control.StartRunSlot(); QTRY_COMPARE(Phase(phase), QString("Running"));
        const QString path = dir.filePath("node0/incoming/simulated.graw");
        QTRY_COMPARE_WITH_TIMEOUT(QFileInfo(path).size(), qint64(budget), 5000);
        QTest::qWait(300);
        QCOMPARE(QFileInfo(path).size(), qint64(budget));
        control.StopRunSlot();
        control.ShutdownSlot();
        Simulator resumed; error.clear();
        QVERIFY(!resumed.Start(dir.path(), 1, 0, 0, -1, 0, 64 * 1024, &error));
        QVERIFY(error.contains("data budget"));
    }
};
QTEST_MAIN(TestGetDAQ)
#include "test_getdaq.moc"
