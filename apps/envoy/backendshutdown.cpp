#include "backendshutdown.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QSet>
#include <QThread>
#include <QLockFile>
#include <memory>
using namespace Deployment;
namespace {
bool Candidate(const QJsonObject &entry){
    if(entry["backend"]!="original-soap" || (entry["role"]!="ecc" && entry["role"]!="receiver"))return false;
    return !entry["actual_pids"].toArray().isEmpty() ||
        (entry["phase"]=="active" || entry["phase"]=="starting" || entry["phase"]=="startup-incomplete");
}
QString Key(const QJsonObject &node,const QJsonObject &instance){return node["id"].toString()+'/'+instance["instance_id"].toString();}
void Match(const QJsonObject &observed,const QJsonObject &selected){
    Require(observed["instance_id"]==selected["instance_id"] && observed["release_id"]==selected["release_id"] && observed["group_sha256"]==selected["group_sha256"],"Managed instance changed since the exit prompt; refresh before stopping it");
}
}
QJsonObject ManagedBackendExitPlan(const QString &projectFile,const QString &localNodeRoot){
    QJsonArray targets,errors,observedInstances;QMap<QString,QJsonObject> nodes;QSet<QString> groups;QString projectRoot;
    if(!projectFile.isEmpty())try{
        const auto project=Parse(Read(projectFile));projectRoot=QFileInfo(projectFile).absolutePath();
        for(const auto &value:project["groups"].toArray())groups.insert(value.toObject()["id"].toString());
        for(const auto &value:project["nodes"].toArray()){const auto node=value.toObject();nodes[node["id"].toString()]=node;}
    }catch(const std::exception &error){errors.append(QString::fromUtf8(error.what()));}
    QString localId;
    if(QFile::exists(localNodeRoot+"/node.json"))try{
        const auto c=Parse(Read(localNodeRoot+"/node.json"));localId=c["node_id"].toString();
        if(!localId.isEmpty())nodes[localId]=QJsonObject{{"id",localId},{"name","This computer"},{"url",QString("https://%1:%2").arg(c["bind"].toString()).arg(c["port"].toInt())},{"certificate_sha256",CertificateFingerprint(c["tls_certificate"].toString())},{"credential_file",c["credential_file"]}};
    }catch(const std::exception &error){errors.append(QString::fromUtf8(error.what()));}
    for(const auto &node:nodes)try{
        const auto inventory=NodeInventory(node);
        for(const auto &value:inventory["instances"].toArray()){
            const auto entry=value.toObject();
            if(node["id"]!=localId && !groups.contains(entry["group_id"].toString()))continue;
            observedInstances.append(QJsonObject{{"node",node},{"instance",entry}});
            if(Candidate(entry))targets.append(QJsonObject{{"node",node},{"instance",entry}});
        }
    }catch(const std::exception &error){errors.append(node["name"].toString()+": "+QString::fromUtf8(error.what()));}
    // After a partial normal shutdown, a stopped ECC remains valid evidence
    // for closing its receiver on a subsequent exit attempt.
    QSet<QString> receiverGroups,eccGroups;
    for(const auto &value:targets){const auto entry=value.toObject()["instance"].toObject();(entry["role"]=="ecc"?eccGroups:receiverGroups).insert(entry["group_id"].toString());}
    for(const auto &value:observedInstances){auto target=value.toObject();const auto entry=target["instance"].toObject();const auto group=entry["group_id"].toString();if(entry["backend"]=="original-soap" && entry["role"]=="ecc" && entry["phase"]=="stopped" && entry["stopped"].toBool() && entry["actual_pids"].toArray().isEmpty() && receiverGroups.contains(group) && !eccGroups.contains(group)){target["already_stopped"]=true;targets.append(target);eccGroups.insert(group);}}
    return {{"targets",targets},{"errors",errors},{"project_root",projectRoot}};
}
void StopManagedBackends(const QJsonObject &plan,const QString &auditRoot,const Logger &log){
    Require(plan["errors"].toArray().isEmpty(),"Some managed services cannot be inspected; keep them running or cancel exit");
    const auto projectRoot=plan["project_root"].toString();std::unique_ptr<QLockFile> lock;
    if(!projectRoot.isEmpty()){lock.reset(new QLockFile(projectRoot+"/deployment.lock"));Require(lock->tryLock(0),"A deployment operation is in progress; finish it before closing backend services");}
    const auto targets=plan["targets"].toArray();QSet<QString> confirmed;
    // Inspect every selected ECC before stopping any service. Node Stop repeats
    // the hardware-state check immediately before acting on its exact process.
    for(const auto &value:targets){const auto target=value.toObject(),entry=target["instance"].toObject(),node=target["node"].toObject();
        if(target["already_stopped"].toBool()){
            bool stopped=false;for(const auto &value:NodeInventory(node)["instances"].toArray()){const auto current=value.toObject();if(current["instance_id"]!=entry["instance_id"])continue;Match(current,entry);stopped=current["phase"]=="stopped" && current["stopped"].toBool() && current["actual_pids"].toArray().isEmpty();}
            Require(stopped,"Previously stopped ECC changed; refresh before stopping its receiver");continue;
        }
        const auto observed=Get(NodeEndpoint(node),"/v1/instances/"+Id(entry["instance_id"].toString())+"/status");Match(observed,entry);
        if(entry["role"]=="ecc"){
            const auto controller=observed["status"].toObject()["controller"].toObject();
            Require(controller.contains("state") && !controller["outcome_unknown"].toBool(),"Cannot confirm ECC acquisition state; keep backends running and inspect it");
            const int state=controller["state"].toInt(-1);Require(state>=0 && state!=5 && state!=6,"Stop the Run before closing ECC and data services");
        }
    }
    const auto directory=auditRoot+"/exit-"+QString::fromLatin1(Token().left(16));PrivateDirectory(directory);Write(directory+"/plan.json",Json(plan));
    for(const auto &role:{QString("ecc"),QString("receiver")})for(const auto &value:targets){
        const auto target=value.toObject(),entry=target["instance"].toObject(),node=target["node"].toObject();if(entry["role"]!=role)continue;
        if(target["already_stopped"].toBool()){confirmed.insert(entry["group_id"].toString());continue;}
        // A receiver may be on another host; require its group's ECC to have
        // stopped in this operation rather than inferring it from an old record.
        if(role=="receiver")Require(confirmed.contains(entry["group_id"].toString()),"No stopped ECC was confirmed for this receiver; use service management to inspect it");
        QJsonObject request{{"schema",1},{"operation_id","exit-"+QString::fromLatin1(Token().left(24))},{"action","stop"},{"release_id",entry["release_id"]},{"group_id",entry["group_id"]},{"role",role}};
        if(role=="receiver"){request["receiver_id"]=entry["receiver_id"];request["ecc_stopped_confirmed"]=true;}
        const auto file=directory+'/'+Sha(Key(node,entry).toUtf8())+".json";Write(file,Json({{"node_id",node["id"]},{"request",request},{"phase","intent"}}));
        const auto endpoint=NodeEndpoint(node);Post(endpoint,"/v1/operations",request);QJsonObject status;QElapsedTimer clock;clock.start();
        do{CheckOperation();status=Get(endpoint,"/v1/operations/"+request["operation_id"].toString());Require(status["request_sha256"]==Sha(Json(request)) && status["operation_id"]==request["operation_id"],"Exit operation response differs from its saved request");Write(file,Json({{"node_id",node["id"]},{"request",request},{"result",status}}));if(status["phase"]=="complete" || status["phase"]=="failed" || status["phase"]=="unknown")break;QThread::msleep(100);}while(clock.elapsed()<30000);
        Require(status["phase"]=="complete",status["message"].toString("Service stop has not completed; inspect its saved exit operation before retrying"));
        const auto result=status["result"].toObject();Match(result,entry);Require(result["phase"]=="stopped" && result["stopped"].toBool(),"Service did not confirm a normal stop");
        if(role=="ecc")confirmed.insert(entry["group_id"].toString());
        const auto registry=projectRoot+"/active-instances.json";
        if(!projectRoot.isEmpty() && QFile::exists(registry)){auto active=Parse(Read(registry));const auto previous=active[entry["instance_id"].toString()].toObject();if(previous["node_id"]==node["id"] && previous["release_id"]==entry["release_id"]){active[entry["instance_id"].toString()]=result;Write(registry,Json(active));}}
        if(log)log("Stopped managed "+role+": "+entry["instance_id"].toString());
    }
}
