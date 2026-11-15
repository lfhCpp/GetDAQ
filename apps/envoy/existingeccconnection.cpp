#include "existingeccconnection.h"
#include "deploymentcommon.h"
#include "localbackendregistry.h"
#include "applicationinstance.h"
#include "protocol.h"
#include "eccsystemclient.h"
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QTcpSocket>
#include <QEventLoop>
#include <QTimer>
#include <QJsonDocument>
#include <QSettings>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUrl>
using namespace Deployment;
namespace {
struct HttpResponse {int code;QByteArray body;};
HttpResponse ReadRequest(const QString &address,int port,const QString &path,const QByteArray &body={}) {
    CheckOperation();QNetworkAccessManager manager;QUrl url;url.setScheme("http");url.setHost(address);url.setPort(port);url.setPath(path);
    QNetworkRequest request(url);request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,QNetworkRequest::ManualRedirectPolicy);
    request.setRawHeader("Connection","close");if(!body.isEmpty()){request.setHeader(QNetworkRequest::ContentTypeHeader,"text/xml; charset=utf-8");request.setRawHeader("SOAPAction","\"\"");}
    auto *reply=body.isEmpty()?manager.get(request):manager.post(request,body);QEventLoop loop;QTimer timeout,interrupt;QString failure;QByteArray bytes;
    timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,[&]{failure="Service check timed out: "+url.toString();reply->abort();loop.quit();});
    QObject::connect(&interrupt,&QTimer::timeout,&loop,[&]{try{CheckOperation();}catch(const std::exception &error){failure=QString::fromUtf8(error.what());reply->abort();loop.quit();}});
    QObject::connect(reply,&QNetworkReply::readyRead,&loop,[&]{if(bytes.size()+reply->bytesAvailable()>262144){failure="ECC discovery response exceeds its size limit";reply->abort();}else bytes+=reply->readAll();});
    QObject::connect(reply,&QNetworkReply::finished,&loop,&QEventLoop::quit);timeout.start(3000);interrupt.start(50);loop.exec();
    Require(failure.isEmpty(),failure);Require(bytes.size()+reply->bytesAvailable()<=262144,"ECC discovery response exceeds its size limit");bytes+=reply->readAll();
    const int code=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();Require(code>0,"Cannot read ECC at "+url.toString()+": "+reply->errorString());CheckOperation();return {code,bytes};
}
bool SpeaksIce(const QString &address,int port){CheckOperation();QTcpSocket socket;socket.connectToHost(address,quint16(port));Require(socket.waitForConnected(1500),QString("Cannot connect to %1:%2. Use Create services if ECC has not been started.").arg(address).arg(port));socket.waitForReadyRead(200);CheckOperation();return socket.peek(4)=="IceP";}
ExistingEccResult CheckNative(RunConfig config,const QString &name,const QString &address,int port){
    config.nativeClient.address=address;config.nativeClient.port=port;config.autoRun.enabled=false;QString error;Require(config.Validate(&error),error);
    EccSystemClient client(config.nativeClient,{});QEventLoop loop;QTimer timeout,interrupt;QByteArray status;QString failure;
    QObject::connect(&client,&EccSystemClient::IdentitySignal,&loop,[&](bool matched,QByteArray,QString problem){if(!problem.isEmpty() || !matched){failure=problem.isEmpty()?QString("ECC identity does not match its saved profile"):problem;loop.quit();}else if(!client.QueryStatus()){failure="Cannot request ECC status";loop.quit();}});
    QObject::connect(&client,&EccSystemClient::StatusSignal,&loop,[&](QByteArray body,QString problem){status=body;failure=problem;loop.quit();});
    timeout.setSingleShot(true);QObject::connect(&timeout,&QTimer::timeout,&loop,[&]{failure="Native ECC check timed out";loop.quit();});
    QObject::connect(&interrupt,&QTimer::timeout,&loop,[&]{try{CheckOperation();}catch(const std::exception &e){failure=QString::fromUtf8(e.what());loop.quit();}});
    timeout.start(8000);interrupt.start(50);Require(client.QueryIdentity(),"Cannot request ECC identity");loop.exec();Require(failure.isEmpty(),failure);Require(!status.isEmpty(),"ECC status was not received");
    const auto controller=Parse(status)["controller"].toObject();const int state=controller["state"].toInt(-1);ExistingEccResult result;result.configuration=config;result.configuration.eccName=name;result.protocol="native";
    result.status=controller["outcome_unknown"].toBool() || controller["needs_reconciliation"].toBool()?QString("Unknown: inspection required"):state==1?QString("Idle"):QString("State %1").arg(state);
    result.detail="Native ECC identity and saved authorization verified. Hardware state has not been changed.";
    return result;
}
}
void RememberEccConnection(const QString &path){QSettings settings;const auto key=StartupConfigurationKey()+"/ecc-connections";auto profiles=settings.value(key).toStringList();profiles.removeAll(path);profiles.prepend(path);while(profiles.size()>128)profiles.removeLast();settings.setValue(key,profiles);}
QStringList KnownEccConnectionProfiles(){auto result=QSettings().value(StartupConfigurationKey()+"/ecc-connections").toStringList();result.append(KnownLocalBackends());const auto project=QSettings().value(StartupConfigurationKey()+"/distributed-project").toString();if(!project.isEmpty()){QDir releases(project+"/console");int count=0;for(const auto &release:releases.entryList(QDir::Dirs|QDir::NoDotAndDotDot,QDir::Time))for(const auto &group:QDir(releases.filePath(release)).entryList(QDir::Dirs|QDir::NoDotAndDotDot)){const auto path=releases.filePath(release+'/'+group+"/envoy.yml");if(QFile::exists(path))result.append(path);if(++count>=256){result.removeDuplicates();return result;}}}result.removeDuplicates();return result;}
ExistingEccResult CheckExistingEcc(const QString &name,const QString &address,int port,const RunConfig &base,const QStringList &profiles,const QString &explicitProfile){
    Address(address);Port(port);Require(!name.trimmed().isEmpty() && name.size()<=128,"Enter an ECC instance name (up to 128 characters)");CheckOperation();
    if(!explicitProfile.isEmpty()){RunConfig imported;QString error;Require(imported.Load(explicitProfile,&error),error);Require(imported.nativeEcc,"Select a saved native GetDAQ connection profile");return CheckNative(imported,name,address,port);}
    if(base.nativeEcc && base.nativeClient.address==address && base.nativeClient.port==port)return CheckNative(base,name,address,port);
    for(const auto &path:profiles){CheckOperation();if(QFileInfo(path).size()>262144)continue;RunConfig known;QString error;if(known.Load(path,&error) && known.nativeEcc && known.nativeClient.address==address && known.nativeClient.port==port)return CheckNative(known,name,address,port);}
    const bool ice=SpeaksIce(address,port);const int httpPort=ice?8083:port;
    const auto identity=ReadRequest(address,httpPort,"/v1/identity");const auto object=QJsonDocument::fromJson(identity.body).object();
    Require(object["kind"]!="getdaq.node-agent" && !object.contains("repository_id"),"This is a host/configuration management port, not an ECC service port. Select the ECC instance or use Create services.");
    Require(identity.code!=401 && !object.contains("service_id"),"This endpoint requires authorization or a saved native connection profile. Existing managed profiles are found automatically; for an external native ECC, import its authorized GetDAQ profile once, or use Create services / host pairing. Do not enter tokens or hashes manually.");
    QString lastError;int state=0,transition=0;QString selectedNamespace;
    for(const auto &ns:{QString("urn:ecc"),QString("urn:http://localhost:8083/")}){NodeConfig node;node.soapNamespace=ns;const auto reply=ReadRequest(address,httpPort,"/",Protocol::EccRequest(node,base.experiment,"GetState"));QString error;const bool valid=Protocol::ParseEcc(reply.body,true,&state,&transition,&error);if(reply.code==200 && valid){selectedNamespace=ns;break;}lastError=error.isEmpty()?QString("HTTP %1").arg(reply.code):error;}
    Require(!selectedNamespace.isEmpty(),"No compatible ECC status response: "+lastError);
    RunConfig config=RunConfig::Default();config.simulation=false;config.experiment=base.experiment;config.fields=base.fields;if(!base.configDir.isEmpty())config.configDir=base.configDir;if(!base.outputDir.isEmpty())config.outputDir=base.outputDir;
    NodeConfig node;node.id=0;node.routerPort=0;node.exporterPort=0;
    if(!base.nativeEcc && !base.simulation && base.nodes.size()==1 && base.nodes[0].host==address && base.nodes[0].eccPort==httpPort){config=base;node=base.nodes[0];}
    config.autoRun.enabled=false;node.host=address;node.eccPort=httpPort;if(node.name.isEmpty())node.name="CoBo 0";node.soapNamespace=selectedNamespace;config.nodes={node};config.eccName=name;
    ExistingEccResult result;result.configuration=config;result.protocol="soap";result.status=state==1?QString("Idle"):QString("State %1 / transition %2").arg(state).arg(transition);result.portChanged=ice;
    result.detail=ice?QString("Ice detected on port %1. SOAP ECC verified on port 8083; the saved profile will use 8083. ").arg(port):QString("SOAP ECC verified. ");
    result.detail+="The SOAP namespace is detected automatically. No board commands were sent.";if(!node.routerPort)result.detail+=" Data target is not configured; connection and status only.";return result;
}
