#include "originaldeployment.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QDir>
#include <QTcpSocket>
#include <QXmlStreamReader>
#include <QJsonArray>
#include <QSet>
namespace Deployment {
namespace {
QString ConfigIdFor(const QJsonObject &group,const QString &kind){
 QSet<QString> paths;for(const auto &v:group["repository_files"].toArray())paths.insert(v.toObject()["path"].toString());
 const auto prefix=kind=="prepare"?QString("configure"):kind;
 const auto has=[&](const QString &id){return paths.contains(prefix+'-'+id+".xcfg") || paths.contains(prefix+'-'+id+".xml");};
 const auto explicitId=group[kind+"_id"].toString();if(!explicitId.isEmpty()){Id(explicitId);Require(has(explicitId),"Missing original "+kind+" configuration: "+explicitId);return explicitId;}
 const auto experiment=Id(group["experiment"].toString());if(has(experiment))return experiment;
 const auto board="cobo"+QString::number(group["board_index"].toInt());if(kind=="describe" && has(board))return board;
 Require(false,"Missing original ECC configuration for "+kind+": "+prefix+'-'+experiment+".xcfg / .xml"+(kind=="describe"?" (also checked "+kind+'-'+board+")":QString()));return {};
}
}
QJsonArray OriginalRepository(const QString &directory,const QString &group,QMap<QString,QByteArray> *files){
    const QDir root(directory);Require(root.exists(),"Choose the original ECC configuration directory");QJsonArray entries;
    QDirIterator it(directory,{"*.xcfg","*.xml"},QDir::Files,QDirIterator::Subdirectories);int n=0;
    while(it.hasNext()){const auto path=it.next();Require(!QFileInfo(path).isSymLink(),"Configuration links must be copied into the master directory: "+path);const auto rel=root.relativeFilePath(path);Require(!rel.startsWith("../") && !rel.contains(':'),"Configuration path escaped the selected directory");const auto bytes=Read(path,16*1024*1024);const auto id=Id(group+"-original-"+QString::number(n++));files->insert(id,bytes);entries.append(QJsonObject{{"file",id},{"path",rel},{"sha256",Sha(bytes)}});}
    Require(!entries.isEmpty(),"The selected directory contains no XML / XCFG files");return entries;
}
void ValidateOriginalFiles(const QJsonObject &group,const QMap<QString,QByteArray> &files){
    Require(group["backend"]=="original-soap","Unsupported original backend");QSet<QString> paths;
    for(const auto &v:group["repository_files"].toArray()){const auto f=v.toObject();const auto path=f["path"].toString();Require(!path.isEmpty() && !QDir::isAbsolutePath(path) && QDir::cleanPath(path)==path && !path.startsWith("../") && !path.contains(':') && !paths.contains(path),"Invalid or duplicate original configuration path");paths.insert(path);const auto id=Id(f["file"].toString());Require(files.contains(id) && Sha(files.value(id))==f["sha256"].toString(),"Original configuration content differs");}
    for(const auto &v:group["repository_files"].toArray()){
        const auto f=v.toObject();QXmlStreamReader xml(files.value(f["file"].toString()));
        while(!xml.atEnd()){xml.readNext();if(xml.isStartElement() && xml.attributes().hasAttribute("include")){
            const auto include=xml.attributes().value("include").toString();const auto relative=QDir::cleanPath(QFileInfo(f["path"].toString()).path()+'/'+include);
            Require(!QDir::isAbsolutePath(include) && !include.contains(':') && !relative.startsWith("../") && paths.contains(relative),"Copy the referenced XML/XCFG file inside the master configuration directory and use a relative include: "+include);
        }}Require(!xml.hasError(),"Invalid original XML/XCFG: "+f["path"].toString()+": "+xml.errorString());
    }
    for(const auto &kind:{QString("describe"),QString("prepare"),QString("configure")})ConfigIdFor(group,kind);
}
int OriginalEccState(const QString &address,int port){
    const QByteArray body="<?xml version=\"1.0\"?><SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:ecc=\"urn:http://localhost:8083/\"><SOAP-ENV:Body><ecc:GetState/></SOAP-ENV:Body></SOAP-ENV:Envelope>";
    QTcpSocket socket;socket.connectToHost(address,quint16(port));Require(socket.waitForConnected(1500),"Cannot inspect original SOAP ECC state");socket.write("POST / HTTP/1.1\r\nHost: "+address.toLatin1()+"\r\nContent-Type: text/xml\r\nSOAPAction: \"\"\r\nConnection: close\r\nContent-Length: "+QByteArray::number(body.size())+"\r\n\r\n"+body);socket.waitForBytesWritten(1500);QByteArray response;QElapsedTimer timer;timer.start();int length=-1,split=-1;
    while(timer.elapsed()<3000){if(socket.bytesAvailable() || socket.waitForReadyRead(100)){response+=socket.readAll();Require(response.size()<262144,"ECC response exceeds limit");split=response.indexOf("\r\n\r\n");if(split>=0 && length<0){for(const auto &line:response.left(split).split('\n'))if(line.toLower().startsWith("content-length:"))length=line.mid(15).trimmed().toInt();}if(split>=0 && length>=0 && response.size()>=split+4+length)break;}if(socket.state()==QAbstractSocket::UnconnectedState)break;}
    Require(response.startsWith("HTTP/1.1 200") || response.startsWith("HTTP/1.0 200"),"SOAP ECC did not return HTTP 200");QXmlStreamReader xml(response.mid(split+4));int state=-1,error=-1,transition=-1;while(!xml.atEnd()){xml.readNext();if(xml.isStartElement()){const auto name=xml.name().toString();if(name=="State")state=xml.readElementText().toInt();else if(name=="Transition")transition=xml.readElementText().toInt();else if(name=="ErrorCode")error=xml.readElementText().toInt();}}
    Require(!xml.hasError() && error==0 && state>=1 && transition==0,"Original ECC is busy or its state is unknown");return state;
}
RunConfig OriginalConsole(const QJsonObject &master,const QJsonObject &group,const QJsonObject &active,const RunConfig &base){
    const auto root=master["root"].toString();const auto directory=root+"/console/"+active["release_id"].toString()+'/'+Id(group["id"].toString());PrivateDirectory(directory);RunConfig c=base;c.simulation=false;c.nativeEcc=false;c.eccName=group["name"].toString();c.experiment=group["experiment"].toString();c.autoRun.enabled=false;c.configDir=group["master_repository"].toString();c.outputDir=base.outputDir;c.filePath=directory+"/envoy.yml";c.stepTimeoutMs=30000;c.nodes.clear();
    // Reusing an activated group must preserve locally saved folder choices.
    if(QFileInfo::exists(c.filePath)){
        RunConfig saved;QString error;Require(saved.Load(c.filePath,&error),"Cannot reload saved console paths: "+error);
        c.configDir=saved.configDir;c.outputDir=saved.outputDir;
    }
    NodeConfig n;n.id=group["board_index"].toInt();n.name=QString("CoBo %1").arg(n.id);n.describeId=ConfigIdFor(group,"describe");n.prepareId=ConfigIdFor(group,"prepare");n.configureId=ConfigIdFor(group,"configure");n.host=group["ecc"].toObject()["address"].toString();n.eccPort=group["ecc"].toObject()["port"].toInt();n.routerHost=group["receiver_ip"].toString();n.routerPort=group["data_port"].toInt();n.exporterPort=0;n.sentryHost=n.routerHost;n.sentryPort=group["sentry_port"].toInt();c.nodes.append(n);QString error;Require(c.Validate(&error) && c.Save(c.filePath,&error),error);return c;
}
}
