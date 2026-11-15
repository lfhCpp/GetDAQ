#include "queryrunner_plantransition_p.h"
#include "planio.h"
#include "queryrunner_validation_p.h"
#include "supervisorcommon.h"
#include "configurationsha256.h"
#include <stdexcept>

namespace GetDAQ { namespace SupervisorQueryPrivate {
using namespace SupervisorIO;

void ValidatePlanTransitionResponse(const SupervisorHttpResponse &response, bool predecessor,
                                    const std::string &agentId, const std::string &controllerId,
                                    const std::string &planSha256, const std::string &instanceId) {
    if (response.code!=200) throw std::runtime_error("Original plan transition proof unavailable; no command retry");
    JsonLimits limits; limits.stringBytes=1024*1024; limits.arrayItems=131072;
    const auto value=ParseStrictJson(response.body,limits);
    const JsonValue *proof=&value;
    if (predecessor) {
        ExactSupervisorFields(value,{"schema","kind","agent_id","instance_id","plan_sha256","proof_sha256","proof"});
        ValidateScope(value,planSha256);
        if (String(value.At("kind"))!="getdaq.supervisor-plan-predecessor" ||
            String(value.At("agent_id"))!=agentId || String(value.At("instance_id"))!=instanceId)
            throw std::runtime_error("Activated plan predecessor identity differs");
        Digest(String(value.At("proof_sha256")));
        proof=&value.At("proof");
    }
    ExactSupervisorFields(*proof,{"schema","kind","proof_file","deployment_root","agent_id","request_key","request","successor_plan","lifecycle","cycle_evidence_sha256"});
    const auto retired=DecodeSupervisorControlRequest(String(proof->At("request")));
    if (Unsigned(proof->At("schema"))!=1 || String(proof->At("kind"))!="getdaq.supervisor-plan-retirement" ||
        String(proof->At("agent_id"))!=agentId || retired.command!=SupervisorCommand::RetirePlan ||
        retired.controllerId!=controllerId || String(proof->At("request_key"))!=SupervisorControlRequestKey(retired) ||
        (predecessor ? retired.successorPlanSha256!=planSha256 : retired.planSha256!=planSha256 || retired.instanceId!=instanceId) ||
        ConfigurationSha256(String(proof->At("successor_plan")))!=retired.successorPlanSha256)
        throw std::runtime_error("Original plan retirement proof/target/request differs");
    const auto lifecycle=DecodeSupervisorStatus(proof->At("lifecycle"));
    if (lifecycle.planSha256!=retired.planSha256 || lifecycle.state!=DataSupervisorStatus::Stopped ||
        lifecycle.needsReconciliation || lifecycle.sessionId!=retired.expectedSessionId)
        throw std::runtime_error("Original plan retirement cycle differs");
}

} }
