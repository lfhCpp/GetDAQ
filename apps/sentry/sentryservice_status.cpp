#include "sentryservice.h"

#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStorageInfo>

namespace {
ServiceResult StatusFailure(int code,const QString &error) {
    ServiceResult result;
    result.code=code;
    result.json["error"]=error;
    return result;
}

bool ProcessExists(const QString &name) {
    QProcess process;
#ifdef Q_OS_WIN
    process.start("tasklist",{"/FO","CSV","/NH"});
#else
    process.start("ps",{"-A","-o","comm="});
#endif
    if(!process.waitForStarted(2000) || !process.waitForFinished(3000)) {
        process.kill();process.waitForFinished(1000);return false;
    }
    if(process.exitStatus()!=QProcess::NormalExit || process.exitCode()!=0)return false;
    for(const QByteArray &line:process.readAllStandardOutput().split('\n')) {
        QString executable=QString::fromLocal8Bit(line).trimmed();
#ifdef Q_OS_WIN
        if(!executable.startsWith('"'))continue;
        executable=executable.section('"',1,1);
        if(executable.compare(name,Qt::CaseInsensitive)==0)return true;
#else
        if(QFileInfo(executable).fileName()==name)return true;
#endif
    }
    return false;
}
}

ServiceResult SentryFiles::Status(const SentryConfig &config) {
    if(config.checkProcess && !ProcessExists(config.processName))
        return StatusFailure(503,"DAQ process missing or process inspection failed: "+config.processName);
    const QDir directory(config.dataDir);
    if(!directory.exists())return StatusFailure(500,"Data directory does not exist.");
    QStorageInfo disk(directory.absolutePath());disk.refresh();
    if(!disk.isValid() || !disk.isReady() || disk.bytesTotal()<=0 || disk.bytesAvailable()<0)
        return StatusFailure(500,"Cannot inspect data filesystem.");
    qint64 bytes=0;int count=0;
    for(const QFileInfo &file:directory.entryInfoList(QDir::Files|QDir::NoSymLinks)) {
        if(file.suffix()!="graw")continue;
        bytes+=file.size();++count;
    }
    ServiceResult result;
    result.json["disk"]=config.diskName.isEmpty() ? disk.rootPath() : config.diskName;
    result.json["process"]=config.checkProcess ? config.processName : "Process check disabled";
    result.json["data_path"]=directory.absolutePath();
    result.json["data_written_gb"]=double(bytes)*1e-9;
    result.json["data_written_bytes"]=QString::number(bytes);
    result.json["data_path_files"]=count;
    result.json["disk_avail_gb"]=double(disk.bytesAvailable())*1e-9;
    result.json["disk_total_gb"]=double(disk.bytesTotal())*1e-9;
    return result;
}
