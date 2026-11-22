#include "nodeagent.h"
#include "projectworkspace.h"
#include "originaldeployment.h"
#include "localserviceprocesses.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QThread>
#include <QElapsedTimer>
#include <QTcpSocket>
#include <QUrl>
#include <QUrlQuery>
#include <QJsonArray>
namespace Deployment {
namespace {
LocalServiceProcess Process(const QJsonObject &p){LocalServiceProcess result;result.pid=qint64(p["pid"].toDouble());result.executable=p["executable"].toString();result.started=p["started"].toString().toLatin1();return result;}
QJsonObject Wire(const LocalServiceProcess &p){return {{"pid",double(p.pid)},{"executable",p.executable},{"started",QString::fromLatin1(p.started)}};}
}
bool NodeAgent::ProjectCanApply(const QString &project,bool allowUnknown)const{
 for(const auto &name:QDir(fRoot+"/instances").entryList({"*.json"},QDir::Files)){
  const auto record=Parse(Read(fRoot+"/instances/"+name));if(record["backend"]!="original-soap")continue;const auto service=Parse(Read(record["config_file"].toString()));if(service["working_directory"]!=project)continue;
  if(record["phase"]=="active" || record["phase"]=="starting"){
   const auto ecc=service["group"].toObject()["ecc"].toObject();try{int transition=0;OriginalEccState(ecc["address"].toString(),ecc["port"].toInt(),&transition);if(transition!=0)return false;}catch(...){if(!allowUnknown)return false;}
  }
 }return true;
}
void NodeAgent::ApplyPendingProjects(){
 for(const auto &name:QDir(fRoot+"/projects").entryList({"*.json"},QDir::Files)){
  const auto project=Parse(Read(fRoot+"/projects/"+name))["path"].toString();const auto pending=project+"/.getdaq/pending-configuration.json";if(!QFile::exists(pending) || !ProjectCanApply(project))continue;
  const auto record=Parse(Read(pending));QMap<QString,QByteArray> files;QJsonArray mapping;const auto hashes=record["files"].toObject();int index=0;
  for(auto it=hashes.begin();it!=hashes.end();++it){const auto id="pending-"+QString::number(index++);const auto bytes=Read(record["cache_directory"].toString()+'/'+it.key(),16*1024*1024);Require(Sha(bytes)==it.value().toString(),"Pending configuration cache differs");files[id]=bytes;mapping.append(QJsonObject{{"file",id},{"path",it.key()},{"sha256",it.value()}});}
  SyncWorkspace(project,{{"repository_files",mapping}},files,record["release_id"].toString(),true,true);
 }
}
void NodeAgent::UpdateProjectRegistries(){
 QMap<QString,QJsonArray> projects;
 for(const auto &name:QDir(fRoot+"/instances").entryList({"*.json"},QDir::Files)){
  const auto entry=Parse(Read(fRoot+"/instances/"+name));if(entry["backend"]!="original-soap" || entry["role"]!="receiver")continue;const auto config=Parse(Read(entry["config_file"].toString()));if(!config["project_mode"].toBool())continue;const auto registry=config["sentry_root"].toString()+"/receivers.json";
  if(!projects.contains(registry))projects[registry]=QJsonArray();if(entry["phase"]!="active" && entry["phase"]!="starting")continue;
  for(const auto &p:entry["processes"].toArray())if(p.toObject()["executable"]==entry["program"])projects[registry].append(p);
 }
 for(auto it=projects.begin();it!=projects.end();++it)Write(it.key(),Json({{"processes",it.value()}}));
}
QJsonObject NodeAgent::StartProjectMonitor(const QJsonObject &request,bool *effects){
 const auto instance=Id(request["group_id"].toString()+'-'+request["receiver_id"].toString());const auto selected=Parse(Read(fRoot+"/instances/"+instance+".json"));Require(selected["release_id"]==request["release_id"] && selected["phase"]=="active","Start the selected receiver first");const auto config=Parse(Read(selected["config_file"].toString()));if(!config["project_mode"].toBool())return selected;
 const auto root=config["sentry_root"].toString(),record=root+"/process.json";const auto executable=InstalledServiceProgram(fPrograms,"getdaq-sentry",config["sentry_program"].toString());
 if(QFile::exists(record) && SameLocalServiceProcess(Process(Parse(Read(record))))){auto result=selected;const auto running=Parse(Read(record))["executable"].toString();result["monitor_program"]=running;result["monitor_update_pending"]=running!=executable;return result;}
 QProcess process;process.setProgram(executable);process.setArguments({"--config",config["sentry_config"].toString()});process.setWorkingDirectory(config["working_directory"].toString());process.setStandardOutputFile(root+"/sentry.log",QIODevice::Append);process.setStandardErrorFile(root+"/sentry.log",QIODevice::Append);qint64 pid=0;*effects=true;Require(process.startDetached(&pid),"Cannot start project Sentry");
 bool found=false;for(int i=0;i<30;++i){for(const auto &p:FindLocalServiceProcesses(executable,config["sentry_config"].toString()))if(p.pid==pid){auto identity=Wire(p);identity["configured_program"]=config["sentry_program"];identity["program_sha256"]=Sha(Read(executable,256*1024*1024));Write(record,Json(identity));found=true;}if(found)break;QThread::msleep(100);}Require(found,"Project Sentry exited; inspect its log");
 const auto settings=Parse(Read(config["sentry_config"].toString()));QTcpSocket socket;QElapsedTimer startup;startup.start();
 while(startup.elapsed()<5000){
  socket.connectToHost(settings["bind"].toString(),quint16(settings["port"].toInt()));
  if(socket.waitForConnected(100))return selected;
  socket.abort();
  // ConnectionRefused may return immediately. Bound startup by elapsed time,
  // not by a retry count that can expire before Sentry reaches main().
  if(!SameLocalServiceProcess(Process(Parse(Read(record)))))break;
  QThread::msleep(50);
 }
 QFile output(root+"/sentry.log");QString detail;if(output.open(QIODevice::ReadOnly)){output.seek(qMax<qint64>(0,output.size()-2048));detail=QString::fromUtf8(output.readAll()).trimmed();}
 Require(false,"Project Sentry listener unavailable after startup wait. "+detail);return selected;
}
QJsonObject NodeAgent::ServiceLog(const QString &path)const{
 const QUrl url(path);const auto pieces=url.path().split('/',QString::SkipEmptyParts);Require(pieces.size()==5 && pieces[0]=="v1" && pieces[1]=="instances" && pieces[3]=="logs","Invalid service log request");
 const auto instance=Id(pieces[2]),component=pieces[4];Require(component=="ecc" || component=="receiver" || component=="sentry","Unknown log component");if(!QFile::exists(fRoot+"/instances/"+instance+".json"))return {{"offset",0},{"generation","missing"},{"text",""},{"available",false}};const auto selected=Parse(Read(fRoot+"/instances/"+instance+".json"));Require(selected["backend"]=="original-soap","Logs are available only for registered original services");const auto config=Parse(Read(selected["config_file"].toString()));
 Require(component=="sentry"?selected["role"]=="receiver":selected["role"]==component,"Log does not belong to this instance");
 const auto log=component=="sentry" && config["project_mode"].toBool()?config["sentry_root"].toString()+"/sentry.log":config["log_directory"].toString(QFileInfo(selected["config_file"].toString()).absolutePath()+"/logs")+'/'+component+".log";
 const QUrlQuery query(url);bool ok=false;const auto offset=query.queryItemValue("offset").toLongLong(&ok);Require(ok || !query.hasQueryItem("offset"),"Invalid log cursor");auto result=ReadServiceLog(log,ok?offset:0,query.queryItemValue("generation"),query.queryItemValue("start")=="now");result["instance_id"]=instance;result["component"]=component;return result;
}
}
