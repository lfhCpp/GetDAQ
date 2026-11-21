#include "nodeagent.h"
#include "projectworkspace.h"
#include "originaldeployment.h"
#include "distributedproject.h"
#include "localserviceprocesses.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QProcessEnvironment>
#include <QTcpServer>
#include <QTcpSocket>
#include <QJsonArray>
#include <QThread>
#include <csignal>
namespace Deployment {
namespace {
QString Instance(const QJsonObject &r){return Id(r["group_id"].toString()+'-'+(r["role"]=="ecc"?QString("ecc"):r["receiver_id"].toString()));}
QJsonObject P(const LocalServiceProcess &p){return {{"pid",double(p.pid)},{"executable",p.executable},{"started",QString::fromLatin1(p.started)}};}
LocalServiceProcess P(const QJsonObject &p){LocalServiceProcess r;r.pid=qint64(p["pid"].toDouble());r.executable=p["executable"].toString();r.started=p["started"].toString().toLatin1();return r;}
QString Executable(const QString &programs,const QString &name){const auto path=QDir(programs).filePath(name);Require(QFileInfo(path).isExecutable() && !QFileInfo(path).isSymLink(),"Install the bundled original backend first: "+name);return path;}
void Free(const QString &ip,int port){QTcpServer probe;Require(probe.listen(QHostAddress(Address(ip,true)),quint16(Port(port))),QString("Port is already in use: %1:%2").arg(ip).arg(port));}
void Exit(const QJsonArray &ps,int signal,bool *effects){for(const auto &v:ps){const auto p=P(v.toObject());if(SameLocalServiceProcess(p)){*effects=true;Require(::kill(p.pid,signal)==0,"Cannot signal selected original backend");}}QElapsedTimer timer;timer.start();while(timer.elapsed()<10000){bool running=false;for(const auto &v:ps)running=running||SameLocalServiceProcess(P(v.toObject()));if(!running)return;QThread::msleep(100);}Require(false,"Original backend has not exited; no unscoped process kill was used");}
}
QJsonObject NodeAgent::PrepareOriginal(const QJsonObject &request,const QJsonObject &group,const QMap<QString,QByteArray> &files,const QString &baseDirectory){
 // Experiment content releases are immutable, but runtime installation paths and
 // preparation observations can change without changing the experiment files.
 // Keep each accepted preparation and its audit artifacts in a separate recipe.
 const auto preparationId=Id(request["operation_id"].toString());
 const auto directory=baseDirectory+"/preparations/"+preparationId;PrivateDirectory(directory);
 const auto role=request["role"].toString(),instance=Instance(request);const auto ecc=group["ecc"].toObject();const auto receivers=group["receivers"].toArray();Require(receivers.size()==1,"One receiver assignment is required per ECC group");const auto rx=receivers[0].toObject();const bool syncOnly=request["action"]=="sync";
 Require(syncOnly || (role=="ecc"?ecc["node_id"]:rx["node_id"])==fNode,"Service is assigned to another execution host");
 ValidateOriginalFiles(group,files);PrivateDirectory(directory+"/repository");QJsonObject checked;
 for(const auto &v:group["repository_files"].toArray()){const auto f=v.toObject();const auto path=directory+"/repository/"+f["path"].toString();Write(path,files.value(f["file"].toString()),true);checked[path]=Sha(files.value(f["file"].toString()));}
 const bool projectMode=group["project_mode"].toBool();QString work=directory,repo=directory+"/repository";QJsonObject sync;
 if(projectMode){
  work=ProjectPath(group,fNode,fData);repo=work;const bool accepted=request["configuration_warning_accepted"].toBool();bool canApply=ProjectCanApply(work,accepted);QTcpSocket observation;observation.connectToHost(ecc["address"].toString(),quint16(ecc["port"].toInt()));if(observation.waitForConnected(250)){try{int transition=0;OriginalEccState(ecc["address"].toString(),ecc["port"].toInt(),&transition);canApply=canApply && transition==0;}catch(...){canApply=canApply && accepted;}}
  sync=SyncWorkspace(work,group,files,request["release_id"].toString(),group["sync_configuration"].toBool(true),canApply);
  Write(fRoot+"/projects/"+Sha(work.toUtf8())+".json",Json({{"path",work}}));
  if(syncOnly)return {{"node_id",fNode},{"group_id",group["id"]},{"role",role},{"instance_id",instance},{"configuration_sync",sync},{"phase",sync["state"]}};
  Require(sync["state"]!="pending","Configuration received and cached; wait for the accepted ECC command to finish, or explicitly sync with a state-warning acknowledgement");
 }else if(role=="receiver"){PrivateDirectory(fData);work=fData+'/'+instance;PrivateDirectory(work);}
 const auto backend=group["receiver_backend"].toString("DataExporter");Require(backend=="DataExporter" || backend=="dataRouter","Choose DataExporter or dataRouter");
 const auto program=Executable(fPrograms,role=="ecc"?"getEccSoapServer":backend);
 const auto control=role=="ecc"?ecc["address"].toString():group["receiver_control_ip"].toString(rx["address"].toString(group["receiver_ip"].toString()));const auto data=group["receiver_ip"].toString();Address(control,true);if(role=="receiver")Address(data,true);
 QStringList arguments;QString markerArg,marker;QJsonArray endpoints;
 if(role=="ecc"){
  markerArg="--config-repo-url";marker=repo;arguments=QStringList{"--port",QString::number(ecc["port"].toInt()),"--alarm-port",QString::number(ecc["callback_port"].toInt()),markerArg,repo};endpoints.append(QJsonObject{{"address",control},{"port",ecc["port"]}});endpoints.append(QJsonObject{{"address",control},{"port",ecc["callback_port"]}});
 }else{
  markerArg="--instance";marker=directory+"/service.json";arguments=QStringList{control+':'+QString::number(rx["port"].toInt()),data+':'+QString::number(group["data_port"].toInt())};
  if(backend=="DataExporter")arguments<<"--export-port"<<QString::number(group["export_port"].toInt());else arguments<<"TCP"<<"FrameStorage";
  arguments<<markerArg<<marker;endpoints.append(QJsonObject{{"address",control},{"port",rx["port"]}});endpoints.append(QJsonObject{{"address",data},{"port",group["data_port"]}});if(backend=="DataExporter")endpoints.append(QJsonObject{{"address",data},{"port",group["export_port"]}});
 }
 QJsonArray args;for(const auto &arg:arguments)args.append(arg);const auto logRoot=projectMode?work+"/.getdaq/logs/"+instance:directory+"/logs";PrivateDirectory(logRoot);
 QJsonObject service{{"backend","original-soap"},{"role",role},{"group",group},{"program",program},{"arguments",args},{"marker_argument",markerArg},{"marker",marker},{"working_directory",work},{"repository",repo},{"bind",control},{"port",role=="ecc"?ecc["port"]:rx["port"]},{"ports",endpoints},{"log_directory",logRoot},{"project_mode",projectMode},{"configuration_sync",sync}};
 if(role=="receiver"){
  const auto sentry=Executable(fPrograms,"getdaq-sentry");const auto formats=projectMode?work+"/CoboFormats.xcfg":QDir(fPrograms).absoluteFilePath("../share/get-bench/format/CoboFormats.xcfg");Require(QFileInfo(formats).isReadable(),"Configuration source must include CoboFormats.xcfg");service["formats"]=formats;
  const auto sentryRoot=projectMode?work+"/.getdaq/sentry":directory;PrivateDirectory(sentryRoot);const auto sentryConfig=sentryRoot+"/sentry.json";
  const auto monitorAddress=group["sentry_ip"].toString(control);QJsonObject monitor{{"data_dir",work},{"bind",monitorAddress},{"port",group["sentry_port"]},{"check_process",true},{"process_name",projectMode?QString("Project receivers"):backend},{"settle_ms",500}};if(projectMode)monitor["process_registry"]=sentryRoot+"/receivers.json";
  if(QFile::exists(sentryConfig) && projectMode)Require(Parse(Read(sentryConfig))==monitor,"All receivers sharing a project must use the same Sentry address/port");else Write(sentryConfig,Json(monitor));
  service["sentry_program"]=sentry;service["sentry_sha256"]=Sha(Read(sentry,256*1024*1024));service["sentry_config"]=sentryConfig;service["sentry_root"]=sentryRoot;service["sentry_bind"]=monitorAddress;
 }
 QJsonObject recipe{{"program_sha256",Sha(Read(program,256*1024*1024))},{"working_directory",work},{"role",role},{"ports",endpoints}};QStringList stableArgs=arguments;if(markerArg=="--instance"){const int position=stableArgs.indexOf(markerArg);if(position>=0){stableArgs.removeAt(position);stableArgs.removeAt(position);}}QJsonArray stable;for(const auto &value:stableArgs)stable.append(value);recipe["arguments"]=stable;if(projectMode && role=="receiver"){service["formats_fingerprint"]=FormatFingerprint(work);recipe["formats"]=service["formats_fingerprint"];recipe["sentry_configuration"]=Parse(Read(service["sentry_config"].toString()));}service["runtime_recipe"]=Sha(Json(recipe));
 const auto configFile=directory+"/service.json";Write(configFile,Json(service),true);checked[configFile]=Sha(Read(configFile));
 QJsonObject result{{"backend","original-soap"},{"preparation_id",preparationId},{"schema",1},{"node_id",fNode},{"release_id",request["release_id"]},{"group_id",group["id"]},{"group_sha256",GroupDigest(group)},{"receiver_id",request["receiver_id"]},{"role",role},{"instance_id",instance},{"config_file",configFile},{"program",program},{"program_sha256",Sha(Read(program,256*1024*1024))},{"runtime_recipe",service["runtime_recipe"]},{"checked_files",checked},{"phase","prepared"},{"project_directory",work},{"configuration_sync",sync}};
 auto quote=[](QString text){text.replace("'","'\\''");return "'"+text+"'";};QStringList command{quote(program)};for(const auto &arg:arguments)command.append(quote(arg));result["startup_preview"]="cd -- "+quote(work)+"\n"+command.join(' ');result["arguments"]=args;result["working_directory"]=work;
 Write(directory+"/prepared.json",Json(result),true);Write(baseDirectory+"/current-preparation.json",Json(result));result.remove("checked_files");return result;
}
QJsonObject NodeAgent::OriginalOperation(const QJsonObject &request,bool *effects){
 const auto instance=Instance(request),selectedFile=fRoot+"/instances/"+instance+".json",directory=fRoot+"/deployments/"+Digest(request["release_id"].toString())+'/'+instance;const auto action=request["action"].toString();
 const auto preparedFile=QFile::exists(directory+"/current-preparation.json")?directory+"/current-preparation.json":directory+"/prepared.json";
 auto selected=Parse(Read(action=="activate"?preparedFile:selectedFile));Require(selected["backend"]=="original-soap" && selected["release_id"]==request["release_id"],"Original deployment identity differs");const auto config=Parse(Read(selected["config_file"].toString()));
 if(action=="activate" && request.contains("preparation_id"))Require(selected["preparation_id"]==request["preparation_id"],"Prepared runtime recipe changed; prepare this group again before starting");
 if(action!="activate"){
  if(action=="stop"){
   if(selected["phase"]=="stopped")return selected;
   Require(selected["phase"]!="terminated-needs-inspection","Backend already terminated. Use Review terminated group after confirming boards stopped and data preserved; normal Stop cannot query an exited ECC");
   if(selected["role"]=="ecc"){const auto ecc=config["group"].toObject()["ecc"].toObject();const int state=OriginalEccState(ecc["address"].toString(),ecc["port"].toInt());Require(state!=5 && state!=6,"Stop acquisition in Run Control before closing original ECC");}
   else Require(request["ecc_stopped_confirmed"].toBool(),"Stop this group's ECC normally before closing DataExporter");
  }else Require(request["accepted_repair"].toBool(),"Confirm selected-instance termination first");
  Exit(selected["processes"].toArray(),selected["role"]=="ecc"?SIGINT:SIGTERM,effects);
  if(selected["role"]=="receiver" && config["project_mode"].toBool()){
   bool other=false;for(const auto &name:QDir(fRoot+"/instances").entryList({"*.json"},QDir::Files)){
    const auto prior=Parse(Read(fRoot+"/instances/"+name));if(prior["instance_id"]==selected["instance_id"] || prior["role"]!="receiver")continue;
    const auto service=Parse(Read(prior["config_file"].toString()));if(service["working_directory"]!=config["working_directory"])continue;
    for(const auto &p:prior["processes"].toArray())other=other||SameLocalServiceProcess(P(p.toObject()));
   }
   const auto file=config["sentry_root"].toString()+"/process.json";if(!other && QFile::exists(file)){Exit(QJsonArray{Parse(Read(file))},SIGTERM,effects);QFile::remove(file);}
  }
selected["phase"]=action=="stop"?"stopped":"terminated-needs-inspection";selected["stopped"]=action=="stop";Write(selectedFile,Json(selected));return selected;
 }
 if(QFile::exists(selectedFile)){const auto previous=Parse(Read(selectedFile));Require(previous["release_id"].toString()==request["expected_previous"].toString(),"Selected original deployment changed");if(previous["phase"]=="active" && previous["release_id"]==request["release_id"]){for(const auto &v:previous["processes"].toArray())Require(SameLocalServiceProcess(P(v.toObject())),"Original backend lost a process; inspect before restarting");return previous;}Require(DeploymentInstanceReleased(previous),"Stop the previous group before changing its configuration");}
 const auto hashes=selected["checked_files"].toObject();for(auto it=hashes.begin();it!=hashes.end();++it)Require(Sha(Read(it.key(),64*1024*1024))==it.value().toString(),"A cached original configuration changed");Require(Sha(Read(selected["program"].toString(),256*1024*1024))==selected["program_sha256"].toString(),"Original backend executable changed");
 for(const auto &v:config["ports"].toArray()){const auto endpoint=v.toObject();Free(endpoint["address"].toString(),endpoint["port"].toInt());}
 auto launch=[&](const QString &program,const QStringList &args,const QString &arg,const QString &marker){QProcess p;p.setProgram(program);p.setArguments(args);p.setWorkingDirectory(config["working_directory"].toString());auto env=QProcessEnvironment::systemEnvironment();if(config.contains("formats"))env.insert("GETDAQ_COBO_FORMATS",config["formats"].toString());p.setProcessEnvironment(env);const auto log=config["log_directory"].toString(directory+"/logs")+'/'+(program==config["sentry_program"].toString()?QString("sentry"):selected["role"].toString())+".log";p.setStandardOutputFile(log,QIODevice::Append);p.setStandardErrorFile(log,QIODevice::Append);qint64 pid=0;*effects=true;Require(p.startDetached(&pid),"Cannot start "+program);for(int i=0;i<30;++i){const auto ps=FindLocalArgumentProcesses(program,arg.toLatin1(),marker);for(const auto &proc:ps)if(proc.pid==pid)return P(proc);QThread::msleep(100);}throw std::runtime_error("Original backend exited during startup; inspect its log");};
 QJsonArray processes;QStringList args;for(const auto &v:config["arguments"].toArray())args.append(v.toString());selected["phase"]="starting";selected["stopped"]=false;Write(selectedFile,Json(selected));
 try{processes.append(launch(config["program"].toString(),args,config["marker_argument"].toString(),config["marker"].toString()));selected["processes"]=processes;Write(selectedFile,Json(selected));if(selected["role"]=="receiver" && !config["project_mode"].toBool()){Require(Sha(Read(config["sentry_program"].toString(),256*1024*1024))==config["sentry_sha256"].toString(),"Sentry executable changed");processes.append(launch(config["sentry_program"].toString(),{"--config",config["sentry_config"].toString()},"--config",config["sentry_config"].toString()));selected["processes"]=processes;Write(selectedFile,Json(selected));}
 QElapsedTimer ready;ready.start();bool ok=false;while(ready.elapsed()<5000){try{if(selected["role"]=="ecc")OriginalEccState(config["bind"].toString(),config["port"].toInt());else{QTcpSocket socket;socket.connectToHost(config["bind"].toString(),quint16(config["port"].toInt()));Require(socket.waitForConnected(150),"Waiting for DataExporter listener");}ok=true;break;}catch(...){QThread::msleep(100);}}Require(ok,"Original backend listener is unavailable; inspect original.log");for(const auto &v:processes)Require(SameLocalServiceProcess(P(v.toObject())),"Original backend exited before readiness");selected["phase"]="active";selected.remove("checked_files");Write(selectedFile,Json(selected));return selected;
 }catch(...){selected["processes"]=processes;selected["phase"]="startup-incomplete";Write(selectedFile,Json(selected));throw;}
}
QJsonObject NodeAgent::OriginalStatus(const QJsonObject &selected)const{
 QJsonArray pids;bool all=true;for(const auto &v:selected["processes"].toArray()){const auto p=P(v.toObject());const bool live=SameLocalServiceProcess(p);all=all&&live;if(live && p.executable==selected["program"].toString())pids.append(double(p.pid));}Require(all && pids.size()==1,"Original backend process is unavailable");QJsonObject status;
 const auto config=Parse(Read(selected["config_file"].toString()));if(selected["role"]=="ecc"){const auto ecc=config["group"].toObject()["ecc"].toObject();const int state=OriginalEccState(ecc["address"].toString(),ecc["port"].toInt());status["controller"]=QJsonObject{{"state",state},{"outcome_unknown",false}};}else status["lifecycle"]=QJsonObject{{"state","listening"}};
 return {{"node_id",fNode},{"instance_id",selected["instance_id"]},{"release_id",selected["release_id"]},{"group_sha256",selected["group_sha256"]},{"actual_pids",pids},{"identity",QJsonObject{{"backend","original-soap"},{"release_id",selected["release_id"]}}},{"status",status}};
}
}
