#include "projectworkspace.h"
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonArray>
#include <QDateTime>
#include <QLockFile>
#include <QSaveFile>
#include <sys/stat.h>
#include <QXmlStreamReader>
#include <QSet>
namespace Deployment {
namespace { void WorkspaceWrite(const QString &path,const QByteArray &bytes){Require(QDir().mkpath(QFileInfo(path).absolutePath()),"Cannot create configuration subdirectory");QSaveFile file(path);Require(file.open(QIODevice::WriteOnly) && file.write(bytes)==bytes.size() && file.commit(),"Cannot update project configuration: "+path);} }
QString FormatFingerprint(const QString &project){
 QJsonObject hashes;QSet<QString> visited;QStringList pending{"CoboFormats.xcfg"};
 while(!pending.isEmpty()){const auto name=pending.takeLast();if(visited.contains(name))continue;Require(visited.size()<512,"Frame dictionary include budget exceeded");visited.insert(name);Require(!QDir::isAbsolutePath(name) && !name.startsWith("../"),"Frame dictionary include escaped project");const auto bytes=Read(project+'/'+name,16*1024*1024);hashes[name]=Sha(bytes);QXmlStreamReader xml(bytes);while(!xml.atEnd()){xml.readNext();if(xml.isStartElement() && xml.attributes().hasAttribute("include"))pending.append(QDir::cleanPath(QFileInfo(name).path()+'/'+xml.attributes().value("include").toString()));}Require(!xml.hasError(),"Invalid frame dictionary");}
 return Sha(Json(hashes));
}
QString ProjectPath(const QJsonObject &group,const QString &node,const QString &fallback){
 const auto configured=group["project_directories"].toObject()[node].toString(fallback);
 Require(QDir::isAbsolutePath(configured),"Choose an absolute project data folder on each execution host");
 const auto path=QDir::cleanPath(configured);Require(path!="/","The filesystem root cannot be a project folder");
 if(QFileInfo::exists(path))Require(QFileInfo(path).isDir() && !QFileInfo(path).isSymLink(),"Project folder must be a real directory");return path;
}
QJsonObject SyncWorkspace(const QString &project,const QJsonObject &group,const QMap<QString,QByteArray> &files,const QString &release,bool enabled,bool canApply){
 Require(QDir().mkpath(project) && QFileInfo(project).isWritable(),"Cannot use project folder: "+project);const auto meta=project+"/.getdaq";PrivateDirectory(meta);QLockFile lock(meta+"/configuration.lock");Require(lock.tryLock(0),"Another configuration update is active in this project folder");
 const auto stage=meta+"/configuration-cache/"+Digest(release);PrivateDirectory(stage);QJsonObject hashes;
 for(const auto &v:group["repository_files"].toArray()){
  const auto f=v.toObject();const auto name=f["path"].toString();Require(!name.isEmpty() && !QDir::isAbsolutePath(name) && QDir::cleanPath(name)==name && !name.startsWith("../"),"Invalid relative configuration name");Require(!name.startsWith(".getdaq/") && name!=".getdaq" && !name.endsWith(".graw",Qt::CaseInsensitive),"Configuration source must not contain project state or raw data");
  const auto bytes=files.value(f["file"].toString());Require(Sha(bytes)==f["sha256"].toString(),"Received configuration differs");Write(stage+'/'+name,bytes);hashes[name]=Sha(bytes);
 }
 const auto record=meta+"/configuration.json";QJsonObject previous;if(QFile::exists(record))previous=Parse(Read(record));
 const auto oldFiles=previous["files"].toObject();bool changed=false;for(auto it=oldFiles.begin();it!=oldFiles.end();++it)if(!hashes.contains(it.key()))changed=true;for(auto it=hashes.begin();it!=hashes.end();++it){const auto target=project+'/'+it.key();if(!QFileInfo::exists(target) || Sha(Read(target,16*1024*1024))!=it.value().toString())changed=true;}
 QJsonObject result{{"project_directory",project},{"release_id",release},{"cache_directory",stage},{"files",hashes},{"sync_enabled",enabled}};
 if(!enabled){QFile::remove(meta+"/pending-configuration.json");result["state"]="disabled";return result;}
 if(changed && !canApply){result["state"]="pending";result["message"]="Configuration received and cached. Return project ECCs to Idle or stop services before applying; no active files changed.";Write(meta+"/pending-configuration.json",Json(result));return result;}
 const auto backup=meta+"/configuration-backups/"+QString::number(QDateTime::currentMSecsSinceEpoch())+'-'+QString::fromLatin1(Token().left(8));
 // Preflight every destination before any replacement. Never traverse target links.
 for(auto it=hashes.begin();it!=hashes.end();++it){const auto target=project+'/'+it.key();auto parent=QFileInfo(target).dir();Require(!QFileInfo(target).isSymLink(),"Refusing a configuration destination link: "+target);while(parent.absolutePath()!=project){Require(parent.absolutePath().startsWith(project+'/') && !QFileInfo(parent.absolutePath()).isSymLink(),"Configuration destination escaped project folder");Require(parent.cdUp(),"Invalid configuration parent");}}
 for(auto it=hashes.begin();it!=hashes.end();++it){const auto target=project+'/'+it.key();if(QFileInfo::exists(target) && Sha(Read(target,16*1024*1024))==it.value().toString())continue;if(QFileInfo::exists(target))Write(backup+'/'+it.key(),Read(target,16*1024*1024));WorkspaceWrite(target,Read(stage+'/'+it.key(),16*1024*1024));}
 // Retire only files previously installed by this synchronizer, retaining backups.
 const auto old=previous["files"].toObject();for(auto it=old.begin();it!=old.end();++it)if(!hashes.contains(it.key())){const auto target=project+'/'+it.key();if(QFileInfo::exists(target) && !QFileInfo(target).isSymLink() && Sha(Read(target,16*1024*1024))==it.value().toString()){Write(backup+'/'+it.key(),Read(target,16*1024*1024));Require(QFile::remove(target),"Cannot retire old managed configuration");}}
 result["state"]="applied";result["changed"]=changed;Write(record,Json(result));QFile::remove(meta+"/pending-configuration.json");return result;
}
QJsonObject ReadServiceLog(const QString &path,qint64 offset,const QString &generation,bool fromEnd){
 QFileInfo info(path);Require(!info.isSymLink(),"Service log cannot be a link");if(!info.exists())return {{"offset",0},{"generation","missing"},{"text",""},{"available",false}};
 QFile file(path);Require(file.open(QIODevice::ReadOnly),"Cannot read service log");
 struct stat st;Require(!::stat(QFile::encodeName(path).constData(),&st),"Cannot inspect service log");const auto current=Sha((info.absoluteFilePath()+':'+QString::number(qulonglong(st.st_dev))+':'+QString::number(qulonglong(st.st_ino))+':'+QString::number(info.birthTime().toMSecsSinceEpoch())).toUtf8());
 if(fromEnd){const auto boundary=file.size();return {{"offset",double(boundary)},{"start_offset",double(boundary)},{"snapshot_size",double(boundary)},{"generation",current},{"text",""},{"reset",true},{"available",true},{"session_boundary",true}};}
 const bool reset=generation!=current || offset<0 || offset>file.size();if(reset)offset=qMax<qint64>(0,file.size()-65536);Require(file.seek(offset),"Cannot seek service log");const auto snapshot=file.size();const auto start=file.pos();const auto bytes=file.read(qMin<qint64>(65536,qMax<qint64>(0,snapshot-start)));
 return {{"offset",double(file.pos())},{"start_offset",double(start)},{"snapshot_size",double(snapshot)},{"generation",current},{"text",QString::fromUtf8(bytes)},{"reset",reset},{"available",true}};
}
}
