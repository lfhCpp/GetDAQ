#include "distributedproject.h"
#include "originaldeployment.h"
#include <QJsonDocument>
#include <QSet>
#include <QHostAddress>
#include <QUrl>
#include <QDir>
namespace Deployment {
QString GroupDigest(QJsonObject group){group.remove("name");return Sha(Json(group));}
namespace {
QJsonObject Object(QJsonValue value){Require(value.isObject(),"Expected configuration object");return value.toObject();}

}
void ValidateGroupFiles(const QJsonObject &group,const QMap<QString,QByteArray> &files){ValidateOriginalFiles(group,files);}
void ValidateRelease(const QJsonObject &manifest){Require(manifest.size()==6 && manifest["schema"].toInt()==1 && manifest["kind"]=="getdaq.distributed-release","Unsupported distributed release");Id(manifest["project_id"].toString());const auto hosts=manifest["hosts"].toArray(),groups=manifest["groups"].toArray(),files=manifest["files"].toArray();Require(!hosts.isEmpty() && hosts.size()<=64 && !groups.isEmpty() && groups.size()<=64 && !files.isEmpty() && files.size()<=512,"Distributed release budget exceeded");QSet<QString> hostIds,groupIds,fileIds,boards;QMap<QString,QSet<int>> ports;QMap<QString,QString> sharedMonitors,sharedConfigurations;QString projectExperiment;
    for(const auto &host:hosts){const auto object=Object(host);const auto id=Id(object["id"].toString());Require(!hostIds.contains(id),"Duplicate node identity");hostIds.insert(id);}
    for(const auto &file:files){const auto entry=Object(file);const auto id=Id(entry["id"].toString());Require(!fileIds.contains(id),"Duplicate file identity");fileIds.insert(id);Digest(entry["sha256"].toString());Require(entry["bytes"].toDouble()>=0 && entry["bytes"].toDouble()<=16*1024*1024 && (entry["objects"].toArray().size()>0 || entry["bytes"].toDouble()==0) && entry["objects"].toArray().size()<=512,"Invalid content size");for(const auto &chunk:entry["objects"].toArray())Digest(chunk.toString());}
    auto reserve=[&](const QString &node,int port){Require(hostIds.contains(node),"Instance refers to an unregistered node");Port(port);Require(!ports[node].contains(port),"Two services on one node use the same port");ports[node].insert(port);};
    for(const auto &value:groups){const auto group=Object(value);const auto id=Id(group["id"].toString());Require(!groupIds.contains(id),"Duplicate control group");groupIds.insert(id);if(group["backend"]=="original-soap"){
 const auto ecc=group["ecc"].toObject();Address(ecc["address"].toString());reserve(ecc["node_id"].toString(),ecc["port"].toInt());reserve(ecc["node_id"].toString(),ecc["callback_port"].toInt());
 const auto receiverKind=group["receiver_backend"].toString("DataExporter");Require(receiverKind=="DataExporter" || receiverKind=="dataRouter","Unknown receiver backend");
 if(group.contains("receiver_control_ip"))Address(group["receiver_control_ip"].toString());
 if(group["project_mode"].toBool()){
  const auto board=QString::number(group["board_index"].toInt());Require(!boards.contains(board),"Project CoBo IDs must be unique");boards.insert(board);
  if(projectExperiment.isEmpty())projectExperiment=group["experiment"].toString();Require(group["experiment"].toString()==projectExperiment,"Shared project groups must use the same Experiment / Run");
  const auto dirs=group["project_directories"].toObject();for(auto it=dirs.begin();it!=dirs.end();++it)Require(hostIds.contains(it.key()) && QDir::isAbsolutePath(it.value().toString()) && QDir::cleanPath(it.value().toString())!="/","Choose an absolute project directory per host");
 }
 Address(group["receiver_ip"].toString());for(const auto &r:group["receivers"].toArray()){const auto rx=r.toObject();reserve(rx["node_id"].toString(),rx["port"].toInt());reserve(rx["node_id"].toString(),group["data_port"].toInt());if(receiverKind=="DataExporter")reserve(rx["node_id"].toString(),group["export_port"].toInt());
 if(group["project_mode"].toBool()){
  const auto node=rx["node_id"].toString(),folder=group["project_directories"].toObject()[node].toString();Require(!folder.isEmpty(),"Set the receiver host project folder");
  const auto key=node+':'+QDir::cleanPath(folder);const auto endpoint=group["sentry_ip"].toString(group["receiver_control_ip"].toString(rx["address"].toString()))+':'+QString::number(group["sentry_port"].toInt());
  QJsonObject files;for(const auto &v:group["repository_files"].toArray()){const auto f=v.toObject();files[f["path"].toString()]=f["sha256"];}
  const auto content=Sha(Json(files));if(sharedMonitors.contains(key)){Require(sharedMonitors[key]==endpoint,"One shared project needs one Sentry address/port");Require(sharedConfigurations[key]==content,"Groups sharing a project folder must use the same complete configuration source");}else {reserve(node,group["sentry_port"].toInt());sharedMonitors[key]=endpoint;sharedConfigurations[key]=content;}
 }else reserve(rx["node_id"].toString(),group["sentry_port"].toInt());}
 for(const auto &f:group["repository_files"].toArray())Require(fileIds.contains(f.toObject()["file"].toString()),"Original repository file missing");continue;
 }Require(false,"This project contains a retired native backend group. Remove or recreate it as an original SOAP ECC group.");
    }
}
Bundle BuildBundle(const QString &projectId,const QJsonArray &hosts,const QJsonArray &groups,const QMap<QString,QByteArray> &files){Bundle bundle;QJsonArray entries;qint64 total=0;for(auto it=files.begin();it!=files.end();++it){Id(it.key());Require(it.value().size()<=16*1024*1024,"Invalid release file size");total+=it.value().size();Require(total<=64*1024*1024,"Release content exceeds 64 MiB");QJsonArray chunks;for(int offset=0;offset<it.value().size();offset+=32768){const auto bytes=it.value().mid(offset,32768);const auto sha=Sha(bytes);bundle.objects[sha]=bytes;chunks.append(sha);}entries.append(QJsonObject{{"id",it.key()},{"sha256",Sha(it.value())},{"bytes",it.value().size()},{"objects",chunks}});}bundle.manifest={{"schema",1},{"kind","getdaq.distributed-release"},{"project_id",Id(projectId)},{"hosts",hosts},{"groups",groups},{"files",entries}};ValidateRelease(bundle.manifest);for(const auto &group:groups)ValidateGroupFiles(group.toObject(),files);Require(Json(bundle.manifest).size()<=1024*1024,"Release manifest exceeds limit");return bundle;}
QByteArray Assemble(const QJsonObject &file,const QMap<QString,QByteArray> &objects){QByteArray bytes;const auto chunks=file["objects"].toArray();for(int i=0;i<chunks.size();++i){const auto sha=Digest(chunks[i].toString());Require(objects.contains(sha) && Sha(objects.value(sha))==sha,"Content chunk missing or damaged");const auto part=objects.value(sha);Require(part.size()>0 && part.size()<=32768 && (i==chunks.size()-1 || part.size()==32768),"Invalid content chunk size");bytes+=part;Require(bytes.size()<=16*1024*1024,"Assembled content exceeds budget");}Require(bytes.size()==file["bytes"].toInt() && Sha(bytes)==file["sha256"].toString(),"Assembled file differs");return bytes;}
QMap<QString,QByteArray> ReadBundleFiles(const Bundle &bundle){QMap<QString,QByteArray> files;for(const auto &file:bundle.manifest["files"].toArray())files[file.toObject()["id"].toString()]=Assemble(file.toObject(),bundle.objects);return files;}
QString UploadBundle(const Endpoint &endpoint,const QString &repositoryId,const Bundle &bundle){ValidateRelease(bundle.manifest);Require(Get(endpoint,"/v1/identity")["repository_id"].toString()==Digest(repositoryId),"Repository identity changed");const QMap<QByteArray,QByteArray> headers{{"X-Repository-ID",repositoryId.toLatin1()}};auto upload=[&](const QByteArray &bytes){const auto sha=Sha(bytes);if(!Get(endpoint,"/v1/objects/"+sha)["present"].toBool()){const auto response=Request(endpoint,"POST","/v1/objects/"+sha,bytes,"application/octet-stream",headers);Require(response.code==200,"Content upload failed");}return sha;};for(auto it=bundle.objects.begin();it!=bundle.objects.end();++it){Require(Sha(it.value())==it.key(),"Content map identity differs");upload(it.value());}const auto bytes=Json(bundle.manifest);QJsonArray chunks;for(int offset=0;offset<bytes.size();offset+=32768)chunks.append(upload(bytes.mid(offset,32768)));const auto receipt=Post(endpoint,"/v1/deployments",{{"manifest_objects",chunks}},headers);Require(receipt["release_id"].toString()==Sha(bytes) && receipt["repository_id"].toString()==repositoryId && receipt["content_verified"].toBool(),"Release registration receipt differs");return Sha(bytes);}
}
