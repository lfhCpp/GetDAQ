#include "eccprotocol.h"
#include "eccprotocolinternal_p.h"
#include "configurationsha256.h"
#include "strictjson.h"
#include <limits>
#include <stdexcept>

namespace GetDAQ {
namespace {
using namespace EccProtocolInternal;
int Voltage(const JsonValue &v) {
    if(v.type!=JsonValue::Number || v.text.empty() || v.text.size()>5 || v.text=="-0")
        throw std::invalid_argument("Pulser voltage must be an integer from -1000 to 1000 mV");
    std::size_t first=v.text[0]=='-' ? 1 : 0;
    if(first==v.text.size() || v.text.find_first_not_of("0123456789",first)!=std::string::npos)
        throw std::invalid_argument("Pulser voltage must be an integer from -1000 to 1000 mV");
    int value=0;for(std::size_t i=first;i<v.text.size();++i)value=value*10+(v.text[i]-'0');
    if(v.text[0]=='-')value=-value;
    if(value < -1000 || value > 1000)throw std::invalid_argument("Pulser voltage must be within -1000..1000 mV");
    return value;
}
}
namespace EccProtocolInternal {
// Diagnostic messages may contain Ice's line breaks. Identifier/policy text
// continues to reject control bytes; only human-readable messages use this.
EccControlRequest DecodeRequest(const JsonValue &v) {
    const auto command=String(v.At("command"));
    if (command=="reprepare-cobo" || command=="configure-cobo") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id"});
    else if(command=="set-cobo-led") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id","asad_id","led_type","led_state"});
    else if(command=="blink-cobo-leds") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id"});
    else if(command=="flip-cobo-led") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id","led_id"});
    else if(command=="pulse-cobo-led") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id","led_id","period_ms"});
    else if(command=="trigger-asad-pulser") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","cobo_id","voltage_mv"});
    else if (command=="retire-data-plans") {
        if(v.members.count("successor_plan_file")) Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","successor_scope_sha256","successor_epoch","successor_plan_sha256","successor_data_binding_sha256","successor_receiver_plans","successor_plan_file"});
        else Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","successor_scope_sha256","successor_epoch","successor_plan_sha256","successor_data_binding_sha256","successor_receiver_plans"});
    }
    else if (command=="handoff-data-control") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","successor_scope_sha256","successor_epoch","successor_plan_sha256","successor_data_binding_sha256"});
    else if (command=="catalog") {
        if(v.members.count("catalog_replace_existing"))
            Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","catalog_experiment","catalog_run_number","catalog_replace_existing"});
        else Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","catalog_experiment","catalog_run_number"});
    }
    else if (command=="resume-catalog-replacement") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","failed_operation_id"});
    else if(command=="reconcile-acquisition-with-alarm" || command=="reconcile-preparation-with-alarm" || command=="release-stopped-alarm") Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","failed_operation_id","alarm_operation_id"});
    else if((command=="complete-data-handoff" || command=="reconcile-description" || command=="reconcile-preparation" || (command=="reconcile-acquisition" || command=="reconcile-acquisition-with-sources") || command=="reconcile-acquisition-with-alarm" || command=="reconcile-preparation-with-alarm" || command=="release-stopped-alarm")) Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch","failed_operation_id"});
    else Fields(v,{"schema","operation_id","command","instance_id","snapshot_sha256","client_id","client_epoch"});
    if (Number(v.At("schema"))!=1) throw std::invalid_argument("Unsupported ECC request schema");
    EccControlRequest r; r.operationId=String(v.At("operation_id")); r.command=String(v.At("command")); r.instanceId=String(v.At("instance_id"));
    r.snapshotSha256=String(v.At("snapshot_sha256")); r.clientId=String(v.At("client_id")); r.clientEpoch=String(v.At("client_epoch"));
    if(command=="reprepare-cobo" || command=="configure-cobo" || command=="blink-cobo-leds")r.coboId=String(v.At("cobo_id"));
    if(command=="flip-cobo-led") {r.coboId=String(v.At("cobo_id"));r.ledFlipId=Number(v.At("led_id"),16);}
    if(command=="pulse-cobo-led") {r.coboId=String(v.At("cobo_id"));r.ledPulseId=Number(v.At("led_id"),16);r.ledPulsePeriodMs=Number(v.At("period_ms"));}
    if(command=="set-cobo-led") {r.coboId=String(v.At("cobo_id"));r.ledAsadId=Number(v.At("asad_id"),3);r.ledType=Number(v.At("led_type"),3);r.ledState=Number(v.At("led_state"),3);}
    if(command=="trigger-asad-pulser") {r.coboId=String(v.At("cobo_id"));r.pulserVoltageMv=Voltage(v.At("voltage_mv"));}
    if (command=="handoff-data-control" || command=="retire-data-plans") {
        r.successorScopeSha256=String(v.At("successor_scope_sha256"));r.successorEpoch=String(v.At("successor_epoch"));
        r.successorPlanSha256=String(v.At("successor_plan_sha256"));
        r.successorDataBindingSha256=String(v.At("successor_data_binding_sha256"));
    }
    if (command=="retire-data-plans") {
        if(v.members.count("successor_plan_file")) {
            r.successorPlanFile=String(v.At("successor_plan_file"));
            if(r.successorPlanFile.empty())throw std::invalid_argument("Explicit successor plan file cannot be empty");
        }
        const auto &plans=v.At("successor_receiver_plans");
        if (plans.type!=JsonValue::Array || plans.items.empty() || plans.items.size()>195) throw std::invalid_argument("Invalid successor receiver plan set");
        for (const auto &plan:plans.items) {
            Fields(plan,{"agent_id","plan_sha256","plan_file"});EccReceiverPlanTarget target;
            target.agentId=String(plan.At("agent_id"));target.planSha256=String(plan.At("plan_sha256"));target.planFile=String(plan.At("plan_file"));
            r.successorReceiverPlans.push_back(target);
        }
    }
    if (command=="catalog") { r.catalogExperiment=String(v.At("catalog_experiment")); r.catalogRunNumber=Number(v.At("catalog_run_number"),unsigned(std::numeric_limits<int>::max())-1); }
    if (command=="catalog" && v.members.count("catalog_replace_existing")) r.catalogReplaceExisting=Boolean(v.At("catalog_replace_existing"));
    if((command=="complete-data-handoff" || command=="resume-catalog-replacement" || command=="reconcile-description" || command=="reconcile-preparation" || (command=="reconcile-acquisition" || command=="reconcile-acquisition-with-sources") || command=="reconcile-acquisition-with-alarm" || command=="reconcile-preparation-with-alarm" || command=="release-stopped-alarm"))r.failedOperationId=String(v.At("failed_operation_id"));
    if(command=="reconcile-acquisition-with-alarm" || command=="reconcile-preparation-with-alarm" || command=="release-stopped-alarm")r.alarmOperationId=String(v.At("alarm_operation_id"));
    ValidateEccControlRequest(r); return r;
}

}
EccControlRequest DecodeEccControlRequest(const std::string &bytes) { return DecodeRequest(Parse(bytes,8192)); }
std::string EncodeEccControlRequest(const EccControlRequest &r) {
    ValidateEccControlRequest(r); auto bytes="{\"schema\":1,\"operation_id\":"+EncodeJsonString(r.operationId)+",\"command\":"+EncodeJsonString(r.command)+
        ",\"instance_id\":"+EncodeJsonString(r.instanceId)+",\"snapshot_sha256\":"+EncodeJsonString(r.snapshotSha256)+",\"client_id\":"+EncodeJsonString(r.clientId)+",\"client_epoch\":"+EncodeJsonString(r.clientEpoch);
    if (r.command=="reprepare-cobo" || r.command=="configure-cobo" || r.command=="blink-cobo-leds")bytes+=",\"cobo_id\":"+EncodeJsonString(r.coboId);
    if (r.command=="flip-cobo-led")bytes+=",\"cobo_id\":"+EncodeJsonString(r.coboId)+",\"led_id\":"+std::to_string(r.ledFlipId);
    if (r.command=="pulse-cobo-led")bytes+=",\"cobo_id\":"+EncodeJsonString(r.coboId)+",\"led_id\":"+std::to_string(r.ledPulseId)+",\"period_ms\":"+std::to_string(r.ledPulsePeriodMs);
    if (r.command=="trigger-asad-pulser")bytes+=",\"cobo_id\":"+EncodeJsonString(r.coboId)+",\"voltage_mv\":"+std::to_string(r.pulserVoltageMv);
    if (r.command=="set-cobo-led")bytes+=",\"cobo_id\":"+EncodeJsonString(r.coboId)+",\"asad_id\":"+std::to_string(r.ledAsadId)+
        ",\"led_type\":"+std::to_string(r.ledType)+",\"led_state\":"+std::to_string(r.ledState);
    if (r.command=="catalog") bytes+=",\"catalog_experiment\":"+EncodeJsonString(r.catalogExperiment)+",\"catalog_run_number\":"+std::to_string(r.catalogRunNumber)+
        (r.catalogReplaceExisting ? ",\"catalog_replace_existing\":true" : "");
    if (r.command=="handoff-data-control" || r.command=="retire-data-plans") bytes+=",\"successor_scope_sha256\":"+EncodeJsonString(r.successorScopeSha256)+
        ",\"successor_epoch\":"+EncodeJsonString(r.successorEpoch)+",\"successor_plan_sha256\":"+EncodeJsonString(r.successorPlanSha256)+
        ",\"successor_data_binding_sha256\":"+EncodeJsonString(r.successorDataBindingSha256);
    if (r.command=="retire-data-plans") {
        if (!r.successorPlanFile.empty()) bytes+=",\"successor_plan_file\":"+EncodeJsonString(r.successorPlanFile);
        bytes+=",\"successor_receiver_plans\":[";
        for (const auto &target:r.successorReceiverPlans) {
            if (bytes.back()!='[') bytes+=',';
            bytes+="{\"agent_id\":"+EncodeJsonString(target.agentId)+",\"plan_sha256\":"+EncodeJsonString(target.planSha256)+",\"plan_file\":"+EncodeJsonString(target.planFile)+"}";
        }
        bytes+=']';
    }
    if((r.command=="complete-data-handoff" || r.command=="resume-catalog-replacement" || r.command=="reconcile-description" || r.command=="reconcile-preparation" || (r.command=="reconcile-acquisition" || r.command=="reconcile-acquisition-with-sources") || r.command=="reconcile-acquisition-with-alarm" || r.command=="reconcile-preparation-with-alarm" || r.command=="release-stopped-alarm"))bytes+=",\"failed_operation_id\":"+EncodeJsonString(r.failedOperationId);
    if(r.command=="reconcile-acquisition-with-alarm" || r.command=="reconcile-preparation-with-alarm" || r.command=="release-stopped-alarm")bytes+=",\"alarm_operation_id\":"+EncodeJsonString(r.alarmOperationId);
    bytes+="}\n";
    if (bytes.size()>8192) throw std::invalid_argument("ECC control request exceeds 8KiB budget");
    return bytes;
}
std::string EccControlRequestKey(const EccControlRequest &r) {
    return ConfigurationSha256("getdaq.ecc-control-request:1\n"+EncodeEccControlRequest(r));
}
}
