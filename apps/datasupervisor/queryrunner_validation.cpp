#include "queryrunner_validation_p.h"
#include <set>
#include <stdexcept>

namespace GetDAQ { namespace SupervisorQueryPrivate {
using namespace SupervisorIO;

JsonValue DecodeResponse(const SupervisorHttpResponse &response,unsigned int expectedCode) {
    if (response.code!=expectedCode)
        throw std::runtime_error("Supervisor returned HTTP "+std::to_string(response.code)+"; no command retry");
    return ParseStrictJson(response.body);
}

void ValidateScope(const JsonValue &value,const std::string &planSha256) {
    if (Unsigned(value.At("schema"))!=1 || String(value.At("plan_sha256"))!=planSha256)
        throw std::runtime_error("Supervisor response schema/plan differs");
}

void ValidateOperation(const JsonValue &value,const std::string &operationId,
                       const std::string &controllerId,const std::string &planSha256) {
    auto fields=std::set<std::string>{"schema","operation_id","command","request_key","phase","message","instance_id","plan_sha256","controller_id","controller_snapshot_sha256","controller_epoch","result"};
    const auto command=ParseSupervisorCommand(String(value.At("command")));
    if (command==SupervisorCommand::Catalog) fields.insert("catalog");
    if (command==SupervisorCommand::HandoffControl || command==SupervisorCommand::CompleteControlHandoff) fields.insert("handoff");
    if (SupervisorCommandRetiresPlan(command)) fields.insert("plan_retirement");
    ExactSupervisorFields(value,fields);
    ValidateScope(value,planSha256);
    if (String(value.At("operation_id"))!=operationId || String(value.At("controller_id"))!=controllerId)
        throw std::runtime_error("Supervisor operation ID/controller differs");
    Digest(String(value.At("instance_id")));
    Digest(String(value.At("request_key")));
    Digest(String(value.At("controller_snapshot_sha256")));
    Digest(String(value.At("controller_epoch")));
    const auto phase=String(value.At("phase"));
    if (phase!="accepted" && phase!="running" && phase!="complete" && phase!="failed" && phase!="unknown")
        throw std::runtime_error("Invalid supervisor operation phase");
    if (command==SupervisorCommand::HandoffControl || command==SupervisorCommand::CompleteControlHandoff) {
        const auto &handoff=value.At("handoff");
        ExactSupervisorFields(handoff,{"successor_controller_snapshot_sha256","successor_controller_epoch","proof_sha256"});
        Digest(String(handoff.At("successor_controller_snapshot_sha256")));
        Digest(String(handoff.At("successor_controller_epoch")));
        const auto proof=String(handoff.At("proof_sha256"));
        if (phase=="complete") Digest(proof);
        else if (!proof.empty()) throw std::runtime_error("Unconfirmed handoff claims a committed proof");
    }
    if (value.At("result").type!=JsonValue::Null && DecodeSupervisorStatus(value.At("result")).planSha256!=planSha256)
        throw std::runtime_error("Supervisor operation result plan differs");
    if (SupervisorCommandRetiresPlan(command)) {
        const auto &retirement=value.At("plan_retirement");
        ExactSupervisorFields(retirement,{"successor_plan_sha256","successor_controller_snapshot_sha256","successor_controller_epoch","proof_sha256"});
        Digest(String(retirement.At("successor_plan_sha256")));
        Digest(String(retirement.At("successor_controller_snapshot_sha256")));
        Digest(String(retirement.At("successor_controller_epoch")));
        const auto proof=String(retirement.At("proof_sha256"));
        if (phase=="complete") Digest(proof);
        else if (!proof.empty()) throw std::runtime_error("Unconfirmed plan retirement claims complete proof");
    }
}

} }
