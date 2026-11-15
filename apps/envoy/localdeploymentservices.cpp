#include "localdeploymentservices.h"
#include "eccsystemclient.h"
#include "eccprotocol.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QTcpServer>
#include <QHostAddress>
#include <QSet>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <stdexcept>
namespace {
QJsonObject Json(const QString &path) {
    QFile file(path);if(!file.open(QIODevice::ReadOnly) || file.size()>1024*1024)throw std::runtime_error(("Cannot read service configuration: "+path).toStdString());
    QJsonParseError error;const auto doc=QJsonDocument::fromJson(file.readAll(),&error);
    if(error.error!=QJsonParseError::NoError || !doc.isObject())throw std::runtime_error(("Invalid service configuration: "+path).toStdString());
    return doc.object();
}
}
LocalDeploymentServices::LocalDeploymentServices(const RunConfig &config,const QString &programs,QObject *parent)
    :QObject(parent),fConfig(config),fPrograms(programs),fDirectory(QFileInfo(config.filePath).absolutePath()),fCheck(new QProcess(this)) {
    fTimeout.setSingleShot(true);
    connect(&fTimeout,&QTimer::timeout,this,[this]{if(fCheck->state()!=QProcess::NotRunning)fCheck->kill();Fail(tr("Local service check timed out. Processes and logs are retained."));});
    connect(fCheck,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,&LocalDeploymentServices::CheckFinished);
    connect(fCheck,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){if(error==QProcess::FailedToStart)Fail(fCheck->errorString());});
}
bool LocalDeploymentServices::IsLocalDeployment(const RunConfig &c) {
#ifdef Q_OS_LINUX
    return c.nativeEcc && c.nativeClient.address=="127.0.0.1" && !c.filePath.isEmpty() &&
        QFileInfo::exists(QFileInfo(c.filePath).absolutePath()+"/ecc.json") && QFileInfo::exists(QFileInfo(c.filePath).absolutePath()+"/supervisor.json");
#else
    Q_UNUSED(c);return false;
#endif
}
void LocalDeploymentServices::Load() {
    if(!IsLocalDeployment(fConfig))throw std::runtime_error("This configuration is not a local managed deployment");
    fEcc=Json(fDirectory+"/ecc.json");fSupervisor=Json(fDirectory+"/supervisor.json");
    if(fEcc["kind"]!="getdaq.ecc-service" || fSupervisor["kind"]!="getdaq.supervisor-service" ||
       fEcc["port"].toInt()!=fConfig.nativeClient.port || fEcc["bind"]!="127.0.0.1" || fSupervisor["bind"]!="127.0.0.1")
        throw std::runtime_error("Local service files do not match the selected console configuration");
}
void LocalDeploymentServices::ValidateStartupConfiguration(){Load();CheckLocalConflicts();}
void LocalDeploymentServices::CheckLocalConflicts() {
    using Endpoint=QPair<QString,int>;
    auto endpoints=[](const QJsonObject &object) {
        QVector<Endpoint> result;result.append(qMakePair(object["bind"].toString(),object["port"].toInt()));
        for(const auto &value:object["receiver_plan"].toObject()["sources"].toArray()) {
            const auto source=value.toObject();result.append(qMakePair(source["bind"].toString(),source["data_port"].toInt()));
            result.append(qMakePair(QString("127.0.0.1"),source["status_port"].toInt()));
        }
        for(const auto &value:object["cobo_policies"].toArray()) {
            const auto m=QRegularExpression("tcp -h ([0-9.]+) -p ([0-9]+)").match(value.toObject()["callback_endpoints"].toString());
            if(m.hasMatch())result.append(qMakePair(m.captured(1),m.captured(2).toInt()));
        }
        return result;
    };
    auto proxies=[](const QJsonObject &topology) {
        QSet<QString> result;for(const auto &key:{"cobos","mutants"})for(const auto &value:topology[key].toArray())result.insert(value.toObject()["node_proxy"].toString());return result;
    };
    const auto requestedBoards=proxies(Json(fEcc["topology"].toObject()["path"].toString()));
    const auto requested=endpoints(fEcc)+endpoints(fSupervisor);
    for(const auto &name:{QString("getdaq-ecc-server"),QString("getdaq-supervisor")})for(const auto &other:LocalServiceConfigurations(QDir(fPrograms).filePath(name))) {
        if(QFileInfo(other).canonicalFilePath()==QFileInfo(fDirectory+(name=="getdaq-ecc-server"?"/ecc.json":"/supervisor.json")).canonicalFilePath())continue;
        const auto object=Json(other);
        if(name=="getdaq-ecc-server") {
            auto overlap=proxies(Json(object["topology"].toObject()["path"].toString()));overlap.intersect(requestedBoards);
            if(!overlap.isEmpty())throw std::runtime_error(tr("Another ECC already controls the requested board: %1. Configuration: %2").arg(*overlap.begin(),other).toStdString());
        }
        for(const auto &used:endpoints(object))for(const auto &wanted:requested)if(wanted.second>0 && wanted.second==used.second &&
            (wanted.first==used.first || wanted.first=="0.0.0.0" || used.first=="0.0.0.0"))
                throw std::runtime_error(tr("Local port %1:%2 is assigned to another running instance: %3").arg(wanted.first).arg(wanted.second).arg(other).toStdString());
    }
}
void LocalDeploymentServices::Fail(const QString &error) {
    if(!fBusy)return;
    fBusy=false;fPhase.clear();fTimeout.stop();emit Failed(error);
}
void LocalDeploymentServices::EnsureStarted() {
    if(fBusy)return;
    fBusy=true;
    try {Load();if(QFileInfo::exists(fDirectory+"/readback-warning-policy.json"))emit Log(tr("Warning: operator-accepted legacy readDataDelay=255 readbacks remain unverified; only this scoped compatibility warning may continue."));CheckLocalConflicts();fIndex=0;NextService();}catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void LocalDeploymentServices::NextService() {
    if(!fBusy)return;
    if(fIndex>=2){fClock.start();VerifyIdentity();return;}
    const QString name=fIndex==0?"getdaq-supervisor":"getdaq-ecc-server";
    const QString config=fDirectory+(fIndex==0?"/supervisor.json":"/ecc.json");
    const QFileInfo binary(QDir(fPrograms).filePath(name));
    if(!binary.isFile() || binary.isSymLink() || !binary.isExecutable() || (binary.permissions()&(QFile::WriteGroup|QFile::WriteOther))) {
        Fail(tr("Missing or writable installed service executable: %1").arg(binary.filePath()));return;
    }
    try {CheckLocalConflicts();}catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));return;}
    const auto processes=FindLocalServiceProcesses(QDir(fPrograms).filePath(name),config);
    if(processes.size()>1){Fail(tr("Multiple %1 processes use this configuration. Inspect them in the service manager.").arg(name));return;}
    if(!processes.isEmpty()){emit Log(tr("Reusing %1, PID %2").arg(name).arg(processes[0].pid));++fIndex;NextService();return;}
    const auto object=fIndex==0?fSupervisor:fEcc;QTcpServer probe;
    if(!probe.listen(QHostAddress(object["bind"].toString()),quint16(object["port"].toInt()))) {Fail(tr("%1 port %2 is occupied by a different service. No process was replaced.").arg(name).arg(object["port"].toInt()));return;}
    probe.close();emit Log(tr("Checking %1 configuration…").arg(name));
    Check(name,{"--config",config,"--check-config"},"start-check");
}
void LocalDeploymentServices::Check(const QString &name,const QStringList &arguments,const QString &phase) {
    fPhase=phase;fCheck->setProgram(QDir(fPrograms).filePath(name));fCheck->setArguments(arguments);fCheck->start();fTimeout.start(30000);
}
void LocalDeploymentServices::CheckFinished(int code,QProcess::ExitStatus status) {
    if(!fBusy || fPhase.isEmpty())return;
    fTimeout.stop();const auto phase=fPhase;fPhase.clear();
    const auto diagnostic=QString::fromLocal8Bit(fCheck->readAllStandardError()).left(4000);fCheck->readAllStandardOutput();
    if(code || status!=QProcess::NormalExit) {
        if(phase=="node-identity" && fClock.elapsed()<15000){QTimer::singleShot(300,this,&LocalDeploymentServices::VerifyIdentity);return;}
        Fail(tr("Local service check failed: %1").arg(diagnostic));return;
    }
    if(phase=="start-check") {
        const QString name=fIndex==0?"getdaq-supervisor":"getdaq-ecc-server";
        const auto logPath=fDirectory+"/logs/"+name+".log";QDir().mkpath(fDirectory+"/logs");
        QProcess process;auto environment=QProcessEnvironment::systemEnvironment();environment.remove("GETDAQ_READBACK_WARNING_POLICY_FILE");
        if(name=="getdaq-ecc-server" && QFileInfo::exists(fDirectory+"/readback-warning-policy.json"))environment.insert("GETDAQ_READBACK_WARNING_POLICY_FILE",fDirectory+"/readback-warning-policy.json");
        process.setProcessEnvironment(environment);process.setProgram(QDir(fPrograms).filePath(name));process.setArguments({"--config",fDirectory+(fIndex==0?"/supervisor.json":"/ecc.json")});
        process.setWorkingDirectory(fDirectory);process.setStandardOutputFile(logPath,QIODevice::Append);process.setStandardErrorFile(logPath,QIODevice::Append);
        qint64 pid=0;if(!process.startDetached(&pid)){Fail(tr("Cannot start %1; see %2").arg(name,logPath));return;}
        emit Log(tr("Started %1, PID %2. Log: %3").arg(name).arg(pid).arg(logPath));++fIndex;NextService();return;
    }
    if(phase=="node-identity")VerifyEcc();
}
void LocalDeploymentServices::VerifyIdentity() {
    if(!fBusy)return;
    const auto endpoints=fEcc["remote_receivers"].toObject()["endpoints"].toArray();
    if(endpoints.size()!=1){Fail(tr("Automatic local startup requires one local data service; use the service manager for distributed data nodes."));return;}
    const auto e=endpoints[0].toObject();
    if(e["address"]!="127.0.0.1" || e["port"].toInt()!=fSupervisor["port"].toInt()){Fail(tr("ECC receiver endpoint does not match the local supervisor."));return;}
    Check("getdaq-supervisor-query",{"identity","--host","127.0.0.1","--port",QString::number(e["port"].toInt()),"--credential-file",e["credential_file"].toString(),
        "--expected-agent",e["agent_id"].toString(),"--controller",fEcc["remote_receivers"].toObject()["controller_id"].toString(),"--expected-plan",e["plan_sha256"].toString()},"node-identity");
}
void LocalDeploymentServices::VerifyEcc() {
    if(!fBusy)return;
    if(fClient){fClient->deleteLater();fClient=nullptr;}
    fClient=new EccSystemClient(fConfig.nativeClient,{},this);
    connect(fClient,&EccSystemClient::IdentitySignal,this,[this](bool matched,QByteArray,QString error){
        if(!fBusy)return;
        if(!matched) {
            if(fClock.elapsed()<15000){QTimer::singleShot(300,this,&LocalDeploymentServices::VerifyEcc);return;}
            Fail(tr("ECC identity could not be verified: %1").arg(error));return;
        }
        if(fPhase=="stop-identity") {
            fPhase="stop-status";fClient->QueryStatus();return;
        }
        fBusy=false;emit Log(tr("ECC and data service identities verified. Connecting…"));emit Ready();
    });
    connect(fClient,&EccSystemClient::StatusSignal,this,[this](QByteArray body,QString error){
        if(!fBusy || fPhase!="stop-status")return;
        if(!error.isEmpty()){Fail(error);return;}
        try {
            const auto s=GetDAQ::DecodeEccControlSystemStatus(body.toStdString());
            if(s.operationActive || s.controlNeedsReconciliation || s.dataNeedsReconciliation || s.controller.outcomeUnknown || s.controller.needsReconciliation || s.controller.state<1 || s.controller.state>3)
                throw std::runtime_error("ECC is not in a confirmed stopped/released state. Complete Run stop or release the configured session before stopping services; use explicit process termination only for repair.");
            if(!FindLocalReceiverProcesses(QDir(fPrograms).filePath("getdaq-receiver"),fDirectory+"/supervisor.json").isEmpty())
                throw std::runtime_error("Receiver processes are still running. Complete data finalization before stopping services.");
            StopExplicitly();
        }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
    });
    fClient->QueryIdentity();
}
void LocalDeploymentServices::StopSafely() {
    if(fBusy)return;
    fBusy=true;fPhase="stop-identity";
    try {
        Load();
        if(FindLocalServiceProcesses(QDir(fPrograms).filePath("getdaq-ecc-server"),fDirectory+"/ecc.json").isEmpty()) {
            if(!FindLocalReceiverProcesses(QDir(fPrograms).filePath("getdaq-receiver"),fDirectory+"/supervisor.json").isEmpty())throw std::runtime_error("ECC is unavailable and receivers are still running. Inspect the session before terminating processes.");
            StopExplicitly();return;
        }
        fPhase="stop-identity";fClock.start();VerifyEcc();
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void LocalDeploymentServices::StopExplicitly() {
    if(fBusy && fPhase!="stop-status" && fPhase!="stop-identity")return;
    fBusy=true;
    try {
        Load();fPhase="stopping";
        fStopping=FindLocalServiceProcesses(QDir(fPrograms).filePath("getdaq-ecc-server"),fDirectory+"/ecc.json");
        fStopping+=FindLocalReceiverProcesses(QDir(fPrograms).filePath("getdaq-receiver"),fDirectory+"/supervisor.json");
        fStopping+=FindLocalServiceProcesses(QDir(fPrograms).filePath("getdaq-supervisor"),fDirectory+"/supervisor.json");
        SignalProcesses();
    }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
}
void LocalDeploymentServices::SignalProcesses() {
    for(const auto &p:fStopping)if(SameLocalServiceProcess(p)) {
        emit Log(tr("Stopping %1, PID %2").arg(QFileInfo(p.executable).fileName()).arg(p.pid));
        if(QProcess::execute("/bin/kill",{"-TERM",QString::number(p.pid)})!=0){Fail(tr("Failed to stop PID %1").arg(p.pid));return;}
    }
    fClock.start();WaitForExit();
}
void LocalDeploymentServices::WaitForExit(bool forced) {
    if(!fBusy)return;
    QStringList remaining;for(const auto &p:fStopping)if(SameLocalServiceProcess(p))remaining<<tr("%1 (PID %2)").arg(QFileInfo(p.executable).fileName()).arg(p.pid);
    if(remaining.isEmpty()){fBusy=false;fPhase.clear();emit Stopped();return;}
    if(fClock.elapsed()<(forced?2000:5000)){QTimer::singleShot(100,this,[this,forced]{WaitForExit(forced);});return;}
    if(forced){Fail(tr("Processes still present after force termination: %1").arg(remaining.join(", ")));return;}
    fPhase="force-choice";emit ForceRequired(remaining.join("\n"));
}
void LocalDeploymentServices::ForceRemaining() {
    if(!fBusy || fPhase!="force-choice")return;
    for(const auto &p:fStopping)if(SameLocalServiceProcess(p))QProcess::execute("/bin/kill",{"-KILL",QString::number(p.pid)});
    fClock.start();WaitForExit(true);
}
void LocalDeploymentServices::CancelForce(){Fail(tr("Force termination canceled. Remaining services are retained."));}
