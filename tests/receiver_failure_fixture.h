#ifndef GETDAQ_TEST_RECEIVER_FAILURE_FIXTURE_H
#define GETDAQ_TEST_RECEIVER_FAILURE_FIXTURE_H
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
namespace ReceiverFailureFixture {
// Metadata-only independent HTTP fixture; never kills processes or issues controls.
inline QByteArray Cache(const QByteArray &bound,const QByteArray &observation,
                        const QString &phase=QString("complete")) {
    auto state=QJsonDocument::fromJson(bound).object();
    auto observed=QJsonDocument::fromJson(observation).object();
    auto nodes=observed.value("nodes").toArray();auto node=nodes[0].toObject();
    auto detail=node.value("observation").toObject();auto processes=detail.value("receivers").toArray();
    auto child=processes[0].toObject();child["state"]="exited";child["exit_code"]=7;child["signal"]=0;
    processes[0]=child;detail["receivers"]=processes;node["observation"]=detail;nodes[0]=node;observed["nodes"]=nodes;
    QJsonObject evidence{{"bound_status",QString::fromUtf8(bound)},
        {"observation",QString::fromUtf8(QJsonDocument(observed).toJson(QJsonDocument::Compact))}};
    QJsonObject root{{"schema",1},{"kind","getdaq.ecc-receiver-failure"},
        {"instance_id",state.value("instance_id")},{"snapshot_sha256",state.value("snapshot_sha256")},
        {"enabled",true},{"latched",true},{"operation_id","internal-receiver-fixture"},{"phase",phase},{"evidence",evidence}};
    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
inline QByteArray SingleSourceCache(const QString &instance,const QString &snapshot,const QString &dataBinding) {
    const QString session(64,'d'),nodeInstance(64,'e'),plan(64,'f');
    QJsonObject child{{"id","C0"},{"session_id",session},{"output_directory","/fixture/session"},{"pid",42},{"exit_code",-1},
        {"reaped",false},{"seal_verified",false},{"received_bytes","4096"},{"frames","1"},{"files","1"}};
    QJsonObject lifecycle{{"schema",1},{"state","ready"},{"needs_reconciliation",false},{"plan_sha256",plan},{"session_id",session},
        {"message",""},{"receivers",QJsonArray{child}}};
    QJsonObject node{{"agent_id","A0"},{"instance_id",nodeInstance},{"plan_sha256",plan},{"lifecycle",lifecycle}};
    QJsonObject controller{{"state",5},{"transition",0},{"error",0},{"outcome_unknown",false},{"needs_reconciliation",false},{"operation_id","start"},{"message",""}};
    QJsonObject data{{"needs_reconciliation",false},{"controller_scope_sha256",snapshot},{"controller_epoch",session},{"message",""},{"nodes",QJsonArray{node}}};
    QJsonObject bound{{"schema",1},{"service","getdaq-ecc-server"},{"instance_id",instance},{"snapshot_sha256",snapshot},
        {"operation_active",false},{"operation_id","start"},{"client_epoch",""},{"control_needs_reconciliation",false},{"message",""},{"controller",controller},{"data",data}};
    QJsonObject process{{"id","C0"},{"pid",42},{"state","running"},{"exit_code",-1},{"signal",0},{"message",""}};
    QJsonObject detail{{"schema",1},{"busy",false},{"observed_at_unix_ms","1"},{"lifecycle",lifecycle},{"receivers",QJsonArray{process}}};
    QJsonObject observedNode{{"agent_id","A0"},{"instance_id",nodeInstance},{"plan_sha256",plan},{"available",true},{"message",""},{"observation",detail}};
    QJsonObject observation{{"schema",1},{"kind","getdaq.ecc-receivers"},{"instance_id",instance},{"snapshot_sha256",snapshot},
        {"data_binding_sha256",dataBinding},{"nodes",QJsonArray{observedNode}}};
    return Cache(QJsonDocument(bound).toJson(QJsonDocument::Compact),QJsonDocument(observation).toJson(QJsonDocument::Compact));
}
inline QByteArray Waiting(const QString &instance,const QString &snapshot) {
    return QJsonDocument(QJsonObject{{"schema",1},{"kind","getdaq.ecc-receiver-failure"},{"instance_id",instance},
        {"snapshot_sha256",snapshot},{"enabled",true},{"latched",false},{"operation_id",""},{"phase","watching"},{"evidence",QJsonValue()}})
        .toJson(QJsonDocument::Compact);
}
}
#endif
