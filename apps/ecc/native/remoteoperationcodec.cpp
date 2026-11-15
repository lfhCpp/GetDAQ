#include "remoteoperationcodec.h"

#include "eccprotocol.h"
#include "planio.h"
#include "supervisorio.h"

#include <set>
#include <stdexcept>
#include <string>

namespace GetDAQ {
namespace RemoteOperationCodec {
using namespace SupervisorIO;

SupervisorControlRequest DecodeRequestValue(const JsonValue &value) {
    if (value.type!=JsonValue::Object) throw std::runtime_error("Parent operation intent request is not an object");
    std::string bytes="{";
    for (const auto &field:value.members) {
        if (bytes.size()>1) bytes+=',';
        bytes+=EncodeJsonString(field.first)+':';
        if (field.second.type==JsonValue::String) bytes+=EncodeJsonString(field.second.text);
        else if (field.second.type==JsonValue::Number || field.second.type==JsonValue::Boolean) bytes+=field.second.text;
        else if (field.second.type==JsonValue::Array) {
            bytes+='[';
            for (const auto &item:field.second.items) {
                if (bytes.back()!='[') bytes+=',';
                if (item.type!=JsonValue::String) throw std::runtime_error("Parent operation intent request has a non-string array item");
                bytes+=EncodeJsonString(item.text);
            }
            bytes+=']';
        } else throw std::runtime_error("Parent operation intent request contains an unsupported JSON field");
    }
    return DecodeSupervisorControlRequest(bytes+'}');
}

OperationResult DecodeOperation(const JsonValue &v,const SupervisorControlRequest &request) {
    std::set<std::string> fields={"schema","operation_id","command","request_key","phase","message","instance_id","plan_sha256","controller_id","controller_snapshot_sha256","controller_epoch","result"};
    if (request.command==SupervisorCommand::Catalog) fields.insert("catalog");
    if (request.command==SupervisorCommand::HandoffControl || request.command==SupervisorCommand::CompleteControlHandoff) fields.insert("handoff");
    if (SupervisorCommandRetiresPlan(request.command)) fields.insert("plan_retirement");
    ExactSupervisorFields(v,fields);
    if (Unsigned(v.At("schema"))!=1 || String(v.At("operation_id"))!=request.operationId || String(v.At("command"))!=SupervisorCommandName(request.command) ||
        String(v.At("request_key"))!=SupervisorControlRequestKey(request) || String(v.At("instance_id"))!=request.instanceId || String(v.At("plan_sha256"))!=request.planSha256 ||
        String(v.At("controller_id"))!=request.controllerId || String(v.At("controller_snapshot_sha256"))!=request.controllerSnapshotSha256 || String(v.At("controller_epoch"))!=request.controllerEpoch)
        throw std::runtime_error("Remote operation reply differs from bound request");
    OperationResult result; result.phase=String(v.At("phase")); result.message=String(v.At("message"));
    if (result.phase!="accepted" && result.phase!="running" && result.phase!="complete" && result.phase!="failed" && result.phase!="unknown") throw std::runtime_error("Invalid remote operation phase");
    if (v.At("result").type!=JsonValue::Null) {
        result.hasData=true; result.data=DecodeSupervisorStatus(v.At("result"));
        if (result.data.planSha256!=request.planSha256) throw std::runtime_error("Remote operation result plan differs");
    }
    if (request.command==SupervisorCommand::HandoffControl || request.command==SupervisorCommand::CompleteControlHandoff || SupervisorCommandRetiresPlan(request.command)) {
        const auto &handoff=v.At(SupervisorCommandRetiresPlan(request.command) ? "plan_retirement" : "handoff");
        if (SupervisorCommandRetiresPlan(request.command)) {
            ExactSupervisorFields(handoff,{"successor_controller_snapshot_sha256","successor_controller_epoch","proof_sha256","successor_plan_sha256"});
            if (String(handoff.At("successor_plan_sha256"))!=request.successorPlanSha256) throw std::runtime_error("Retirement successor plan differs");
        } else ExactSupervisorFields(handoff,{"successor_controller_snapshot_sha256","successor_controller_epoch","proof_sha256"});
        if (String(handoff.At("successor_controller_snapshot_sha256"))!=request.successorControllerSnapshotSha256 ||
            String(handoff.At("successor_controller_epoch"))!=request.successorControllerEpoch)
            throw std::runtime_error("Remote controller handoff successor differs from original intent");
        result.handoffProofSha256=String(handoff.At("proof_sha256"));
        if (result.phase=="complete") Digest(result.handoffProofSha256);
        else if (!result.handoffProofSha256.empty()) throw std::runtime_error("Unconfirmed handoff claims a proof");
    }
    if (result.phase=="complete" && !result.hasData) throw std::runtime_error("Completed remote operation lacks data lifecycle result");
    if (request.command==SupervisorCommand::Catalog && v.At("catalog").type!=JsonValue::Null) {
        const auto &c=v.At("catalog"); ExactSupervisorFields(c,{"schema","kind","complete","request_key","manifest_sha256","operation_id","directory","payload_method","raw_payload_bytes_read","linked_files"});
        if (Unsigned(c.At("schema"))!=1 || String(c.At("kind"))!="getdaq.receiver-catalog-complete" || !Boolean(c.At("complete")) ||
            String(c.At("payload_method"))!="hardlink" || Unsigned(c.At("raw_payload_bytes_read"),true)) throw std::runtime_error("Remote catalog completion method/status differs");
        EccControlCatalogReceipt receipt; receipt.requestKey=String(c.At("request_key")); receipt.manifestSha256=String(c.At("manifest_sha256")); receipt.operationId=String(c.At("operation_id"));
        receipt.directory=String(c.At("directory")); receipt.linkedFiles=Unsigned(c.At("linked_files"),true); result.catalogJson=EncodeEccControlCatalogReceipt(receipt);
    }
    if (request.command==SupervisorCommand::Catalog && result.phase=="complete" && result.catalogJson.empty()) throw std::runtime_error("Remote catalog lacks completion receipt");
    return result;
}

} // namespace RemoteOperationCodec
} // namespace GetDAQ
