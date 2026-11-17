#include "originaldeployment.h"
#include "distributedproject.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>
#include <QJsonArray>
#include <QDebug>
using namespace Deployment;
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{
 QTemporaryDir temp;Require(temp.isValid(),"temporary directory");const auto repo=temp.path()+"/repo";QDir().mkpath(repo);
 for(const auto &name:{"describe-cobo0.xcfg","prepare-pedestals.xcfg","configure-pedestals.xcfg"}){QFile f(repo+'/'+name);Require(f.open(QIODevice::WriteOnly),"fixture");f.write("<config/>");}
 QMap<QString,QByteArray> files;QJsonObject g{{"id","g1"},{"backend","original-soap"},{"name","Original"},{"experiment","pedestals"},{"master_repository",repo},{"board_index",0},{"receiver_ip","127.0.0.1"},{"data_port",46005},{"export_port",46491},{"sentry_port",8080},{"ecc",QJsonObject{{"node_id","host"},{"address","127.0.0.1"},{"port",8083},{"callback_port",46011}}},{"receivers",QJsonArray{QJsonObject{{"id","data"},{"node_id","host"},{"address","127.0.0.1"},{"port",46004}}}}};
 g["repository_files"]=OriginalRepository(repo,"g1",&files);ValidateOriginalFiles(g,files);const auto bundle=BuildBundle("project",QJsonArray{QJsonObject{{"id","host"}}},QJsonArray{g},files);Require(!bundle.objects.isEmpty(),"published original bytes");
 const auto root=temp.path()+"/master";PrivateDirectory(root);auto c=OriginalConsole({{"root",root}},g,{{"release_id",QString(64,'a')}},RunConfig::Default());Require(!c.nativeEcc && !c.simulation && c.nodes.size()==1,"real SOAP console");Require(c.nodes[0].ConfigId("describe",c.experiment)=="cobo0" && c.nodes[0].ConfigId("prepare",c.experiment)=="pedestals" && c.nodes[0].ConfigId("configure",c.experiment)=="pedestals","actual original configuration IDs");Require(c.nodes[0].routerPort==46005 && c.nodes[0].sentryPort==8080,"distinct data and monitor ports");RunConfig loaded;QString error;Require(loaded.Load(c.filePath,&error),error);Require(!loaded.nativeEcc && loaded.nodes[0].describeId=="cobo0","saved profile maps to original files");
 auto bad=g;auto rxs=bad["receivers"].toArray();auto rx=rxs[0].toObject();rx["port"]=8083;rxs[0]=rx;bad["receivers"]=rxs;bool rejected=false;try{BuildBundle("project",QJsonArray{QJsonObject{{"id","host"}}},QJsonArray{bad},files);}catch(...){rejected=true;}Require(rejected,"same-host port collision rejected");
 auto entries=g["repository_files"].toArray();auto f=entries[0].toObject();files[f["file"].toString()]="<config include=\"/master-only/file.xcfg\"/>";f["sha256"]=Sha(files[f["file"].toString()]);entries[0]=f;bad=g;bad["repository_files"]=entries;rejected=false;try{ValidateOriginalFiles(bad,files);}catch(...){rejected=true;}Require(rejected,"nonportable include diagnosed before publication");
 qInfo()<<"PASS original repository, Describe fallback, SOAP YAML, port collision and portable include checks";return 0;
 }catch(const std::exception &e){qCritical().noquote()<<e.what();return 1;}}
