#include "runcontrol.h"
#include "runrecorder.h"
#include <QCoreApplication>
#include <QTimer>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    const bool expectCatalogFailure=argc==3 && QString::fromLocal8Bit(argv[2])=="--expect-catalog-failure";
    const bool expectAlarm=argc==3 && QString::fromLocal8Bit(argv[2])=="--expect-alarm";
    const bool expectReceiverFailure=argc==3 && QString::fromLocal8Bit(argv[2])=="--expect-receiver-failure";
    if (argc!=2 && !expectCatalogFailure && !expectAlarm && !expectReceiverFailure) { qCritical()<<"Pass reviewed synthetic native GUI config [--expect-catalog-failure|--expect-alarm]"; return 2; }
    RunConfig config; QString error;
    if (!config.Load(QString::fromLocal8Bit(argv[1]),&error) || !config.nativeEcc) { qCritical()<<"Invalid native GUI config"<<error; return 2; }
    RunControl control; bool connected=false,busy=true,active=false,scheduled=false,inspection=false,finishing=false; int state=-1,step=0; QTimer deadline;
    deadline.setSingleShot(true);
    QObject::connect(&deadline,&QTimer::timeout,&app,[&] { qCritical()<<"FAIL actual native GUI workflow deadline at step"<<step; control.ShutdownSlot(); app.exit(1); }); deadline.start(500000);
    std::function<void()> advance;
    advance=[&] {
        if (finishing || scheduled || !connected || busy) return;
        if (expectAlarm && step==5 && inspection && state==4 && active) {
            QJsonObject journal;
            if (!RunRecorder::ReadJournal(config,&journal,&error) || journal.value("phase").toString()=="Complete" || journal.value("native_alarm_cache").toString().isEmpty() ||
                QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv"))) { qCritical()<<"FAIL actual GUI alarm completed Run or lost original cache"; control.ShutdownSlot(); app.exit(1); return; }
            const auto alarm=GetDAQ::DecodeEccControlAlarmStatus(journal.value("native_alarm_cache").toString().toStdString());
            if (!alarm.latched || alarm.phase!="complete" || alarm.operationId.empty()) return;
            deadline.stop(); qInfo()<<"PASS actual Qt alarm is visible, original cache saved, incomplete Run retained, no number advance";
            control.ShutdownSlot(); app.exit(0); return;
        }
        if (expectReceiverFailure && step==5 && inspection && state==5 && active) {
            QJsonObject journal;
            if (!RunRecorder::ReadJournal(config,&journal,&error) || journal.value("phase").toString()=="Complete" ||
                QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv"))) {qCritical()<<"FAIL receiver failure completed Run";control.ShutdownSlot();app.exit(1);return;}
            const auto bytes=journal.value("native_receiver_failure_cache").toString().toUtf8();
            if (bytes.isEmpty()) return;
            const auto policy=GetDAQ::DecodeEccControlReceiverFailureStatus(bytes.toStdString());
            if (!policy.latched || policy.phase!="complete" || policy.failedIds.size()!=1 || !journal.value("duration_incomplete").toBool()) return;
            finishing=true;deadline.stop();qInfo()<<"PASS actual Qt receiver-failure evidence and stop result visible; Run incomplete, no number advance";
            control.ShutdownSlot();app.exit(0);return;
        }
        const bool failedCatalog=expectCatalogFailure && step==7 && inspection && state==3 && active;
        const bool ready=failedCatalog || (step==0 && state==1) || (step==1 && state==2) || (step==2 && state==3) || (step==3 && state==4) ||
            (step==4 && state==5 && active) || (step==5 && state==6 && active) || (step==6 && state==5 && active) ||
            (step==8 && state==3 && !active) || (step==9 && state==2 && !active) || (step==10 && state==1 && !active);
        if (!ready) return;
        scheduled=true; QTimer::singleShot(0,&app,[&] {
            scheduled=false;
            if (!connected || busy) return;
            const int action=step++;
            if ((expectAlarm || expectReceiverFailure) && action==4) {
                QFile marker(QDir(config.RunDirectory()).filePath(expectReceiverFailure ? "receiver-failure-probe-ready" : "alarm-probe-ready"));
                if (!marker.open(QIODevice::WriteOnly) || marker.write("GUI Running, waiting for external fixture alarm\n")<0) {
                    qCritical()<<"FAIL actual GUI alarm ready marker"; control.ShutdownSlot(); app.exit(1); return;
                }
                marker.close(); return;
            }
            if (action==7 && expectCatalogFailure) {
                QJsonObject journal;
                if (!RunRecorder::ReadJournal(config,&journal,&error) || journal.value("phase").toString()=="Complete" ||
                    QFileInfo::exists(QDir(config.RunDirectory()).filePath("run.csv")) || QFileInfo::exists(QDir(config.RunDirectory()).filePath("data-catalogs.json"))) {
                    qCritical()<<"FAIL catalog failure produced completed Run"; control.ShutdownSlot(); app.exit(1); return;
                }
                deadline.stop(); qInfo()<<"PASS actual Qt catalog failure retains incomplete Run and original intent; no retry/number advance";
                control.ShutdownSlot(); app.exit(0); return;
            }
            if (action<=2) control.ProgressSlot();
            else if (action==3) control.StartRunSlot();
            else if (action==4) control.PauseSlot();
            else if (action==5) control.ResumeSlot();
            else if (action==6) control.StopRunSlot();
            else if (action==8 || action==9) control.RegressSlot();
            else if (action==10) {
                deadline.stop(); QFile record(QDir(config.RunDirectory()).filePath("receiver-status.json"));
                if (!record.open(QIODevice::ReadOnly)) { qCritical()<<"FAIL actual GUI Run seal/session record missing"; control.ShutdownSlot(); app.exit(1); return; }
                const auto status=GetDAQ::DecodeEccControlSystemStatus(record.readAll().toStdString()); std::size_t sources=0;
                if (config.nativeClient.configurationStatusEnabled) {
                    QFile metadata(QDir(config.RunDirectory()).filePath("ecc-configuration.json"));
                    if (!metadata.open(QIODevice::ReadOnly)) {qCritical()<<"FAIL actual GUI startup configuration record missing";control.ShutdownSlot();app.exit(1);return;}
                    const auto configuration=GetDAQ::DecodeEccControlConfiguration(metadata.readAll().toStdString());
                    if (configuration.snapshotSha256!=config.nativeClient.snapshotSha256.toStdString() || configuration.inputBindingSha256!=config.nativeClient.inputBindingSha256.toStdString()) {
                        qCritical()<<"FAIL actual GUI startup configuration binding differs";control.ShutdownSlot();app.exit(1);return;
                    }
                }
                for (const auto &node:status.nodes) for (const auto &receiver:node.receivers) {
                    if (node.state!="stopped" || !receiver.reaped || receiver.exitCode!=0 || !receiver.sealVerified) { qCritical()<<"FAIL actual GUI dataset proof differs"; control.ShutdownSlot(); app.exit(1); return; } ++sources;
                }
                if (sources!=2) { qCritical()<<"FAIL actual GUI source count"; control.ShutdownSlot(); app.exit(1); return; }
                qInfo()<<"PASS actual Qt RunControl whole-system nine commands, pause/resume, receiver EOF/seal references and Run record; synthetic localhost only";
                control.DisconnectSlot(); control.ShutdownSlot(); app.exit(0);
            }
        });
    };
    QObject::connect(&control,&RunControl::StatusSignal,&app,[&](const QVector<NodeStatus> &rows) {
        if (rows.size()!=2) return;
        int observed=rows[0].state;
        for (const auto &row:rows) if (!row.eccOnline || row.state!=observed) return;
        state=observed; advance();
    });
    QObject::connect(&control,&RunControl::ControlSignal,&app,[&](const QString &phase,bool c,bool b,bool a) { connected=c; busy=b; active=a; inspection=phase=="Native requires inspection"; advance(); });
    QObject::connect(&control,&RunControl::RunFinishedSignal,&app,[&](int next) { if (step==7 && next==config.runNumber+1) { step=8; advance(); } });
    QObject::connect(&control,&RunControl::LogSignal,&app,[](const QString &message) { qInfo().noquote()<<message; });
    control.ConnectSlot(config); return app.exec();
}
