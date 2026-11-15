#include "applicationinstance.h"
#include <QLocalSocket>
#include <QLockFile>
#include <QCryptographicHash>
#include <QDir>
#include <QWidget>
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#ifdef Q_OS_UNIX
#include <unistd.h>
#include <fcntl.h>
#endif
ApplicationInstance::ApplicationInstance(const QString &installation,QObject *parent):QObject(parent) {
    QString user=qEnvironmentVariable("USERNAME");
#ifdef Q_OS_UNIX
    user=QString::number(::getuid());
#endif
    fPrefix="getdaq-gui-"+QString::fromLatin1(QCryptographicHash::hash((QDir(installation).absolutePath()+'|'+user).toUtf8(),QCryptographicHash::Sha256).toHex().left(24));
}
bool ApplicationInstance::Acquire(const QString &configuration) {
    const auto canonical=QFileInfo(configuration).canonicalFilePath();
    const auto scope=configuration.isEmpty()?QString("unconfigured"):(canonical.isEmpty()?QFileInfo(configuration).absoluteFilePath():canonical);
    const auto name=fPrefix+'-'+QString::fromLatin1(QCryptographicHash::hash(scope.toUtf8(),QCryptographicHash::Sha256).toHex().left(16));
    fError.clear();if(fServer && name==fName)return true;
    QLockFile guard(QDir::tempPath()+'/'+name+".startup.lock");guard.setStaleLockTime(10000);
    if(!guard.tryLock(1500)){fError="Another GetDAQ startup is in progress. Retry shortly.";return false;}
    QLocalSocket existing;existing.connectToServer(name);
    if(existing.waitForConnected(300)){existing.write("activate\n");existing.waitForBytesWritten(300);return false;}
    if(existing.error()!=QLocalSocket::ServerNotFoundError && existing.error()!=QLocalSocket::ConnectionRefusedError) {fError=existing.errorString();return false;}
    QLocalServer::removeServer(name);
    auto *server=new QLocalServer(this);server->setSocketOptions(QLocalServer::UserAccessOption);
    if(!server->listen(name)){fError=server->errorString();delete server;return false;}
#ifdef Q_OS_UNIX
    const int fd=int(server->socketDescriptor());const int flags=::fcntl(fd,F_GETFD);
    if(flags<0 || ::fcntl(fd,F_SETFD,flags|FD_CLOEXEC)<0){server->close();delete server;fError="Cannot prevent service inheritance of the GUI instance socket";return false;}
#endif
    connect(server,&QLocalServer::newConnection,this,[this,server]{while(server->hasPendingConnections()) {
        auto *socket=server->nextPendingConnection();socket->disconnectFromServer();socket->deleteLater();Activate();
    }});
    if(fServer){fServer->close();delete fServer;}fServer=server;fName=name;
    return true;
}
void ApplicationInstance::SetWindow(QWidget *window){fWindow=window;if(fPending)Activate();}
void ApplicationInstance::Activate(){if(!fWindow){fPending=true;return;}fPending=false;fWindow->showNormal();fWindow->raise();fWindow->activateWindow();}

QString LocalInstallationDirectory() {
    QDir installation(QCoreApplication::applicationDirPath());installation.cdUp();
    if(installation.dirName().startsWith("runtime-"))installation.cdUp();
    return installation.absolutePath();
}
QString StartupConfigurationKey() {
    return "startup/"+QString::fromLatin1(QCryptographicHash::hash(LocalInstallationDirectory().toUtf8(),QCryptographicHash::Sha256).toHex().left(24))+"/configuration";
}

bool LoadLocalSiteEnvironment(const QString &installation,QString *error) {
    QFile file(installation+"/site/runtime-environment.json");if(!file.exists())return true;
    if(!file.open(QIODevice::ReadOnly) || file.size()>16384){*error="Cannot read site runtime environment";return false;}
    QJsonParseError parseError;const auto doc=QJsonDocument::fromJson(file.readAll(),&parseError);
    if(parseError.error!=QJsonParseError::NoError || !doc.isObject()){*error="Invalid site runtime environment JSON";return false;}
    const QStringList allowed={"GETDAQ_CONTROL_IPV4","GETDAQ_RECEIVER_IPV4","GETDAQ_COBO_IPV4","GETDAQ_LEGACY_FIELD_POLICY_PROXY","GETDAQ_LEGACY_FIELD_POLICY_NODE"};
    const auto object=doc.object();
    for(auto i=object.begin();i!=object.end();++i)if(!allowed.contains(i.key()) || !i.value().isString() || i.value().toString().contains(QChar(0))) {
        *error="Unexpected site runtime environment entry";return false;
    }
    for(auto i=object.begin();i!=object.end();++i)if(qEnvironmentVariableIsEmpty(i.key().toUtf8().constData()))qputenv(i.key().toUtf8().constData(),i.value().toString().toUtf8());
    return true;
}
