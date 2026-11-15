#ifndef GETDAQ_PLAN_TRANSITION_FIXTURE_H
#define GETDAQ_PLAN_TRANSITION_FIXTURE_H
#include "eccprotocol.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
namespace PlanTransitionFixture {
inline QString H(char value) {return QString(64,QLatin1Char(value));}
inline QByteArray Json(const QJsonObject &value) {return QJsonDocument(value).toJson(QJsonDocument::Compact);}
inline QByteArray Identity() {
    return Json({{"schema",1},{"service","getdaq-ecc-server"},{"service_id","test-ecc"},{"client_id","test-console"},
        {"instance_id",H('a')},{"snapshot_sha256",H('b')},{"input_binding_sha256",H('c')},{"configuration_sha256",H('1')},{"artifact_sha256",H('2')},{"data_binding_sha256",H('3')}});
}
inline QByteArray Status(bool retired=false) {
    QJsonObject receiver{{"id","C0"},{"session_id",H('9')},{"output_directory","/data/original"},{"pid",1234},{"exit_code",0},{"reaped",true},{"seal_verified",true},{"received_bytes","4096"},{"frames","1"},{"files","1"}};
    QJsonObject life{{"schema",1},{"state","stopped"},{"needs_reconciliation",false},{"plan_sha256",H('8')},{"session_id",H('9')},{"message",""},{"receivers",QJsonArray{receiver}}};
    QJsonObject node{{"agent_id","node-0"},{"instance_id",H('7')},{"plan_sha256",H('8')},{"lifecycle",life}};
    QJsonObject data{{"needs_reconciliation",retired},{"controller_scope_sha256",H('5')},{"controller_epoch",H('6')},{"message",""},{"nodes",QJsonArray{node}}};
    QJsonObject controller{{"state",1},{"transition",0},{"error",0},{"outcome_unknown",false},{"needs_reconciliation",retired},{"operation_id","old-reset"},{"message",retired ? "Data lifecycle requires reconciliation" : ""}};
    return Json({{"schema",1},{"service","getdaq-ecc-server"},{"instance_id",H('a')},{"snapshot_sha256",H('b')},{"operation_active",false},{"operation_id","old-reset"},
        {"client_epoch",H('d')},{"control_needs_reconciliation",retired},{"message",retired ? "Original plans retired" : ""},{"controller",controller},{"data",data}});
}
inline QByteArray Successor() {
    const QJsonObject value{{"schema",1},{"kind","getdaq.ecc-successor-plan"},{"plan_file","/host/reviewed/successor.json"},{"service_id","next-ecc"},{"controller_id","native-source-controller"},
        {"input_binding_sha256",H('c')},{"snapshot_sha256",H('e')},{"data_lifecycle_binding_sha256",H('2')},{"data_controller_scope_sha256",H('f')},{"audit_root","/host/new-controller"},{"receiver_controller_epoch",H('1')}};
    QByteArray bytes="{\"schema\":1,\"kind\":\"getdaq.ecc-successor-plan\"";
    for(const auto &key:QStringList{"plan_file","service_id","controller_id","input_binding_sha256","snapshot_sha256","data_lifecycle_binding_sha256","data_controller_scope_sha256","audit_root","receiver_controller_epoch"}) {
        auto encoded=QJsonDocument(QJsonArray{value.value(key)}).toJson(QJsonDocument::Compact);encoded=encoded.mid(1,encoded.size()-2);bytes+=",\""+key.toUtf8()+"\":"+encoded;
    }
    return bytes+"}\n";
}
inline QByteArray Targets() {
    return Json({{"schema",1},{"kind","getdaq.reviewed-data-plan-targets"},{"plans",QJsonArray{QJsonObject{{"agent_id","node-0"},{"plan_sha256",H('4')},{"plan_file","/data/reviewed/next-plan.json"}}}}});
}
inline GetDAQ::EccControlRequest Request(const std::string &operation="reviewed-retire") {
    const auto review=GetDAQ::ReviewEccPlanTransition(Successor().toStdString(),Targets().toStdString());
    GetDAQ::EccControlRequest request;request.command="retire-data-plans";request.operationId=operation;request.clientId="test-console";
    request.instanceId=H('a').toStdString();request.snapshotSha256=H('b').toStdString();request.clientEpoch=H('d').toStdString();
    request.successorPlanFile=review.successor.planFile;request.successorPlanSha256=review.successorSha256;request.successorScopeSha256=review.successor.scopeSha256;
    request.successorEpoch=review.successor.epoch;request.successorDataBindingSha256=review.successor.dataBindingSha256;request.successorReceiverPlans=review.targets;return request;
}
inline GetDAQ::EccControlRequest Completion(const GetDAQ::EccControlRequest &original,const std::string &id="complete-retirement") {
    GetDAQ::EccControlRequest request;request.command="complete-data-handoff";request.operationId=id;request.failedOperationId=original.operationId;
    request.instanceId=original.instanceId;request.snapshotSha256=original.snapshotSha256;request.clientId=original.clientId;request.clientEpoch=original.clientEpoch;return request;
}
inline std::string Operation(const GetDAQ::EccControlRequest &request,const std::string &phase) {
    GetDAQ::EccControlOperation operation;operation.request=request;operation.requestKey=GetDAQ::EccControlRequestKey(request);operation.phase=phase;
    if(phase=="complete" || phase=="failed") {
        GetDAQ::EccControlControllerStatus status;status.state=1;status.operationId="old-reset";status.needsReconciliation=true;
        operation.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);
    }
    return GetDAQ::EncodeEccControlOperation(operation);
}
inline QByteArray Receipt(const GetDAQ::EccControlRequest &request) {
    const auto directory=QString("/host/controller/operations/")+QString::fromStdString(request.operationId)+"/data-handoff";
    const QList<QPair<QString,QString>> fields={{"operation_id","node-retire"},{"command","retire-plan"},{"plan_sha256",H('8')},{"instance_id",H('7')},{"controller_id","native-source-controller"},
        {"controller_snapshot_sha256",H('5')},{"controller_epoch",H('6')},{"configuration_sha256",H('1')},{"artifact_sha256",H('2')},{"parent_audit_reference",directory},
        {"successor_controller_snapshot_sha256",QString::fromStdString(request.successorScopeSha256)},{"successor_controller_epoch",QString::fromStdString(request.successorEpoch)},
        {"successor_plan_file",QString::fromStdString(request.successorReceiverPlans[0].planFile)},{"successor_plan_sha256",QString::fromStdString(request.successorReceiverPlans[0].planSha256)},{"expected_session_id",H('9')}};
    QByteArray child="{\"schema\":1";
    for(const auto &field:fields) {
        auto encoded=QJsonDocument(QJsonArray{field.second}).toJson(QJsonDocument::Compact);encoded=encoded.mid(1,encoded.size()-2);
        child+=",\""+field.first.toUtf8()+"\":"+encoded;
    }
    child+="}\n";
    const auto key=QString::fromLatin1(QCryptographicHash::hash("getdaq.supervisor-request:1\n"+child,QCryptographicHash::Sha256).toHex());
    QJsonObject node{{"agent_id","node-0"},{"instance_id",H('7')},{"operation_id","node-retire"},{"request_key",key},{"proof_sha256",H('3')},{"request",QString::fromUtf8(child)}};
    return Json({{"schema",1},{"kind","getdaq.remote-plan-retirement"},{"original_scope_sha256",H('5')},{"original_epoch",H('6')},
        {"successor_scope_sha256",QString::fromStdString(request.successorScopeSha256)},{"successor_epoch",QString::fromStdString(request.successorEpoch)},
        {"audit_directory",directory},{"nodes",QJsonArray{node}}});
}
}
#endif
