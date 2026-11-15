#include "earlywarningrecovery.h"
#include "hardwaredeployment.h"
#include "localserviceprocesses.h"
#include "eccsystemclient.h"
#include "httpclient.h"
#include "eccprotocol.h"
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonArray>
#include <QDateTime>
#include <QTimer>
#include <QRegularExpression>
#include <stdexcept>
namespace {
void Require(bool value,const QString &message){if(!value)throw std::runtime_error(message.toStdString());}
QJsonObject Json(const QString &path){QJsonParseError error;const auto doc=QJsonDocument::fromJson(ReadHardwareSetupFile(path),&error);Require(error.error==QJsonParseError::NoError && doc.isObject(),"Invalid recovery metadata: "+path);return doc.object();}
QJsonArray Lines(const QString &path){QJsonArray result;for(const auto &line:ReadHardwareSetupFile(path).split('\n'))if(!line.trimmed().isEmpty()){const auto doc=QJsonDocument::fromJson(line);Require(doc.isObject(),"Invalid recovery journal: "+path);result.append(doc.object());}return result;}
}
QJsonObject ValidateEarlyDelayWarningEvidence(const QString &directory,const QByteArray &status) {
    const auto state=GetDAQ::DecodeEccControlSystemStatus(status.toStdString());
    const bool emptyIdle=!state.operationActive && (state.controller.state==1 || state.controller.state==2) && state.controller.error==0 && !state.controller.outcomeUnknown;
    Require(emptyIdle || (!state.operationActive && state.controller.state==2 && state.controller.outcomeUnknown && state.controller.needsReconciliation),"Recovery requires a known empty Idle baseline or the original early compatibility warning, with no active operation");
    const auto failed=QString::fromStdString(state.controller.operationId);
    const auto ecc=Json(directory+"/ecc.json"),topology=Json(ecc["topology"].toObject()["path"].toString());
    const auto boards=topology["cobos"].toArray();Require(boards.size()==1 && boards[0].toObject()["enabled"].toBool() && topology["mutants"].toArray().isEmpty(),"This narrow recovery supports one CoBo without MuTaNT only");
    if(emptyIdle) {
        const auto journal=ecc["audit_root"].toString()+"/ecc.jsonl";QJsonObject hashes{{journal,HardwareSetupSha256(ReadHardwareSetupFile(journal))}};
        return {{"schema",1},{"kind","getdaq.empty-idle-baseline"},{"original_directory",directory},{"original_instance",QString::fromStdString(state.instanceId)},
            {"original_snapshot",QString::fromStdString(state.snapshotSha256)},{"original_status_sha256",HardwareSetupSha256(status)},{"original_files",hashes},{"board",boards[0].toObject()},{"requires_compatibility_warning",false},{"previous_known_state",int(state.controller.state)}};
    }
    Require(QRegularExpression("^[A-Za-z0-9_.-]{1,200}$").match(failed).hasMatch() && failed.endsWith("-prepare"),"Original failed Prepare identity is unavailable");
    const auto root=ecc["audit_root"].toString()+"/operations/"+failed+"/prepare";
    const auto before=root+"/cobo-0/before-subscription.jsonl";
    const auto rows=Lines(before);Require(rows.size()>2,"Missing failed hardware plan evidence");
    const auto plan=rows[0].toObject()["steps"].toArray();const auto last=rows.last().toObject();const int index=last["step"].toInt(-1);
    Require(last["phase"]=="mismatch" && last["actual"]=="255" && index>0 && index<plan.size(),"Failure is not the accepted readDataDelay=255 warning");
    const auto expected=plan[index].toObject(),write=plan[index-1].toObject();
    const auto field=expected["field"].toString();
    Require(expected["operation"]=="expect" && expected["mask"]=="255" && expected["device"]=="ctrl" && expected["register"]=="readDataDelay" && QRegularExpression("^AGET[0-3]$").match(field).hasMatch() &&
        write["operation"]=="write" && write["device"]=="ctrl" && write["register"]=="readDataDelay" && write["field"]==field && write["value"]==expected["value"],"Original mismatch is outside the allowed compatibility warning");
    bool acknowledged=false;
    for(int i=1;i<rows.size();++i){const auto row=rows[i].toObject();if(row["step"].toInt(-1)==index-1 && row["phase"]=="acknowledged")acknowledged=true;if(row["phase"]=="unknown" || row["phase"]=="read_failed")throw std::runtime_error("Transport/write uncertainty cannot be accepted as a compatibility warning");}
    Require(acknowledged,"The original delay write was not acknowledged");
    const auto alarms=root+"/cobo-0/alarms.jsonl";const auto alarmRows=Lines(alarms);
    Require(alarmRows.size()==1 && alarmRows[0].toObject()["phase"]=="listener_prepared","An active or unknown subscription requires the normal recovery workflow");
    const auto prepareFile=root+"/cobo-0/prepare.jsonl";const auto prepare=Lines(prepareFile);
    Require(prepare.size()==3 && prepare.last().toObject()["phase"]=="stopped" && prepare.last().toObject()["full_system_prepared"]==false,"Prepare progressed beyond the bounded early-warning stage");
    const auto systemFile=root+"/system.jsonl";const auto system=Lines(systemFile);
    Require(!system.isEmpty() && system.last().toObject()["phase"]=="failed","The original system failure is not terminal");
    for(const auto &v:system){const auto row=v.toObject();const auto phase=row["phase"].toString();Require(phase!="cobo_prepare_complete" && phase!="mutant_prepare_begin","Another prepared board requires its original cleanup workflow");}
    QJsonObject hashes;for(const auto &file:{before,alarms,prepareFile,systemFile})hashes[file]=HardwareSetupSha256(ReadHardwareSetupFile(file));
    return {{"schema",1},{"kind","getdaq.accepted-early-warning-baseline"},{"failed_operation_id",failed},{"original_directory",directory},{"original_instance",QString::fromStdString(state.instanceId)},
        {"original_snapshot",QString::fromStdString(state.snapshotSha256)},{"original_status_sha256",HardwareSetupSha256(status)},{"failure_field","ctrl.readDataDelay."+field},{"expected",expected["value"]},{"actual","255"},{"original_files",hashes},{"board",boards[0].toObject()},{"requires_compatibility_warning",true}};
}
EarlyWarningRecovery::EarlyWarningRecovery(const RunConfig &config,const QString &programs,QObject *parent):QObject(parent),fConfig(config),fPrograms(programs),fDirectory(QFileInfo(config.filePath).absolutePath()),fClient(new EccSystemClient(config.nativeClient,{},this)),fHttp(new HttpClient(this)),fProcess(new QProcess(this)) {
    connect(fClient,&EccSystemClient::IdentitySignal,this,[this](bool matched,QByteArray,QString error){if(!matched){Fail(error);return;}fClient->QueryStatus();});
    connect(fClient,&EccSystemClient::StatusSignal,this,[this](QByteArray body,QString error){
        if(fDone)return;
        if(!error.isEmpty()){Fail(error);return;}
        try {
            fProof=ValidateEarlyDelayWarningEvidence(fDirectory,body);fBoard=fProof["board"].toObject();
            Require(FindLocalReceiverProcesses(QDir(fPrograms).filePath("getdaq-receiver"),fDirectory+"/supervisor.json").isEmpty(),"A receiver is still running; finish/repair its session first");
            const auto supervisor=Json(fDirectory+"/supervisor.json");const auto token=ReadHardwareSetupFile(supervisor["credential_file"].toString(),1024).trimmed();
            const auto url=QUrl(QString("http://127.0.0.1:%1/v1/status").arg(supervisor["port"].toInt()));
            fHttp->Request(url,{},"application/json",3000,[this](const QByteArray &reply,const QString &failure){
                if(!failure.isEmpty()){Fail(failure);return;}
                const auto o=QJsonDocument::fromJson(reply).object(),lifecycle=o["lifecycle"].toObject();
                if(o["operation_active"].toBool(true) || !lifecycle.contains("receivers") || !lifecycle["receivers"].toArray().isEmpty() || !lifecycle["session_id"].toString().isEmpty()){Fail("An existing receiver session requires normal data recovery");return;}
                fProof["supervisor_status_sha256"]=HardwareSetupSha256(reply);fReadIndex=-1;ReadNext();
            },{{"Authorization","Bearer "+token}});
        }catch(const std::exception &e){Fail(QString::fromUtf8(e.what()));}
    });
    connect(fProcess,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        if(fDone)return;
        const auto body=fProcess->readAllStandardOutput();const auto error=fProcess->readAllStandardError();
        if(code || status!=QProcess::NormalExit){Fail(QString::fromLocal8Bit(error).left(2000));return;}
        if(fReadIndex<0) {
            const auto plan=QJsonDocument::fromJson(body).object();
            if(plan["snapshot_sha256"].toString()!=fConfig.nativeClient.snapshotSha256 || plan["input_binding_sha256"].toString()!=fConfig.nativeClient.inputBindingSha256){Fail("Current files differ from the original live ECC binding; no reset is allowed");return;}
            fProof["original_checked_plan_sha256"]=HardwareSetupSha256(body);fReadIndex=0;ReadNext();return;
        }
        const auto value=QJsonDocument::fromJson(body).object()["value"].toString();bool ok=false;const auto actual=value.toULongLong(&ok);
        if(!ok){Fail("Hardware read did not return an integer");return;}
        const QStringList keys={"pipeCtrl.firmwareEnable","pipeCtrl.intEnable","mutantConfig.initDone","asadEnable","hardwareVersion",fProof["failure_field"].toString().mid(5)};
        const auto key=keys[fReadIndex];
        if((fReadIndex<4 && actual!=0) || (fReadIndex==4 && actual!=5) || (fReadIndex==5 && actual!=255)){Fail(tr("Read-only recovery check rejected %1 = %2").arg(key,value));return;}
        fReads[key]=value;emit Log(tr("Recovery check: %1 = %2").arg(key,value));++fReadIndex;ReadNext();
    });
    connect(fProcess,&QProcess::errorOccurred,this,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)Fail(fProcess->errorString());});
}
void EarlyWarningRecovery::Start(){emit Log(tr("Checking the original early Prepare failure and current stopped/unpowered hardware read-only…"));fClient->QueryIdentity();QTimer::singleShot(30000,this,[this]{if(!fDone){fProcess->kill();Fail("Recovery verification timed out; nothing was reset");}});}
void EarlyWarningRecovery::Fail(const QString &message){if(fDone)return;fDone=true;emit Failed(message);}
void EarlyWarningRecovery::ReadNext(){
    if(fDone)return;
    if(fReadIndex<0){fProcess->start(QDir(fPrograms).filePath("getdaq-ecc-server"),{"--config",fDirectory+"/ecc.json","--plan-config"});return;}
    if(fReadIndex>=(fProof["requires_compatibility_warning"].toBool()?6:5)){fDone=true;fProof["hardware_reads"]=fReads;fProof["verified_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);fProof["operator_accepted_warning"]=fProof["requires_compatibility_warning"];emit Verified(fProof);return;}
    const QStringList registers={"pipeCtrl","pipeCtrl","mutantConfig","asadEnable","hardwareVersion","readDataDelay"},fields={"firmwareEnable","intEnable","initDone","","",fProof["failure_field"].toString().section('.',-1)};
    QStringList arguments{"--description",fBoard["hardware_description_file"].toString(),fields[fReadIndex].isEmpty()?"read-register":"read-field",fBoard["node_proxy"].toString(),"ctrl",registers[fReadIndex]};
    if(!fields[fReadIndex].isEmpty())arguments.append(fields[fReadIndex]);
    fProcess->start(QDir(fPrograms).filePath("getdaq-hardware"),arguments);
}
