#include "localserviceprocesses.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QThread>
#include <csignal>
#include <stdexcept>
#ifdef Q_OS_LINUX
#include <unistd.h>
#endif
namespace {
QByteArray Read(const QString &path) {QFile f(path);return f.open(QIODevice::ReadOnly)?f.readAll():QByteArray();}
QByteArray Birth(qint64 pid) {
    const auto stat=Read(QString("/proc/%1/stat").arg(pid));
    const auto fields=stat.mid(stat.lastIndexOf(')')+2).split(' ');
    return fields.size()>19 && fields[0]!="Z"?fields[19]:QByteArray();
}
}
bool SameLocalServiceProcess(const LocalServiceProcess &p) {
#ifdef Q_OS_LINUX
    const QString proc=QString("/proc/%1").arg(p.pid);
    return p.pid>1 && QFileInfo(proc).ownerId()==::getuid() &&
        (QFileInfo(proc+"/exe").symLinkTarget()==p.executable || QFileInfo(proc+"/exe").symLinkTarget()==p.executable+" (deleted)") && !p.started.isEmpty() && Birth(p.pid)==p.started;
#else
    Q_UNUSED(p);return false;
#endif
}
static QVector<LocalServiceProcess> Candidates(const QString &program) {
    QVector<LocalServiceProcess> result;
#ifdef Q_OS_LINUX
    const QString expected=QFileInfo(program).canonicalFilePath();
    if(expected.isEmpty())return result;
    QDir runtime=QFileInfo(expected).absoluteDir();runtime.cdUp();
    QString install;
    if(runtime.dirName().startsWith("runtime-")){runtime.cdUp();install=runtime.canonicalPath()+'/';}
    for(const auto &name:QDir("/proc").entryList(QDir::Dirs|QDir::NoDotAndDotDot)) {
        bool ok=false;const qint64 pid=name.toLongLong(&ok);if(!ok || pid<2)continue;
        const QString proc="/proc/"+name;if(QFileInfo(proc).ownerId()!=::getuid())continue;
        const QString exe=QFileInfo(proc+"/exe").symLinkTarget();QString original=exe;
        if(original.endsWith(" (deleted)"))original.chop(10);
        if(QFileInfo(original).fileName()!=QFileInfo(expected).fileName() || (original!=expected && (install.isEmpty() || !original.startsWith(install))))continue;
        LocalServiceProcess process;process.pid=pid;process.executable=exe;process.started=Birth(pid);
        if(SameLocalServiceProcess(process))result.append(process);
    }
#else
    Q_UNUSED(program);
#endif
    return result;
}

namespace {
QString Arg(const LocalServiceProcess &p,const QByteArray &key) {
    const auto args=Read(QString("/proc/%1/cmdline").arg(p.pid)).split('\0');
    for(int i=1;i+1<args.size();++i)if(args[i]==key)return QString::fromLocal8Bit(args[i+1]);
    return {};
}
}
QVector<LocalServiceProcess> FindLocalServiceProcesses(const QString &program,const QString &config) {
    return FindLocalArgumentProcesses(program,"--config",config);
}
QVector<LocalServiceProcess> FindLocalArgumentProcesses(const QString &program,const QByteArray &argument,const QString &config) {
    QVector<LocalServiceProcess> result;const auto expected=QFileInfo(config).canonicalFilePath();
    if(expected.isEmpty())return result;
    for(const auto &p:Candidates(program))if(QFileInfo(Arg(p,argument)).canonicalFilePath()==expected)result.append(p);
    return result;
}
QVector<LocalServiceProcess> FindLocalReceiverProcesses(const QString &program,const QString &supervisorConfig) {
    QVector<LocalServiceProcess> result;
    const auto doc=QJsonDocument::fromJson(Read(supervisorConfig));const auto plan=doc.object()["receiver_plan"].toObject();
    const auto configuration=plan["configuration_sha256"].toString(),artifact=plan["artifact_sha256"].toString();
    if(configuration.isEmpty() || artifact.isEmpty())return result;
    auto processes=Candidates(program);
    if(QFileInfo(program).fileName()=="getdaq-receiver")processes+=Candidates(QFileInfo(program).absolutePath()+"/getdaq-ice-receiver");
    for(const auto &p:processes) {
        if(Arg(p,"--configuration-sha256")!=configuration || Arg(p,"--artifact-sha256")!=artifact)continue;
        for(const auto &value:plan["sources"].toArray()) {
            const auto source=value.toObject();const auto root=QFileInfo(source["output_root"].toString()).canonicalFilePath();
            const auto output=QFileInfo(Arg(p,"--output")).canonicalFilePath();
            if(!root.isEmpty() && output.startsWith(root+'/') && Arg(p,"--sender-id")==source["id"].toString() &&
                Arg(p,"--bind")==source["bind"].toString() && Arg(p,"--port")==QString::number(source["data_port"].toInt())){result.append(p);break;}
        }
    }
    return result;
}

QStringList LocalServiceConfigurations(const QString &program) {
    QStringList result;
    for(const auto &p:Candidates(program)) {
        const auto file=QFileInfo(Arg(p,"--config")).canonicalFilePath();
        if(!file.isEmpty() && !result.contains(file))result.append(file);
    }
    return result;
}

int CleanupOrphanedLocalServiceProcesses(const QString &program,const QByteArray &argument) {
    int stopped=0;
#ifdef Q_OS_LINUX
    const auto name=QFileInfo(program).fileName();
    if(!((name=="getdaq-config-service" && argument=="--root") || (name=="getdaq-node-agent" && argument=="--config")))return 0;
    for(const auto &process:Candidates(program)) {
        const auto configured=Arg(process,argument);if(configured.isEmpty() || !QDir::isAbsolutePath(configured))continue;
        const auto directory=argument=="--config"?QFileInfo(configured).absolutePath():configured;
        bool orphan=!QFileInfo::exists(configured);
        const auto lifetimeLock=directory+(argument=="--config"?"/agent.lock":"/lock");
        const auto descriptors=QString("/proc/%1/fd").arg(process.pid);
        for(const auto &fd:QDir(descriptors).entryList(QDir::AllEntries|QDir::NoDotAndDotDot)) {
            const auto target=QFileInfo(descriptors+'/'+fd).symLinkTarget();
            if(target==directory+" (deleted)" || target==lifetimeLock+" (deleted)")orphan=true;
        }
        if(!orphan || !SameLocalServiceProcess(process))continue;
        if(::kill(process.pid,SIGTERM))throw std::runtime_error("Cannot stop this installation's orphaned management service");
        for(int i=0;i<50 && SameLocalServiceProcess(process);++i)QThread::msleep(100);
        if(SameLocalServiceProcess(process))throw std::runtime_error("Orphaned management service is still finishing accepted work; wait and retry. It was not force killed.");
        ++stopped;
    }
#else
    Q_UNUSED(program);Q_UNUSED(argument);
#endif
    return stopped;
}
