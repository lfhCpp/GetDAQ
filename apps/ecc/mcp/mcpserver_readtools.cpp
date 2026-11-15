#include "mcpserver_p.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <stdexcept>

namespace GetDAQControlMcpInternal {
QString Server::Call(const QString &name, const QJsonObject &arguments)
{
    if (name == "ecc_identity") { Require(arguments.isEmpty(), "ecc_identity accepts no arguments"); return QString::fromUtf8(Http("GET", "/v1/identity")); }
    if (name == "ecc_status") { Require(arguments.isEmpty(), "ecc_status accepts no arguments"); return QString::fromUtf8(Http("GET", "/v1/status")); }
    if (name == "ecc_cobo_lifecycles") return CoBoLifecycles(arguments);
    if (name == "ecc_hardware_state") { Require(arguments.isEmpty(), "ecc_hardware_state accepts no arguments"); return QString::fromUtf8(Http("GET", "/v1/hardware-state")); }
    if (name == "ecc_receivers") { Require(arguments.isEmpty(), "ecc_receivers accepts no arguments"); return QString::fromUtf8(Http("GET", "/v1/receivers")); }
    if (name == "ecc_operation") {
        const QString operationId = Text(arguments.value("operation_id"), "operation_id", 128);
        Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(operationId).hasMatch(), "Invalid operation_id");
        Require(arguments.size() == 1, "ecc_operation accepts only operation_id");
        return QString::fromUtf8(Http("GET", "/v1/operations/" + operationId));
    }
    if (name == "ecc_submit_operation") return Submit(arguments);
    if (name == "ecc_reprepare_cobo") return ReprepareCoBo(arguments);
    if (name == "ecc_configure_cobo") return ConfigureCoBo(arguments);
    if (name == "ecc_catalog_run") return Catalog(arguments);
    if (name == "ecc_resume_catalog_replacement") return ResumeCatalogReplacement(arguments);
    throw std::invalid_argument("Unknown or disabled MCP tool");
}

QJsonObject Server::CoBoTargets()
{
    QJsonParseError parse;
    const QJsonDocument identityDocument=QJsonDocument::fromJson(Http("GET","/v1/identity"),&parse);
    Require(parse.error==QJsonParseError::NoError && identityDocument.isObject(),"ECC identity response is invalid JSON");
    const auto identity=identityDocument.object();
    const QString instance=Text(identity.value("instance_id"),"identity.instance_id",64);
    const QString snapshot=Text(identity.value("snapshot_sha256"),"identity.snapshot_sha256",64);
    const QRegularExpression digestPattern("^[0-9a-f]{64}$");
    Require(digestPattern.match(instance).hasMatch() && digestPattern.match(snapshot).hasMatch() &&
            Text(identity.value("client_id"),"identity.client_id",128)==fSettings.clientId,
            "ECC identity differs from the fixed MCP service binding");
    const QJsonDocument targetsDocument=QJsonDocument::fromJson(Http("GET","/v1/cobo-maintenance-targets"),&parse);
    Require(parse.error==QJsonParseError::NoError && targetsDocument.isObject(),"CoBo maintenance targets response is invalid JSON");
    const auto targets=targetsDocument.object();
    Require(targets.size()==7 && targets.value("schema").toInt(-1)==2 &&
            targets.value("kind").toString()=="getdaq.ecc-cobo-maintenance-targets" &&
            targets.value("instance_id").toString()==instance && targets.value("snapshot_sha256").toString()==snapshot &&
            digestPattern.match(targets.value("configuration_sha256").toString()).hasMatch() &&
            digestPattern.match(targets.value("artifact_sha256").toString()).hasMatch(),
            "CoBo maintenance targets differ from the fixed ECC identity");
    const auto cobos=targets.value("cobos").toArray();
    Require(!cobos.isEmpty() && cobos.size()<=192,"Invalid active CoBo maintenance set");
    const QRegularExpression idPattern("^[A-Za-z0-9_-]{1,128}$");
    const QStringList states=QStringList()<<"unknown"<<"uninitialized"<<"described"<<"prepared"<<"configured"<<"running"<<"paused"<<"transitioning";
    const QStringList phases=QStringList()<<"idle"<<"accepted"<<"running"<<"complete"<<"failed"<<"unknown";
    QStringList seen;
    for(const auto &value:cobos) {
        Require(value.isObject(),"CoBo maintenance target must be an object");
        const auto target=value.toObject();
        Require(target.size()==6 && target.value("id").isString() && target.value("node_name").isString() &&
                target.value("lifecycle_state").isString() && target.value("operation_id").isString() &&
                target.value("operation").isString() && target.value("operation_phase").isString() &&
                idPattern.match(target.value("id").toString()).hasMatch() &&
                !target.value("node_name").toString().isEmpty() && target.value("node_name").toString().size()<=128 &&
                states.contains(target.value("lifecycle_state").toString()) && phases.contains(target.value("operation_phase").toString()),
                "Malformed CoBo maintenance target");
        const QString id=target.value("id").toString(),phase=target.value("operation_phase").toString();
        Require(!seen.contains(id),"Duplicate CoBo maintenance target");seen.append(id);
        Require((phase=="idle" && target.value("operation_id").toString().isEmpty() && target.value("operation").toString().isEmpty()) ||
                (phase!="idle" && idPattern.match(target.value("operation_id").toString()).hasMatch() &&
                 !target.value("operation").toString().isEmpty() && target.value("operation").toString().size()<=64),
                "CoBo maintenance operation evidence is malformed");
    }
    return targets;
}

QString Server::CoBoLifecycles(const QJsonObject &arguments)
{
    Require(arguments.isEmpty(),"ecc_cobo_lifecycles accepts no arguments");
    return QString::fromUtf8(QJsonDocument(CoBoTargets()).toJson(QJsonDocument::Compact));
}

} // namespace GetDAQControlMcpInternal
