#include "legacy_runcontrol_tests_p.h"

#include <QtTest>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonObject>
#include <QStringList>
#include <QTemporaryDir>

#include "eccclient.h"
#include "getdaq_test_support_p.h"
#include "mocknode_fixture_p.h"
#include "runcontrol.h"
#include "runrecorder.h"

namespace GetDAQTestInternal {

using GetDAQTestFixtures::MockNode;

void RunPausedHardwareCanBeStopped()
    {
        QTemporaryDir dir; QStringList trace; MockNode node("CoBo", &trace);
        QVERIFY(node.isListening()); node.state = 6;
        RunConfig c = GetDAQTestSupport::Config(dir.path()); c.nodes.removeLast(); c.simulation = false;
        c.nodes[0].host = "127.0.0.1"; c.nodes[0].eccPort = node.serverPort(); c.nodes[0].sentryPort = node.serverPort();
        RunControl control; QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 6));
        control.StopRunSlot(); QTRY_VERIFY(GetDAQTestSupport::At(status, 4));
        QVERIFY(trace.contains("CoBo:Stop")); QCOMPARE(node.catalogCount, 0);
        control.ShutdownSlot();
    }

void RunLegacySelectedNodeProgressAndRegressRemainIndependent()
    {
        QStringList trace;
        MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir;
        RunConfig c=GetDAQTestSupport::Config(dir.path()); c.simulation=false; GetDAQTestSupport::MakeFiles(c);
        const int coboId=c.nodes.first().id, mutantId=c.nodes.last().id;
        for (NodeConfig &node : c.nodes) {
            node.host="127.0.0.1";
            node.eccPort=node.mutant ? mutant.serverPort() : cobo.serverPort();
            node.sentryPort=node.eccPort;
        }
        RunControl control;
        QSignalSpy status(&control,&RunControl::StatusSignal), logs(&control,&RunControl::LogSignal);
        const auto nodeAt=[&](int id,int state) {
            if(status.isEmpty()) return false;
            const auto rows=qvariant_cast<QVector<NodeStatus> >(status.last().at(0));
            for(const auto &row:rows)if(row.id==id)return row.eccOnline && row.transition==0 && row.state==state;
            return false;
        };
        control.ConnectSlot(c); QTRY_VERIFY(nodeAt(coboId,1)); QTRY_VERIFY(nodeAt(mutantId,1));

        control.ProgressSlot(coboId); QTRY_VERIFY(nodeAt(coboId,2)); QVERIFY(nodeAt(mutantId,1));
        control.ProgressSlot(mutantId); QTRY_VERIFY(nodeAt(mutantId,2)); QVERIFY(nodeAt(coboId,2));
        const int beforeBlockedPrepare=trace.size();
        control.ProgressSlot(coboId); QTest::qWait(80);
        QCOMPARE(trace.size(),beforeBlockedPrepare); QVERIFY(nodeAt(coboId,2)); QVERIFY(nodeAt(mutantId,2));
        QVERIFY(!logs.isEmpty()); QVERIFY(logs.last().at(0).toString().contains("MuTaNT/CoBo ordering"));

        control.ProgressSlot(mutantId); QTRY_VERIFY(nodeAt(mutantId,3)); QVERIFY(nodeAt(coboId,2));
        control.ProgressSlot(coboId); QTRY_VERIFY(nodeAt(coboId,3)); QVERIFY(nodeAt(mutantId,3));
        const int beforeEarlyMutantConfigure=trace.size();
        control.ProgressSlot(mutantId); QTest::qWait(80);
        QCOMPARE(trace.size(),beforeEarlyMutantConfigure); QVERIFY(nodeAt(coboId,3)); QVERIFY(nodeAt(mutantId,3));
        QVERIFY(logs.last().at(0).toString().contains("ordering"));
        control.ProgressSlot(coboId); QTRY_VERIFY(nodeAt(coboId,4)); QVERIFY(nodeAt(mutantId,3));
        control.ProgressSlot(mutantId); QTRY_VERIFY(nodeAt(mutantId,4)); QVERIFY(nodeAt(coboId,4));
        control.RegressSlot(coboId); QTRY_VERIFY(nodeAt(coboId,3)); QVERIFY(nodeAt(mutantId,4));
        control.RegressSlot(mutantId); QTRY_VERIFY(nodeAt(mutantId,3)); QVERIFY(nodeAt(coboId,3));
        control.RegressSlot(coboId); QTRY_VERIFY(nodeAt(coboId,2)); QVERIFY(nodeAt(mutantId,3));

        QCOMPARE(trace.count("cobo:Describe"),1);
        QCOMPARE(trace.count("mutant:Describe"),1);
        QCOMPARE(trace.count("mutant:Prepare"),1);
        QCOMPARE(trace.count("cobo:Prepare"),1);
        QCOMPARE(trace.count("cobo:Configure"),1);
        QCOMPARE(trace.count("mutant:Configure"),1);
        QCOMPARE(trace.count("cobo:Breakup"),1);
        QCOMPARE(trace.count("mutant:Breakup"),1);
        QCOMPARE(trace.count("cobo:Undo"),1);
        control.DisconnectSlot(); control.ShutdownSlot();
    }

void RunReadyDuringTransitionCannotStart()
    {
        QTemporaryDir dir; QStringList trace; MockNode node("CoBo", &trace);
        QVERIFY(node.isListening()); node.state = 4; node.transition = 3;
        RunConfig c = GetDAQTestSupport::Config(dir.path()); c.nodes.removeLast(); c.simulation = false;
        c.nodes[0].host = "127.0.0.1"; c.nodes[0].eccPort = node.serverPort(); c.nodes[0].sentryPort = node.serverPort();
        RunControl control; QSignalSpy status(&control, &RunControl::StatusSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4));
        control.StartRunSlot(); QTest::qWait(150);
        QVERIFY(!trace.contains("CoBo:Start")); QVERIFY(!QFileInfo::exists(c.RunDirectory()));
        control.ShutdownSlot();
    }

void RunLegacySoapHttpRequestSendsQuotedEmptySoapAction()
    {
        QTemporaryDir dir;
        QStringList trace;
        MockNode server("legacy-soap", &trace);
        QVERIFY(server.isListening());
        NodeConfig node = RunConfig::Default().nodes.first();
        node.host = "127.0.0.1";
        node.eccPort = server.serverPort();
        RunConfig config = GetDAQTestSupport::Config(dir.path());
        config.nodes = {node};
        config.simulation = false;

        EccClient client(node, config);
        QSignalSpy replies(&client, &EccClient::ReplySignal);
        QVERIFY(client.Send("GetState"));
        QTRY_COMPARE(replies.size(), 1);
        QCOMPARE(replies.first().at(0).toInt(), node.id);
        QCOMPARE(replies.first().at(1).toString(), QString("GetState"));
        QCOMPARE(replies.first().at(2).toInt(), 1);
        QVERIFY(replies.first().at(4).toString().isEmpty());
        QCOMPARE(server.soapActionHeaders, QStringList() << "\"\"");
    }

void RunRealHttpOrdering()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false; GetDAQTestSupport::MakeFiles(c);
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 1));
        for (int target : {2, 3, 4}) { control.ProgressSlot(); QTRY_VERIFY(GetDAQTestSupport::At(status, target)); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Monitoring")); }
        QVERIFY(trace.indexOf("mutant:Prepare") < trace.indexOf("cobo:Prepare"));
        QVERIFY(trace.indexOf("cobo:Configure") < trace.indexOf("mutant:Configure"));
        control.StartRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Running"));
        QVERIFY(trace.indexOf("cobo:Start") < trace.indexOf("mutant:Start"));
        control.PauseSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Paused")); QTRY_VERIFY(GetDAQTestSupport::At(status, 6));
        QVERIFY(trace.contains("cobo:Pause") && trace.contains("mutant:Pause"));
        control.ResumeSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Running")); QTRY_VERIFY(GetDAQTestSupport::At(status, 5));
        QVERIFY(trace.contains("cobo:Resume") && trace.contains("mutant:Resume"));
        control.StopRunSlot(); QTRY_COMPARE(done.count(), 1);
        QVERIFY(trace.indexOf("mutant:Stop") < trace.indexOf("cobo:Stop"));
        QVERIFY(trace.indexOf("cobo:Stop") < trace.indexOf("cobo:Catalog"));
        QCOMPARE(cobo.catalogCount, 1);
        QVERIFY(QFileInfo::exists(c.RunDirectory() + "/backup.complete"));
        control.DisconnectSlot(); control.ShutdownSlot();
    }

void RunFailedStartNeverStartsMutant()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        cobo.state = mutant.state = 4; cobo.failStart = true;
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); QTest::qWait(100);
        control.StartRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Error"));
        QVERIFY(!trace.contains("mutant:Start")); QCOMPARE(cobo.catalogCount, 0);
        control.ShutdownSlot();
    }

void RunStateConfirmationDeadline()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        cobo.state = mutant.state = 4; cobo.deferState = true;
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); QTest::qWait(100);
        control.StartRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Error"));
        QVERIFY(!trace.contains("mutant:Start")); control.ShutdownSlot();
    }

void RunCatalogTimeoutNotRepeated()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        cobo.state = mutant.state = 4; cobo.hangCatalog = true;
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); QTest::qWait(100);
        control.StartRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Running"));
        control.StopRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Error"));
        QCOMPARE(cobo.catalogCount, 1); QCOMPARE(done.count(), 0);
        QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); control.FinalizeSlot(); QTest::qWait(300);
        QCOMPARE(cobo.catalogCount, 1); QCOMPARE(done.count(), 0);
        control.ShutdownSlot();
    }

void RunStopWaitsForMutantState()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        cobo.state = mutant.state = 5; mutant.deferStop = true;
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 5));
        control.StopRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Error"));
        QVERIFY(trace.contains("mutant:Stop")); QVERIFY(!trace.contains("cobo:Stop"));
        QCOMPARE(cobo.catalogCount, 0); control.ShutdownSlot();
    }

void RunBackupRetryDoesNotRecatalog()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        cobo.state = mutant.state = 4;
        QTemporaryDir dir; RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = c.nodes[0].sentryPort = cobo.serverPort(); c.nodes[1].eccPort = mutant.serverPort();
        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal), done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); QTest::qWait(100);
        control.StartRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Running"));
        control.StopRunSlot(); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Error"));
        QCOMPARE(done.count(), 0); QCOMPARE(cobo.catalogCount, 1);
        GetDAQTestSupport::MakeFiles(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 4)); control.FinalizeSlot();
        QTRY_COMPARE(done.count(), 1); QCOMPARE(cobo.catalogCount, 1);
        control.ShutdownSlot();
    }

void RunRunReplacementWaitsForAllCatalogsBeforeCleanupAndIncrement()
    {
        QStringList trace; MockNode cobo("cobo", &trace), mutant("mutant", &trace);
        QVERIFY(cobo.isListening()); QVERIFY(mutant.isListening());
        QTemporaryDir dir; QVERIFY(dir.isValid());
        RunConfig c = GetDAQTestSupport::Config(dir.path()); c.simulation = false;
        c.nodes[0].host = c.nodes[1].host = "127.0.0.1";
        c.nodes[0].eccPort = cobo.serverPort(); c.nodes[0].sentryPort = cobo.serverPort();
        c.nodes[1].eccPort = mutant.serverPort();
        GetDAQTestSupport::MakeFiles(c);
        QString error; QVERIFY2(RunRecorder::Reserve(c, &error), qPrintable(error));
        QVERIFY(RunRecorder::Finish(c, 3.0).isEmpty());
        QFile oldMarker(c.RunDirectory() + "/old-local-data-marker"); QVERIFY(oldMarker.open(QIODevice::WriteOnly));
        QCOMPARE(oldMarker.write("old", 3), qint64(3)); oldMarker.close();
        QJsonObject oldJournal; QVERIFY(RunRecorder::ReadJournal(c, &oldJournal, &error));
        oldJournal["phase"] = "Complete"; QVERIFY(RunRecorder::WriteJournal(c, oldJournal, &error));

        RunControl control;
        QSignalSpy status(&control, &RunControl::StatusSignal), phase(&control, &RunControl::ControlSignal);
        QSignalSpy done(&control, &RunControl::RunFinishedSignal);
        control.ConnectSlot(c); QTRY_VERIFY(GetDAQTestSupport::At(status, 1));
        for (int target : {2, 3, 4}) {
            control.ProgressSlot(); QTRY_VERIFY(GetDAQTestSupport::At(status, target));
            QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Monitoring"));
        }
        control.StartRunSlot(true); QTRY_COMPARE(GetDAQTestSupport::Phase(phase), QString("Running"));
        QTest::qWait(80); control.StopRunSlot();
        QTRY_COMPARE_WITH_TIMEOUT(done.count(), 1, 5000);
        QCOMPARE(done.last().at(0).toInt(), 1);
        QCOMPARE(cobo.replacementCatalogCount, 1);
        QCOMPARE(cobo.replacementFinalizeCount, 1);
        QCOMPARE(cobo.replacementOperationId.isEmpty(), false);
        QVERIFY(trace.indexOf("cobo:Catalog") >= 0);
        QVERIFY(trace.indexOf("cobo:FinalizeReplacement") > trace.indexOf("cobo:Catalog"));
        QJsonObject replacementJournal; QVERIFY(RunRecorder::ReadJournal(c, &replacementJournal, &error));
        QCOMPARE(replacementJournal.value("phase").toString(), QString("Complete"));
        QVERIFY(replacementJournal.value("replacement_cleanup_complete").toBool());
        QVERIFY(!replacementJournal.contains("replacement_backup"));
        const QString parent = QFileInfo(c.RunDirectory()).absolutePath();
        const QStringList leftovers = QDir(parent).entryList({".run_0000.replaced-*"}, QDir::Dirs | QDir::Hidden);
        QVERIFY(leftovers.isEmpty());
        control.ShutdownSlot();
    }

} // namespace GetDAQTestInternal
