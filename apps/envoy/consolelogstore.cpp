#include "consolelogstore.h"
#include "runconfig.h"

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
    const QString localData=DefaultWorkspaceDirectory();
    const QString preferredDirectory=localData.isEmpty() ? QString() : QDir(localData).filePath("logs");
    QStringList directories;
    if(!preferredDirectory.isEmpty())directories.append(preferredDirectory);
    const QString temporaryDirectory=TemporaryLogDirectory();
    if(!directories.contains(temporaryDirectory))directories.append(temporaryDirectory);

    const QString session=QDateTime::currentDateTime().toString("yyyyMMdd-hhmmss-zzz")+QString("-%1-").arg(QCoreApplication::applicationPid())+QUuid::createUuid().toString(QUuid::WithoutBraces);
    const QString name="application.log";
    for(const QString &directory:directories) {
        const auto sessionDirectory=QDir(directory).filePath("sessions/"+session);
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
