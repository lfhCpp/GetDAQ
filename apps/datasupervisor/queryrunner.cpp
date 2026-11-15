#include "controlhttp.h"
#include "planio.h"
#include "queryfile.h"
#include "queryrunner_catalogreplacement_p.h"
#include "queryrunner_plantransition_p.h"
#include "queryrunner_validation_p.h"
#include "supervisorcommon.h"
#include "configurationsha256.h"
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <vector>

namespace {
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;
using namespace GetDAQ::SupervisorQueryPrivate;
unsigned int Number(const std::string &text,unsigned int maximum) {
    JsonValue value; value.type=JsonValue::Number; value.text=text;
    const auto n=Unsigned(value); if (!n || n>maximum) throw std::invalid_argument("Invalid query numeric option"); return unsigned(n);
}
}
using namespace GetDAQ;
using namespace GetDAQ::SupervisorIO;
int RunQuery(int argc,char **argv) {
    try {
        if (argc==2 && std::string(argv[1])=="--help") {
            std::cout<<"getdaq-supervisor-query identity|status|receivers|operation|catalog-replacement-observation|submit|build-handoff-completion|plan-retirement|predecessor --host IPv4 --port PORT --credential-file FILE\n"
                     <<"  --expected-agent ID --controller ID --expected-plan SHA256 [--timeout-ms 2000]\n"
                     <<"  operation and catalog-replacement-observation require --operation-id ID; submit requires --request-file JSON.\n"
                     <<"  build-handoff-completion requires the original failed request file,\n"
                     <<"    --failed-operation-id ORIGINAL_ID and --operation-id NEW_ID; it validates and prints a bound request without POST.\n"
                     <<"POST is sent once; 202 means accepted, not completed.\n";
#ifdef _WIN32
            std::cout<<"Windows credentials require a protected ACL granting access only to the current user and LocalSystem.\n";
#else
            std::cout<<"Credentials are read from 0600 files.\n";
#endif
            return 0;
        }
        if (argc<2 || argc%2!=0) throw std::invalid_argument("Use --help for fixed query options");
        const std::string mode=argv[1]; std::map<std::string,std::string> arguments;
        const std::set<std::string> supported={"--host","--port","--credential-file","--expected-agent","--controller","--expected-plan","--timeout-ms","--operation-id","--request-file","--failed-operation-id"};
        for (int i=2;i<argc;i+=2) if (!supported.count(argv[i]) || !arguments.emplace(argv[i],argv[i+1]).second) throw std::invalid_argument("Unknown/duplicate query option");
        const auto required=[&](const std::string &key) { const auto found=arguments.find(key); if (found==arguments.end()) throw std::invalid_argument("Missing query option: "+key); return found->second; };
        const auto agent=required("--expected-agent"),controller=required("--controller"),plan=required("--expected-plan"); Text(agent,128); Text(controller,128); Digest(plan);
        SupervisorHttpOptions options; options.address=required("--host"); options.port=Number(required("--port"),65535); options.credentialFile=required("--credential-file");
        if (arguments.count("--timeout-ms")) options.timeoutMs=Number(arguments.at("--timeout-ms"),60000);
        if (mode!="identity" && mode!="status" && mode!="receivers" && mode!="operation" && mode!="catalog-replacement-observation" && mode!="submit" && mode!="build-handoff-completion" && mode!="plan-retirement" && mode!="predecessor") throw std::invalid_argument("Unsupported query command");
        const bool hasOperation=arguments.count("--operation-id");
        const bool hasRequest=arguments.count("--request-file");
        const bool hasFailed=arguments.count("--failed-operation-id");
        if ((mode=="operation" || mode=="catalog-replacement-observation")!=hasOperation && mode!="build-handoff-completion") throw std::invalid_argument("Command has missing/irrelevant operation ID option");
        if ((mode=="submit")!=hasRequest && mode!="build-handoff-completion") throw std::invalid_argument("Command has missing/irrelevant request file option");
        if (mode=="build-handoff-completion" ? !(hasOperation && hasRequest && hasFailed) : hasFailed)
            throw std::invalid_argument("build-handoff-completion requires original request, failed ID and distinct new operation ID");
        SupervisorHttpClient client(options); const auto identityResponse=client.Identity(); const auto identity=DecodeResponse(identityResponse,200);
        ExactSupervisorFields(identity,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","plan"}); ValidateScope(identity,plan);
        if (String(identity.At("service"))!="getdaq-supervisor" || String(identity.At("agent_id"))!=agent || String(identity.At("controller_id"))!=controller) throw std::runtime_error("Supervisor agent/controller differs");
        const auto instance=String(identity.At("instance_id")); Digest(instance); const auto decoded=DecodeSupervisorPlan(identity.At("plan"));
        if (ConfigurationSha256(EncodeOptions(decoded.options,decoded.executableSha256))!=plan) throw std::runtime_error("Supervisor plan bytes differ from expected digest");
        SupervisorHttpResponse response=identityResponse;
        if (mode=="plan-retirement" || mode=="predecessor") {
            response=mode=="predecessor" ? client.Predecessor() : client.PlanRetirement();
            ValidatePlanTransitionResponse(response,mode=="predecessor",agent,controller,plan,instance);
        } else if (mode=="status") {
            response=client.Status(); const auto v=DecodeResponse(response,200);
            ExactSupervisorFields(v,{"schema","service","agent_id","instance_id","plan_sha256","operation_active","operation_id","control_needs_reconciliation","message","controller_snapshot_sha256","controller_epoch","lifecycle"}); ValidateScope(v,plan);
            if (String(v.At("service"))!="getdaq-supervisor" || String(v.At("agent_id"))!=agent || String(v.At("instance_id"))!=instance || DecodeSupervisorStatus(v.At("lifecycle")).planSha256!=plan)
                throw std::runtime_error("Supervisor status identity/instance differs");
            Boolean(v.At("operation_active")); Boolean(v.At("control_needs_reconciliation"));
        } else if (mode=="receivers") {
            response=client.Receivers(); const auto v=DecodeResponse(response,200);
            ExactSupervisorFields(v,{"schema","service","agent_id","controller_id","instance_id","plan_sha256","observation"}); ValidateScope(v,plan);
            if (String(v.At("service"))!="getdaq-supervisor" || String(v.At("agent_id"))!=agent || String(v.At("controller_id"))!=controller ||
                String(v.At("instance_id"))!=instance || DecodeSupervisorObservation(v.At("observation")).lifecycle.planSha256!=plan)
                throw std::runtime_error("Receiver observation identity/instance/plan differs");
        } else if (mode=="operation") {
            const auto id=required("--operation-id"); response=client.Operation(id); ValidateOperation(DecodeResponse(response,200),id,controller,plan);
        } else if (mode=="catalog-replacement-observation") {
            response=ObserveCatalogReplacement(client,required("--operation-id"),agent,controller,plan,instance);
        } else if (mode=="build-handoff-completion") {
            const auto originalBytes=ReadSupervisorQueryFile(required("--request-file"),8192);
            const auto original=DecodeSupervisorControlRequest(originalBytes);
            const auto failedId=required("--failed-operation-id");
            const auto completionId=required("--operation-id");
            SupervisorOperationId(failedId);
            SupervisorOperationId(completionId);

            if (EncodeSupervisorControlRequest(original)!=originalBytes || original.command!=SupervisorCommand::HandoffControl ||
                original.operationId!=failedId || completionId==failedId)
                throw std::invalid_argument("Original request must be the exact HandoffControl intent and completion must use a new ID");
            if (original.planSha256!=plan || original.instanceId!=instance || original.controllerId!=controller ||
                original.configurationSha256!=decoded.options.configurationSha256 || original.artifactSha256!=decoded.options.artifactSha256)
                throw std::invalid_argument("Original handoff request differs from the live service identity");

            const auto operationResponse=client.Operation(failedId);
            const auto failed=DecodeResponse(operationResponse,200);
            ValidateOperation(failed,failedId,controller,plan);
            if (String(failed.At("command"))!="handoff-control" || String(failed.At("phase"))!="failed" ||
                String(failed.At("request_key"))!=SupervisorControlRequestKey(original) || failed.At("result").type==JsonValue::Null)
                throw std::runtime_error("Original operation is not the exact failed handoff; no completion request generated");
            const auto originalResult=DecodeSupervisorStatus(failed.At("result"));

            const auto status=DecodeResponse(client.Status(),200);
            ExactSupervisorFields(status,{"schema","service","agent_id","instance_id","plan_sha256","operation_active","operation_id",
                "control_needs_reconciliation","message","controller_snapshot_sha256","controller_epoch","lifecycle"});
            ValidateScope(status,plan);
            const auto lifecycle=DecodeSupervisorStatus(status.At("lifecycle"));
            if (String(status.At("service"))!="getdaq-supervisor" || String(status.At("agent_id"))!=agent ||
                String(status.At("instance_id"))!=instance || Boolean(status.At("operation_active")) ||
                (!String(status.At("operation_id")).empty() && String(status.At("operation_id"))!=failedId) ||
                !Boolean(status.At("control_needs_reconciliation")) ||
                String(status.At("controller_snapshot_sha256"))!=original.controllerSnapshotSha256 ||
                String(status.At("controller_epoch"))!=original.controllerEpoch ||
                lifecycle.state!=DataSupervisorStatus::Stopped || lifecycle.needsReconciliation ||
                EncodeSupervisorStatus(lifecycle)!=EncodeSupervisorStatus(originalResult))
                throw std::runtime_error("Live owner or normally stopped cycle differs from the original failed handoff; no request generated");

            auto completion=original;
            completion.command=SupervisorCommand::CompleteControlHandoff;
            completion.operationId=completionId;
            completion.failedHandoffOperationId=failedId;
            std::cout<<EncodeSupervisorControlRequest(completion);
            return 0;
        } else if (mode=="submit") {
            const auto request=DecodeSupervisorControlRequest(ReadSupervisorQueryFile(required("--request-file"),2U*1024U*1024U));
            if (request.planSha256!=plan || request.instanceId!=instance || request.controllerId!=controller || request.configurationSha256!=decoded.options.configurationSha256 || request.artifactSha256!=decoded.options.artifactSha256)
                throw std::invalid_argument("Request scope differs from validated service identity; POST not sent");
            response=client.Submit(request); const auto v=DecodeResponse(response,202); ValidateOperation(v,request.operationId,controller,plan);
            if (String(v.At("request_key"))!=SupervisorControlRequestKey(request) || String(v.At("instance_id"))!=instance || String(v.At("command"))!=SupervisorCommandName(request.command))
                throw std::runtime_error("Accepted response differs from single submitted intent; outcome requires inspection");
            if ((request.command==SupervisorCommand::HandoffControl || request.command==SupervisorCommand::CompleteControlHandoff) &&
                (String(v.At("handoff").At("successor_controller_snapshot_sha256"))!=request.successorControllerSnapshotSha256 ||
                 String(v.At("handoff").At("successor_controller_epoch"))!=request.successorControllerEpoch))
                throw std::runtime_error("Accepted handoff successor differs from submitted intent");
            if (SupervisorCommandRetiresPlan(request.command) &&
                (String(v.At("plan_retirement").At("successor_plan_sha256"))!=request.successorPlanSha256 ||
                 String(v.At("plan_retirement").At("successor_controller_snapshot_sha256"))!=request.successorControllerSnapshotSha256 ||
                 String(v.At("plan_retirement").At("successor_controller_epoch"))!=request.successorControllerEpoch))
                throw std::runtime_error("Accepted retirement target differs from submitted intent");
        }
        std::cout<<response.body; return 0;
    } catch (const std::exception &e) { std::cerr<<e.what()<<'\n'; return 2; }
}
