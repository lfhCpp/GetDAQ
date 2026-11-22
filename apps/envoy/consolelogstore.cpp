#include "consolelogstore.h"
#include "runconfig.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QUuid>

namespace {
QString TemporaryLogDirectory()
{
    return QDir(QDir::tempPath()).filePath(
        QString("GetDAQ-%1/logs").arg(QCoreApplication::applicationPid()));
}

bool OpenAppend(QFile &file,const QString &directory,const QString &name)
{
    if(directory.isEmpty() || !QDir().mkpath(directory))return false;
    file.setFileName(QDir(directory).filePath(name));
    return file.open(QIODevice::WriteOnly|QIODevice::Append);
}
}

bool ConsoleLogStore::OpenApplicationLog(const QString &recordsRoot)
{
    const QString localData=recordsRoot.isEmpty()?RunConfig::Default().outputDir:recordsRoot;
    fRecordsRoot=QDir::cleanPath(localData);
    const QString preferredDirectory=localData.isEmpty() ? QString() : QDir(localData).filePath("logs");
    QStringList directories;
    if(!preferredDirectory.isEmpty())directories.append(preferredDirectory);
    const QString temporaryDirectory=TemporaryLogDirectory();
    if(!directories.contains(temporaryDirectory))directories.append(temporaryDirectory);

    if(fSession.isEmpty())fSession=QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz")+QString("-%1-").arg(QCoreApplication::applicationPid())+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString name="application.log";
    for(const QString &directory:directories) {
        const auto sessionDirectory=QDir(directory).filePath("sessions/"+fSession);
        if(OpenAppend(fApplication,sessionDirectory,name)) {
            fApplicationDirectory=sessionDirectory;
            fUsedFallback=(directory!=preferredDirectory);
            fError.clear();
            return true;
        }
    }
    fError=fApplication.errorString();
    return false;
}

bool ConsoleLogStore::OpenConnectionLog()
{
    CloseConnectionLog();
    QStringList bases;
    if(!fApplicationDirectory.isEmpty())bases.append(fApplicationDirectory);
    const QString temporary=TemporaryLogDirectory();
    if(!bases.contains(temporary))bases.append(temporary);

    const QString name=QString("connection-%1-%2-%3.log")
        .arg(QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz"))
        .arg(QCoreApplication::applicationPid())
        .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    for(const QString &base:bases) {
        const QString directory=QDir(base).filePath("connections");
        if(OpenAppend(fConnection,directory,name)) {fError.clear();return true;}
    }
    fError=fConnection.errorString();
    return false;
}

void ConsoleLogStore::CloseConnectionLog()
{
    if(!fConnection.isOpen())return;
    fConnection.flush();
    fConnection.close();
}

void ConsoleLogStore::Append(const QByteArray &line)
{
    if(fApplication.isOpen()) {fApplication.write(line);fApplication.flush();}
    if(fConnection.isOpen()) {fConnection.write(line);fConnection.flush();}
}

bool ConsoleLogStore::SetRecordsDirectory(const QString &recordsRoot)
{
    const auto root=QDir::cleanPath(recordsRoot);
    if(root==fRecordsRoot && fApplication.isOpen() && !fUsedFallback)return true;
    if(!fApplication.isOpen())return OpenApplicationLog(root);
    const auto destination=QDir(root).filePath("logs/sessions/"+fSession);
    if(destination==fApplicationDirectory)return true;
    fApplication.flush();if(fConnection.isOpen())fConnection.flush();
    const auto oldDirectory=fApplicationDirectory;
    const auto connectionName=fConnection.isOpen()?QDir(oldDirectory).relativeFilePath(fConnection.fileName()):QString();
    if(!QDir().mkpath(destination)){fError="Cannot create logs under GUI Run records: "+root;return false;}
    QDirIterator entries(oldDirectory,QDir::Files|QDir::NoDotAndDotDot,QDirIterator::Subdirectories);
    while(entries.hasNext()){
        const auto source=entries.next(),target=QDir(destination).filePath(QDir(oldDirectory).relativeFilePath(source));
        if(QFileInfo(source).isSymLink() || QFileInfo(target).isSymLink() || !QDir().mkpath(QFileInfo(target).absolutePath()) || (QFile::exists(target) && !QFile::remove(target)) || !QFile::copy(source,target)){
            fError="Cannot relocate this GUI session log to: "+target;return false;
        }
    }
    QFile nextApplication,nextConnection;
    if(!OpenAppend(nextApplication,destination,"application.log") || (!connectionName.isEmpty() && !OpenAppend(nextConnection,QFileInfo(destination+'/'+connectionName).absolutePath(),QFileInfo(connectionName).fileName()))){fError="Cannot open relocated GUI session logs";return false;}
    fApplication.close();fConnection.close();nextApplication.close();nextConnection.close();
    OpenAppend(fApplication,destination,"application.log");
    if(!connectionName.isEmpty())OpenAppend(fConnection,QFileInfo(destination+'/'+connectionName).absolutePath(),QFileInfo(connectionName).fileName());
    fApplicationDirectory=destination;fRecordsRoot=root;fUsedFallback=false;fError.clear();
    QDir(oldDirectory).removeRecursively();return true;
}
