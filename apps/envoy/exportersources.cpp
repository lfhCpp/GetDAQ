#include "exportersources.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
QJsonArray ExporterSources(const RunConfig &configuration,const QVector<NodeStatus> &observations){
 QJsonArray result;QSet<QString> seen;
 auto add=[&](QString label,QString host,int port){if(host.isEmpty() || host=="0.0.0.0" || port<1 || port>65535)return;const auto endpoint=host+':'+QString::number(port);if(seen.contains(endpoint))return;seen.insert(endpoint);result.append(QJsonObject{{"id",endpoint},{"label",label+" — "+endpoint},{"endpoint",endpoint}});};
 QFile file(configuration.deploymentProject);QJsonObject project;if(file.open(QIODevice::ReadOnly) && file.size()<=16*1024*1024)project=QJsonDocument::fromJson(file.readAll()).object();
 for(const auto &entry:project["groups"].toArray()){
  const auto group=entry.toObject(),ecc=group["ecc"].toObject();bool selected=false;for(const auto &node:configuration.nodes)if(node.host==ecc["address"].toString() && node.eccPort==ecc["port"].toInt())selected=true;
  if(!selected || group["receiver_backend"].toString("DataExporter")!="DataExporter")continue;
  for(const auto &receiverValue:group["receivers"].toArray()){
   const auto receiver=receiverValue.toObject();QString host=group["receiver_ip"].toString(receiver["address"].toString());int port=group["export_port"].toInt();QString state="configured";
   for(const auto &hostValue:project["nodes"].toArray()){const auto node=hostValue.toObject();if(node["id"]!=receiver["node_id"])continue;
    for(const auto &instanceValue:node["inventory"].toObject()["instances"].toArray()){const auto instance=instanceValue.toObject();if(instance["group_id"]==group["id"] && instance["receiver_id"]==receiver["id"] && !instance["actual_pids"].toArray().isEmpty() && instance["preview_port"].toInt()>0){host=instance["preview_host"].toString(host);port=instance["preview_port"].toInt();state="last observed running";}}
   }
   add(group["name"].toString()+" / "+receiver["id"].toString()+" ("+state+")",host,port);
  }
 }
 for(const auto &node:configuration.nodes){if(node.mutant)continue;int port=node.previewPort;bool detected=false;if(!port)for(const auto &state:observations)if(state.id==node.id && state.sentryOnline && state.process.contains("DataExporter",Qt::CaseInsensitive)){port=46491;detected=true;}if(port>0)add(node.name+(detected?" (detected DataExporter; default export port)":" (configured DataExporter)"),node.previewHost.isEmpty()?(node.routerHost.isEmpty()?node.host:node.routerHost):node.previewHost,port);}
 return result;
}
