#include "mcpserver_p.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QRegularExpression>
#include <QStringList>
#include <stdexcept>

namespace GetDAQControlMcpInternal {
QString Server::Submit(const QJsonObject &arguments)
{
    Require(fSettings.allowControl, "Control tools are disabled by local MCP configuration");
    Require(arguments.size() == 3, "ecc_submit_operation accepts exactly command, operation_id and confirmation");
    const QString command = Text(arguments.value("command"), "command", 64);
    const QString operationId = Text(arguments.value("operation_id"), "operation_id", 128);
    const QString confirmation = Text(arguments.value("confirmation"), "confirmation", 256);
    Require(QStringList({"describe", "prepare", "configure", "start", "stop", "pause", "resume", "breakup", "reset"}).contains(command),
            "Lifecycle command is not permitted through MCP");
    Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(operationId).hasMatch(), "Invalid operation_id");
    Require(confirmation == "I_CONFIRM_" + command + "_" + operationId, "Confirmation does not match command and operation ID");

    return SubmitRequest(command, operationId, QJsonObject());
}

QString Server::ConfigureCoBo(const QJsonObject &arguments)
{
    Require(fSettings.allowControl,"Control tools are disabled by local MCP configuration");
    Require(arguments.size()==3,"ecc_configure_cobo accepts exactly cobo_id, operation_id and confirmation");
    const QString coboId=Text(arguments.value("cobo_id"),"cobo_id",128);
    const QString operationId=Text(arguments.value("operation_id"),"operation_id",128);
    const QString confirmation=Text(arguments.value("confirmation"),"confirmation",256);
    Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(coboId).hasMatch(),"Invalid cobo_id");
    Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(operationId).hasMatch(),"Invalid operation_id");
    Require(confirmation=="I_CONFIRM_configure-cobo_"+coboId+"_"+operationId,"Confirmation does not match selected CoBo and operation ID");
    QJsonParseError parse;const QJsonDocument statusDocument=QJsonDocument::fromJson(Http("GET","/v1/status"),&parse);
    Require(parse.error==QJsonParseError::NoError && statusDocument.isObject(),"ECC status response is invalid JSON");
    const QJsonObject status=statusDocument.object(),controller=status.value("controller").toObject();
    Require(!status.value("operation_active").toBool() && controller.value("state").toInt(-1)==3 &&
            controller.value("transition").toInt()==0 && controller.value("error").toInt()==0 &&
            !controller.value("outcome_unknown").toBool() && !controller.value("needs_reconciliation").toBool() &&
            !status.value("control_needs_reconciliation").toBool() && !status.value("data_needs_reconciliation").toBool(),
            "Selected CoBo Configure requires a healthy known Prepared system with no active operation");
    QJsonObject fields;fields.insert("cobo_id",coboId);
    return SubmitRequest("configure-cobo",operationId,fields);
}

QString Server::ReprepareCoBo(const QJsonObject &arguments)
{
    Require(fSettings.allowControl,"Control tools are disabled by local MCP configuration");
    Require(arguments.size()==3,"ecc_reprepare_cobo accepts exactly cobo_id, operation_id and confirmation");
    const QString coboId=Text(arguments.value("cobo_id"),"cobo_id",128);
    const QString operationId=Text(arguments.value("operation_id"),"operation_id",128);
    const QString confirmation=Text(arguments.value("confirmation"),"confirmation",256);
    const QRegularExpression idPattern("^[A-Za-z0-9_-]{1,128}$");
    Require(idPattern.match(coboId).hasMatch() && idPattern.match(operationId).hasMatch(),"Invalid CoBo or operation ID");
    Require(confirmation=="I_CONFIRM_reprepare-cobo_"+coboId+"_"+operationId,
            "Confirmation does not match selected CoBo and operation ID");
    const QJsonObject targets=CoBoTargets();
    const QString instance=targets.value("instance_id").toString(),snapshot=targets.value("snapshot_sha256").toString();
    bool selected=false;
    for(const auto &value:targets.value("cobos").toArray()) {
        const auto target=value.toObject();
        if(target.value("id").toString()==coboId &&
           (target.value("lifecycle_state").toString()=="prepared" || target.value("lifecycle_state").toString()=="configured"))
            selected=true;
    }
    Require(selected,"Reprepare requires an active CoBo with known Prepared or Configured lifecycle evidence");
    QJsonParseError parse;
    const QJsonDocument statusDocument=QJsonDocument::fromJson(Http("GET","/v1/status"),&parse);
    Require(parse.error==QJsonParseError::NoError && statusDocument.isObject(),"ECC status response is invalid JSON");
    const auto status=statusDocument.object(),controller=status.value("controller").toObject();
    Require(status.value("instance_id").toString()==instance && status.value("snapshot_sha256").toString()==snapshot &&
            !status.value("operation_active").toBool() && controller.value("state").toInt(-1)==3 &&
            controller.value("transition").toInt()==0 && controller.value("error").toInt()==0 &&
            !controller.value("outcome_unknown").toBool() && !controller.value("needs_reconciliation").toBool() &&
            !status.value("control_needs_reconciliation").toBool() && !status.value("data_needs_reconciliation").toBool(),
            "CoBo reprepare requires a healthy known Prepared system with stopped data processes");
    QJsonObject fields;fields.insert("cobo_id",coboId);
    return SubmitRequest("reprepare-cobo",operationId,fields);
}

QString Server::Catalog(const QJsonObject &arguments)
{
    Require(fSettings.allowControl, "Control tools are disabled by local MCP configuration");
    Require(arguments.size() == 5, "ecc_catalog_run requires experiment, run_number, replace_completed_run, operation_id and confirmation");
    const QString experiment=Text(arguments.value("experiment"),"experiment",80);
    Require(QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_-]{0,79}$").match(experiment).hasMatch(),
            "Invalid experiment name");
    const QJsonValue runValue=arguments.value("run_number");
    Require(runValue.isDouble() && runValue.toDouble()>=1 && runValue.toDouble()<2147483647 &&
            runValue.toDouble()==double(runValue.toInt(-1)), "run_number must be an integer in the supported range");
    const bool replace=arguments.value("replace_completed_run").toBool();
    Require(arguments.value("replace_completed_run").isBool(), "replace_completed_run must be a JSON boolean");
    const QString operationId=Text(arguments.value("operation_id"),"operation_id",128);
    const QString confirmation=Text(arguments.value("confirmation"),"confirmation",256);
    Require(QRegularExpression("^[A-Za-z0-9_-]+$").match(operationId).hasMatch(), "Invalid operation_id");
    Require(confirmation=="I_CONFIRM_catalog_"+operationId, "Confirmation does not match catalog and operation ID");
    QJsonObject fields; fields.insert("catalog_experiment",experiment); fields.insert("catalog_run_number",runValue.toInt());
    if(replace) fields.insert("catalog_replace_existing",true);
    return SubmitRequest("catalog",operationId,fields);
}

QString Server::ResumeCatalogReplacement(const QJsonObject &arguments)
{
    Require(fSettings.allowControl, "Control tools are disabled by local MCP configuration");
    Require(arguments.size()==3, "ecc_resume_catalog_replacement requires failed_operation_id, operation_id and confirmation");
    const QString failedId=Text(arguments.value("failed_operation_id"),"failed_operation_id",128);
    const QString operationId=Text(arguments.value("operation_id"),"operation_id",128);
    const QString confirmation=Text(arguments.value("confirmation"),"confirmation",384);
    const QRegularExpression idPattern("^[A-Za-z0-9_-]{1,128}$");
    Require(idPattern.match(failedId).hasMatch() && idPattern.match(operationId).hasMatch() && failedId!=operationId,
            "Invalid or reused catalog recovery operation ID");
    Require(confirmation=="I_CONFIRM_catalog_recovery_"+failedId+"_"+operationId,
            "Confirmation must bind the original failed catalog and new recovery operation ID");

    QJsonParseError parse;
    const auto identityDoc=QJsonDocument::fromJson(Http("GET","/v1/identity"),&parse);
    Require(parse.error==QJsonParseError::NoError && identityDoc.isObject(),"ECC identity response is invalid JSON");
    const auto identity=identityDoc.object();
    const QString instance=Text(identity.value("instance_id"),"identity.instance_id",64);
    const QString snapshot=Text(identity.value("snapshot_sha256"),"identity.snapshot_sha256",64);
    Require(Text(identity.value("client_id"),"identity.client_id",128)==fSettings.clientId,
            "MCP client_id does not match the fixed ECC service policy");
    const auto originalDoc=QJsonDocument::fromJson(Http("GET","/v1/operations/"+failedId),&parse);
    Require(parse.error==QJsonParseError::NoError && originalDoc.isObject(),"Original catalog operation response is invalid JSON");
    const auto original=originalDoc.object();
    Require(original.value("phase").toString()=="failed" && original.value("request").isObject(),
            "Recovery requires the exact terminal failed catalog operation");
    const auto request=original.value("request").toObject();
    Require(request.value("command").toString()=="catalog" && request.value("catalog_replace_existing").toBool() &&
            request.value("operation_id").toString()==failedId && request.value("instance_id").toString()==instance &&
            request.value("snapshot_sha256").toString()==snapshot && request.value("client_id").toString()==fSettings.clientId,
            "Original operation is not this client’s failed replacement request");
    const auto statusDoc=QJsonDocument::fromJson(Http("GET","/v1/status"),&parse);
    Require(parse.error==QJsonParseError::NoError && statusDoc.isObject(),"ECC status response is invalid JSON");
    const auto status=statusDoc.object();
    const auto controller=status.value("controller").toObject();
    Require(status.value("instance_id").toString()==instance && status.value("snapshot_sha256").toString()==snapshot &&
            status.value("client_epoch").toString()==request.value("client_epoch").toString() &&
            !status.value("operation_active").toBool() && controller.value("state").toInt(-1)==3 &&
            !controller.value("transition").toInt() && !controller.value("error").toInt() &&
            !controller.value("outcome_unknown").toBool() && !controller.value("needs_reconciliation").toBool(),
            "Catalog recovery requires the original live ECC owner and a known Prepared controller");
    QJsonObject fields; fields.insert("failed_operation_id",failedId);
    return SubmitRequest("resume-catalog-replacement",operationId,fields);
}

} // namespace GetDAQControlMcpInternal
