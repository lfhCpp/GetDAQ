#ifndef GETDAQ_COBO_MAINTENANCE_FIXTURE_H
#define GETDAQ_COBO_MAINTENANCE_FIXTURE_H
#include "plan_transition_fixture.h"
namespace CoBoMaintenanceFixture {
inline QByteArray Status(int state=3) {
    auto body=QJsonDocument::fromJson(PlanTransitionFixture::Status()).object();auto controller=body["controller"].toObject();controller["state"]=state;body["controller"]=controller;
    return PlanTransitionFixture::Json(body);
}
inline QByteArray Targets(bool single=false) {
    using PlanTransitionFixture::H;
    QJsonArray cobos{QJsonObject{{"id","C0"},{"node_name","CoBo0"},{"lifecycle_state","prepared"},{"operation_id","prepare-1"},{"operation","prepare"},{"operation_phase","complete"}}};
    if(!single)cobos.append(QJsonObject{{"id","C1"},{"node_name","CoBo1"},{"lifecycle_state","prepared"},{"operation_id","prepare-1"},{"operation","prepare"},{"operation_phase","complete"}});
    return PlanTransitionFixture::Json({{"schema",2},{"kind","getdaq.ecc-cobo-maintenance-targets"},{"instance_id",H('a')},{"snapshot_sha256",H('b')},
        {"configuration_sha256",H('1')},{"artifact_sha256",H('2')},{"cobos",cobos}});
}
inline GetDAQ::EccControlOperation Complete(const std::string &target="C0",const std::string &command="reprepare-cobo") {
    GetDAQ::EccControlOperation operation;auto &request=operation.request;request.command=command;request.coboId=target;request.operationId="gui-maintenance";
    request.instanceId=std::string(64,'a');request.snapshotSha256=std::string(64,'b');request.clientId="test-console";request.clientEpoch=std::string(64,'d');
    operation.requestKey=GetDAQ::EccControlRequestKey(request);operation.phase="complete";
    GetDAQ::EccControlControllerStatus status;status.state=3;status.operationId=request.operationId;operation.resultJson=GetDAQ::EncodeEccControlControllerStatus(status);return operation;
}
}
#endif
