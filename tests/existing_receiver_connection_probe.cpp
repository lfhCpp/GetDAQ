#include "existingeccconnection.h"
#include "deploymentcommon.h"
#include "runcontrol.h"
#include "protocol.h"
#include <QCoreApplication>
#include <QEventLoop>
#include <QTemporaryDir>
#include <QTimer>
#include <QDebug>
int main(int argc,char **argv){QCoreApplication app(argc,argv);try{
 Deployment::Require(argc==4,"Pass existing SOAP ECC port, data receiver IP and data port");QTemporaryDir temp;RunConfig base=RunConfig::Default();base.configDir=temp.path();base.outputDir=temp.path()+"/records";
 const auto checked=CheckExistingEcc("Existing ECC","127.0.0.1",QString(argv[1]).toInt(),base,{});auto c=checked.configuration;c.nodes[0].routerHost=argv[2];c.nodes[0].routerPort=QString(argv[3]).toInt();c.nodes[0].exporterPort=0;
 Deployment::Require(checked.protocol=="soap" && !c.nativeEcc,"Existing service must not request local backend startup");
 const auto request=Protocol::EccRequest(c.nodes[0],"pedestals","Configure");Deployment::Require(request.contains("DataRouter") && request.contains(argv[2]) && request.contains(argv[3]) && !request.contains("DataExporter"),"Legacy receiver address and TCP port are passed to ECC without an exporter requirement");
 RunControl control;QEventLoop loop;bool observed=false;QObject::connect(&control,&RunControl::StatusSignal,&loop,[&](const QVector<NodeStatus> &nodes){if(!nodes.isEmpty() && nodes[0].eccOnline && nodes[0].state==1){observed=true;loop.quit();}});QTimer::singleShot(5000,&loop,&QEventLoop::quit);control.ConnectSlot(c);loop.exec();control.DisconnectSlot();Deployment::Require(observed,"Existing ECC actual Idle not observed");qInfo()<<"PASS existing SOAP ECC + external DataRouter endpoint; no DataExporter startup required";return 0;
 }catch(const std::exception &e){qCritical().noquote()<<e.what();return 1;}}
