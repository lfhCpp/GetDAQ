#include "deploymentpaths.h"
#include <QStringList>
#include <QDir>
QString DeploymentPathSummary(const QJsonObject &group,const QJsonArray &hosts,const QString &source,const QString &records){
 if(group.isEmpty())return "Select an ECC group to see its configuration and data folders.";
 QStringList lines{"Source XML/XCFG (master, current setting): "+source};
 auto assignment=[&](const QJsonObject &target,bool receiver){for(const auto &value:hosts){const auto host=value.toObject();if(host["id"]!=target["node_id"])continue;const auto name=host["name"].toString();const auto inventory=host["inventory"].toObject();QString folder=group["project_directories"].toObject()[host["id"].toString()].toString();if(folder.isEmpty() && group["project_mode"].toBool())folder=inventory["data_directory"].toString();if(!group["project_mode"].toBool() && receiver && !inventory["data_directory"].toString().isEmpty())folder=QDir(inventory["data_directory"].toString()).filePath(group["id"].toString()+'-'+target["id"].toString());
  const auto label=receiver?QString("Raw .graw / receiver working folder"):QString("ECC configuration working folder");
  lines<<label+" on "+name+" (current setting): "+(folder.isEmpty()?QString("automatic; refresh after preparation"):folder);
  if(receiver && !folder.isEmpty())lines<<"After successful Sentry archive: "+QDir(folder).filePath(group["experiment"].toString()+"/run_NNNN");
  for(const auto &entry:inventory["instances"].toArray()){const auto instance=entry.toObject();if(instance["group_id"]!=group["id"] || instance["role"]!=(receiver?QString("receiver"):QString("ecc")))continue;if(receiver && instance["receiver_id"]!=target["id"])continue;
   const auto observed=instance[receiver?"data_directory":"configuration_repository"].toString();if(observed.isEmpty() || observed==folder)continue;const bool running=!instance["actual_pids"].toArray().isEmpty();lines<<(running?"Last observed running folder on ":"Last prepared folder on ")+name+": "+observed;
   if(running)lines<<"Folder change pending: normally stop and restart this group's services to apply the current setting. Existing raw files are not moved.";
  }
 }};
 assignment(group["ecc"].toObject(),false);for(const auto &target:group["receivers"].toArray())assignment(target.toObject(),true);
 lines<<"GUI records and logs (YAML / journal / CSV / backups / logs, not raw data): "+records;return lines.join('\n');
}
