#include "consolelogstore.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QStandardPaths>
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

bool ConsoleLogStore::OpenApplicationLog()
{
    const QString localData=QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation);
    const QString cacheData=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString preferredDirectory=localData.isEmpty() ? QString() : QDir(localData).filePath("logs");
    QStringList directories;
    if(!preferredDirectory.isEmpty())directories.append(preferredDirectory);
    const QString cacheDirectory=cacheData.isEmpty() ? QString() : QDir(cacheData).filePath("logs");
    if(!cacheDirectory.isEmpty() && !directories.contains(cacheDirectory))directories.append(cacheDirectory);
    const QString temporaryDirectory=TemporaryLogDirectory();
    if(!directories.contains(temporaryDirectory))directories.append(temporaryDirectory);

    const QString name=QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz")+
        QString("-%1.log").arg(QCoreApplication::applicationPid());
    for(const QString &directory:directories) {
        if(OpenAppend(fApplication,directory,name)) {
            fApplicationDirectory=directory;
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
    const QString cacheData=QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    QStringList bases;
    if(!fApplicationDirectory.isEmpty())bases.append(fApplicationDirectory);
    if(!cacheData.isEmpty()) {
        const QString cacheLogs=QDir(cacheData).filePath("logs");
        if(!bases.contains(cacheLogs))bases.append(cacheLogs);
    }
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
