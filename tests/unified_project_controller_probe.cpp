#include "deploymentcontroller.h"
#include "originaldeployment.h"
#include <QCoreApplication>
#include <QDir>
#include <QJsonArray>
#include <QDebug>
#include <iostream>
using namespace Deployment;
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{
 Require(argc>=3,"probe ACTION MASTER.json [NODE_ID]");const QString action=argv[1];const auto master=Parse(Read(argv[2]));const auto root=master["root"].toString();auto project=Parse(Read(root+"/project.json"));const auto nodes=project["nodes"].toArray();QJsonObject result;
 if(action=="start" || action=="sync" || action=="services" || action=="start-selected" || action=="prepare-selected"){
  auto groups=project["groups"].toArray();QMap<QString,QByteArray> files;QJsonArray hosts;
  for(const auto &node:nodes)hosts.append(QJsonObject{{"id",node.toObject()["id"]}});
  for(int i=0;i<groups.size();++i){auto group=groups[i].toObject();group["repository_files"]=OriginalRepository(project["configuration_source"].toString(),group["id"].toString(),&files);groups[i]=group;}
  project["groups"]=groups;Write(root+"/project.json",Json(project));const auto bundle=BuildBundle(project["project_id"].toString("unified-test"),hosts,groups,files);
  if(action=="sync"){QSet<QString> targets;if(argc==4)targets.insert(argv[3]);result=SyncProject(master,nodes,bundle,targets);}
  else {QSet<QString> selection;if(action=="start-selected" || action=="prepare-selected")for(int i=3;i<argc;++i)selection.insert(argv[i]);const auto prepared=PrepareProject(master,nodes,bundle,{},QString(),false,selection);if(action=="prepare-selected"){std::cout<<Json(prepared).constData()<<std::endl;return 0;}const auto active=ActivateProject(master,nodes,prepared,{},QString(),selection);if(action=="services"){result={{"preparation",prepared},{"activation",active}};std::cout<<Json(result).constData()<<std::endl;return 0;}auto base=RunConfig::Default();base.outputDir=root+"/gui-records";auto config=ConsoleConfiguration(master,prepared,active,groups[0].toObject()["id"].toString(),base,selection);result={{"preparation",prepared},{"activation",active},{"configuration",config.filePath},{"snapshot",config.configurationSnapshot},{"nodes",config.nodes.size()}};Write(root+"/probe-start.json",Json(result));}
 }else if(action=="join-local"){
  Require(argc==4,"Supply local node.json");const auto config=Parse(Read(argv[3]));const auto local=EnsureLocalNode(config["root"].toString(),config["program_directory"].toString(),config["bind"].toString(),config["port"].toInt(),config["data_directory"].toString());const auto joined=JoinLocalNodeToProject(master,local);result={{"node_id",joined["id"]},{"paired",joined["paired"]}};
 }else if(action=="refresh"){
  const auto start=Parse(Read(root+"/probe-start.json"));RunConfig config;QString error;Require(config.Load(start["configuration"].toString(),&error),error);config=RefreshProjectRunConfiguration(config);result={{"snapshot",config.configurationSnapshot}};
 }else if(action=="stop"){
  const auto prepared=Parse(Read(root+"/prepared-release.json"));QJsonArray stopped;for(const auto &g:project["groups"].toArray())stopped.append(StopGroup(master,nodes,prepared,g.toObject()["id"].toString(),false));result["groups"]=stopped;
 }else Require(false,"Unknown probe action");
 std::cout<<Json(result).constData()<<std::endl;return 0;
}catch(const std::exception &e){qCritical().noquote()<<e.what();return 1;}}
