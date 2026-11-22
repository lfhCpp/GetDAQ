#include "deploymentcommon.h"
#include "strictjson.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QDir>
#include <QRandomGenerator>
#include <QHostAddress>
#include <QNetworkInterface>
#include <QProcess>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QSslSocket>
#include <QSslCertificate>
#include <QSslError>
#include <QUrl>
#include <QElapsedTimer>
#include <QDateTime>
#include <QRegularExpression>
#include <stdexcept>
#include <functional>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
namespace Deployment {
namespace {thread_local OperationScope *activeOperation=nullptr;}
OperationScope::OperationScope(std::shared_ptr<std::atomic_bool> canceled,int timeoutMs):fCanceled(std::move(canceled)),fTimeoutMs(timeoutMs),fPrevious(activeOperation){fClock.start();activeOperation=this;}
OperationScope::~OperationScope(){activeOperation=fPrevious;}
void OperationScope::Check() const{Require(!fCanceled || !fCanceled->load(),"Local waiting stopped. Accepted work may still be running; refresh the original operation before continuing.");Require(fClock.elapsed()<fTimeoutMs,"Deployment wait limit reached. Refresh the original operation; accepted work was not canceled or replayed.");}
void CheckOperation(){if(activeOperation)activeOperation->Check();}
void Require(bool value,const QString &message){if(!value)throw std::runtime_error(message.toStdString());}
QString Id(const QString &value){Require(QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]{0,95}$").match(value).hasMatch() && value!="..",QStringLiteral("Invalid bounded identity: %1").arg(value));return value;}
QString Sha(const QByteArray &bytes){return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
QString Digest(const QString &value){Require(QRegularExpression("^[0-9a-f]{64}$").match(value).hasMatch(),"Expected SHA256 identity");return value;}
QByteArray Json(const QJsonObject &object){return QJsonDocument(object).toJson(QJsonDocument::Compact);}
QJsonObject Parse(const QByteArray &bytes){GetDAQ::JsonLimits limits;limits.stringBytes=1024*1024;limits.arrayItems=65536;GetDAQ::ParseStrictJson(bytes.toStdString(),limits);QJsonParseError error;const auto doc=QJsonDocument::fromJson(bytes,&error);Require(error.error==QJsonParseError::NoError && doc.isObject(),"Expected strict JSON object");return doc.object();}
QByteArray Read(const QString &path,qint64 maximum){QFile file(path);Require(!QFileInfo(path).isSymLink() && file.open(QIODevice::ReadOnly) && file.size()<=maximum,"Cannot read bounded deployment file: "+path);const auto bytes=file.read(maximum+1);Require(bytes.size()<=maximum && file.error()==QFile::NoError,"Deployment read failed: "+path);return bytes;}
QByteArray Secret(const QString &path){const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);struct stat st;Require(fd>=0,"Cannot open private credential");QFile file;file.open(fd,QIODevice::ReadOnly,QFileDevice::AutoCloseHandle);Require(!::fstat(fd,&st) && S_ISREG(st.st_mode) && st.st_uid==::geteuid() && (st.st_mode&0777)==0600 && st.st_size>0 && st.st_size<=4096,"Credential requires an owned 0600 regular file");const auto bytes=file.readAll().trimmed();Require(!bytes.isEmpty() && !bytes.contains('\r') && !bytes.contains('\n'),"Invalid credential bytes");return bytes;}
void PrivateDirectory(const QString &path){Require(QDir::isAbsolutePath(path) && QDir::cleanPath(path)==path,"Use a canonical absolute directory");QString current;for(const auto &part:path.split('/',QString::SkipEmptyParts)){current+='/'+part;Require(!QFileInfo(current).isSymLink(),"Directory crosses a symbolic link");}if(!QFileInfo::exists(path)){Require(QDir().mkpath(path),"Cannot create directory: "+path);Require(QFile::setPermissions(path,QFile::ReadOwner|QFile::WriteOwner|QFile::ExeOwner),"Cannot protect directory");}struct stat st;Require(!::lstat(QFile::encodeName(path).constData(),&st) && S_ISDIR(st.st_mode) && st.st_uid==::geteuid() && (st.st_mode&0777)==0700,"Directory must be privately owned (0700): "+path);}
void Write(const QString &path,const QByteArray &bytes,bool immutable){PrivateDirectory(QFileInfo(path).absolutePath());Require(!QFileInfo(path).isSymLink(),"Refusing symbolic link output");if(immutable && QFile::exists(path)){Require(Read(path,64*1024*1024)==bytes,"Immutable deployment file changed: "+path);return;}QSaveFile file(path);Require(file.open(QIODevice::WriteOnly) && file.setPermissions(QFile::ReadOwner|QFile::WriteOwner) && file.write(bytes)==bytes.size() && file.commit(),"Cannot persist deployment file: "+path);const int directory=::open(QFile::encodeName(QFileInfo(path).absolutePath()).constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);Require(directory>=0,"Cannot open deployment parent");const int result=::fsync(directory);::close(directory);Require(!result,"Cannot sync deployment parent");if(immutable)Require(QFile::setPermissions(path,QFile::ReadOwner),"Cannot freeze deployment input");}
QByteArray Token(){QByteArray bytes;for(int i=0;i<8;++i)bytes+=QByteArray::number(QRandomGenerator::system()->generate(),16).rightJustified(8,'0');return bytes;}
QString InstalledServiceProgram(const QString &programs,const QString &name,const QString &fallback){
    Require(QFileInfo(name).fileName()==name && !name.contains('\\'),"Invalid installed service name");
    QDir root(programs);if(root.dirName()!="bin")return fallback;root.cdUp();if(root.dirName().startsWith("runtime-"))root.cdUp();
    const auto selector=root.filePath("site/gui-runtime.txt");if(!QFile::exists(selector))return fallback;
    QFile input(selector);Require(input.open(QIODevice::ReadOnly) && input.size()<=4096,"Cannot read installed runtime selection");const auto runtime=QString::fromUtf8(input.readAll()).trimmed();
    Require(runtime=="installed" || QRegularExpression("^runtime-[A-Za-z0-9_-]+$").match(runtime).hasMatch(),"Invalid installed runtime selection");
    const auto expected=QDir::cleanPath(root.absoluteFilePath((runtime=="installed"?QString():runtime+'/')+"bin/"+name));const QFileInfo executable(expected);
    Require(executable.isExecutable() && executable.canonicalFilePath()==expected,"Selected installed service is missing or redirects outside its runtime: "+name);
    return expected;
}
QString Address(const QString &address,bool local){QHostAddress ip(address);Require(ip.protocol()==QAbstractSocket::IPv4Protocol && ip.toString()==address && ip!=QHostAddress::AnyIPv4 && !ip.isMulticast() && ip!=QHostAddress::Broadcast,"Use a concrete IPv4 address: "+address);if(local)Require(QNetworkInterface::allAddresses().contains(ip),"Address is not on this host: "+address);return address;}
int Port(int port,bool allowZero){Require(port>=(allowZero?0:1) && port<=65535,"Invalid port");return port;}
QByteArray Run(const QString &program,const QStringList &arguments,int timeout){
    CheckOperation();QProcess process;process.setProcessChannelMode(QProcess::SeparateChannels);process.start(program,arguments);
    try{Require(process.waitForStarted(5000),"Cannot start installed program: "+QFileInfo(program).fileName());QElapsedTimer clock;clock.start();while(process.state()!=QProcess::NotRunning){CheckOperation();Require(clock.elapsed()<timeout,"Offline check timed out");process.waitForFinished(qMin(200,qMax(1,timeout-int(clock.elapsed()))));}Require(process.exitStatus()==QProcess::NormalExit && process.exitCode()==0,"Offline check failed: "+QString::fromLocal8Bit(process.readAllStandardError()).left(4096));return process.readAllStandardOutput();}
    catch(...){if(process.state()!=QProcess::NotRunning){process.kill();process.waitForFinished(3000);}throw;}
}

void CreateCertificate(const QString &directory,const QString &name){PrivateDirectory(directory);const auto cert=directory+"/tls.crt",key=directory+"/tls.key";if(QFile::exists(cert) && QFile::exists(key))return;Require(!QFile::exists(cert) && !QFile::exists(key),"Incomplete certificate setup; inspect existing files");const auto openssl=QStandardPaths::findExecutable("openssl");Require(!openssl.isEmpty(),"Install the system openssl tool to create a local pairing certificate");Run(openssl,{"req","-x509","-newkey","rsa:2048","-nodes","-days","3650","-subj","/CN="+Id(name),"-keyout",key,"-out",cert},30000);Require(QFile::setPermissions(key,QFile::ReadOwner|QFile::WriteOwner) && QFile::setPermissions(cert,QFile::ReadOwner|QFile::WriteOwner),"Cannot protect TLS material");}
QString CertificateFingerprint(const QString &path){const QSslCertificate cert(Read(path,65536));Require(!cert.isNull(),"Invalid certificate");return QString::fromLatin1(cert.digest(QCryptographicHash::Sha256).toHex());}
Response Request(const Endpoint &endpoint,const QString &method,const QString &path,const QByteArray &body,const QByteArray &contentType,const QMap<QByteArray,QByteArray> &extra){
    CheckOperation();QElapsedTimer clock;clock.start();const QUrl url(endpoint.url);
    Require(url.isValid() && url.userInfo().isEmpty() && (url.path().isEmpty() || url.path()=="/") && url.query().isEmpty() && url.fragment().isEmpty(),"Invalid deployment endpoint URL");Address(url.host());const int port=Port(url.port(url.scheme()=="https"?443:80));
    Require(path.startsWith('/') && !path.contains('\r') && !path.contains('\n') && (method=="GET" || method=="POST"),"Invalid bounded HTTP request");
    QSslSocket socket;socket.setReadBufferSize(3*1024*1024);
    auto wait=[&](std::function<bool(int)> poll,std::function<bool()> ready,int limit,const QString &message){while(!ready()){CheckOperation();Require(clock.elapsed()<limit,message+": "+endpoint.url+path);if(!poll(qMin(250,qMax(1,limit-int(clock.elapsed())))) && socket.state()==QAbstractSocket::UnconnectedState)throw std::runtime_error((message+": "+endpoint.url+path+" ("+socket.errorString()+")").toStdString());}};
    if(url.scheme()=="https"){
        Digest(endpoint.certificateSha256);socket.setPeerVerifyMode(QSslSocket::VerifyNone);socket.setProtocol(QSsl::TlsV1_2OrLater);socket.connectToHostEncrypted(url.host(),quint16(port));
        wait([&](int ms){return socket.waitForEncrypted(ms);},[&]{return socket.isEncrypted();},10000,"TLS connection failed or timed out");
        Require(QString::fromLatin1(socket.peerCertificate().digest(QCryptographicHash::Sha256).toHex())==endpoint.certificateSha256 && socket.peerCertificate().expiryDate()>QDateTime::currentDateTimeUtc(),"Paired TLS identity differs or certificate expired");
    }else{
        Require(url.scheme()=="http" && (QHostAddress(url.host()).isLoopback() || QNetworkInterface::allAddresses().contains(QHostAddress(url.host()))),"Remote deployment endpoints require paired HTTPS");socket.connectToHost(url.host(),quint16(port));
        wait([&](int ms){return socket.waitForConnected(ms);},[&]{return socket.state()==QAbstractSocket::ConnectedState;},5000,"Local service connection failed or timed out");
    }
    CheckOperation();QByteArray request=method.toLatin1()+' '+path.toLatin1()+" HTTP/1.1\r\nHost: "+url.host().toLatin1()+"\r\nConnection: close\r\nContent-Type: "+contentType+"\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n";
    if(!endpoint.credential.isEmpty())request+="Authorization: Bearer "+endpoint.credential+"\r\n";
    for(auto it=extra.begin();it!=extra.end();++it){Require(!it.key().contains('\r') && !it.key().contains('\n') && !it.value().contains('\r') && !it.value().contains('\n'),"Invalid HTTP header");request+=it.key()+": "+it.value()+"\r\n";}
    request+="\r\n";request+=body;Require(socket.write(request)==request.size(),"Request write failed");
    wait([&](int ms){return socket.waitForBytesWritten(ms);},[&]{return !socket.bytesToWrite();},20000,"Request flush failed or timed out");
    QByteArray received;int split=-1;qlonglong length=-1;Response response;
    while(clock.elapsed()<20000){
        CheckOperation();received+=socket.readAll();Require(received.size()<=2*1024*1024+16384,"Deployment response too large");split=received.indexOf("\r\n\r\n");
        if(split>=0 && length<0){Require(split<=16384,"Response headers too large");const auto lines=received.left(split).split('\n');const auto first=lines[0].trimmed().split(' ');Require(first.size()>=2,"Invalid HTTP response");response.code=first[1].toInt();for(const auto &line:lines){const int colon=line.indexOf(':');if(colon>0 && line.left(colon).toLower()=="content-length"){bool ok=false;length=line.mid(colon+1).trimmed().toLongLong(&ok);Require(ok && length>=0 && length<=2*1024*1024,"Invalid response length");}}Require(length>=0,"Response requires Content-Length");}
        if(split>=0 && length>=0 && received.size()>=split+4+length){Require(received.size()==split+4+length,"Unexpected trailing HTTP response");response.body=received.mid(split+4);return response;}
        if(!socket.bytesAvailable() && socket.state()==QAbstractSocket::UnconnectedState)break;
        socket.waitForReadyRead(qMin(250,qMax(1,20000-int(clock.elapsed()))));
    }
    throw std::runtime_error(("Deployment response incomplete or timed out: "+endpoint.url+path+". Refresh the original operation before retrying.").toStdString());
}

QJsonObject Get(const Endpoint &endpoint,const QString &path){const auto response=Request(endpoint,"GET",path);Require(response.code==200,"GET failed ("+QString::number(response.code)+"): "+QString::fromUtf8(response.body).left(2048));return Parse(response.body);}
QJsonObject Post(const Endpoint &endpoint,const QString &path,const QJsonObject &body,const QMap<QByteArray,QByteArray> &headers){const auto response=Request(endpoint,"POST",path,Json(body),"application/json",headers);Require(response.code==200 || response.code==202,"POST failed ("+QString::number(response.code)+"): "+QString::fromUtf8(response.body).left(2048));return Parse(response.body);}
}
