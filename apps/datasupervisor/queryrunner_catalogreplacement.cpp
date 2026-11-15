#include "queryrunner_catalogreplacement_p.h"
#include "queryrunner_validation_p.h"
#include "supervisorcommon.h"
#include <stdexcept>

namespace GetDAQ { namespace SupervisorQueryPrivate {
using namespace SupervisorIO;

SupervisorHttpResponse ObserveCatalogReplacement(SupervisorHttpClient &client,
                                                   const std::string &publishOperationId,
                                                   const std::string &agentId,
                                                   const std::string &controllerId,
                                                   const std::string &planSha256,
                                                   const std::string &instanceId) {
    SupervisorOperationId(publishOperationId);
    const auto operationReply=client.Operation(publishOperationId);
    const auto original=DecodeResponse(operationReply,200);
    ValidateOperation(original,publishOperationId,controllerId,planSha256);
    if (String(original.At("command"))!="catalog")
        throw std::runtime_error("Original operation is not a catalog Publish; no observation accepted");

    const auto response=client.CatalogReplacementObservation(publishOperationId);
    const auto value=DecodeResponse(response,200);
    ExactSupervisorFields(value,{"schema","kind","agent_id","current_instance_id","plan_sha256","publish_operation_id","request_key","original_instance_id","controller_id","controller_snapshot_sha256","controller_epoch","catalog_replacement_id","operation_phase","observation"});
    if (Unsigned(value.At("schema"))!=1 || String(value.At("kind"))!="getdaq.supervisor-catalog-replacement-observation" ||
        String(value.At("agent_id"))!=agentId || String(value.At("current_instance_id"))!=instanceId ||
        String(value.At("plan_sha256"))!=planSha256 || String(value.At("publish_operation_id"))!=publishOperationId ||
        String(value.At("request_key"))!=String(original.At("request_key")) ||
        String(value.At("original_instance_id"))!=String(original.At("instance_id")) ||
        String(value.At("controller_id"))!=controllerId ||
        String(value.At("controller_snapshot_sha256"))!=String(original.At("controller_snapshot_sha256")) ||
        String(value.At("controller_epoch"))!=String(original.At("controller_epoch")) ||
        String(value.At("operation_phase"))!=String(original.At("phase")))
        throw std::runtime_error("Catalog replacement observation differs from the persisted Publish operation/current service identity");
    Digest(String(value.At("request_key")));
    Digest(String(value.At("original_instance_id")));
    Digest(String(value.At("controller_snapshot_sha256")));
    Digest(String(value.At("controller_epoch")));
    const auto replacement=String(value.At("catalog_replacement_id"));
    SupervisorOperationId(replacement);
    const auto &observation=value.At("observation");
    ExactSupervisorFields(observation,{"schema","kind","operation_id","request_key","phase","target","backup","candidate","old_receipt_sha256","published_receipt_sha256","transaction_journal_present","old_preserved_marker_present","candidate_published_marker_present","raw_payload_bytes_read","recovery_authorized"});
    const auto phase=String(observation.At("phase"));
    if (Unsigned(observation.At("schema"))!=1 || String(observation.At("kind"))!="getdaq.catalog-replacement-observation" ||
        String(observation.At("operation_id"))!=replacement ||
        (phase!="unknown" && phase!="prepared" && phase!="old_preserved" && phase!="published" && phase!="finalize_incomplete" && phase!="finalized") ||
        String(observation.At("raw_payload_bytes_read"))!="0" || Boolean(observation.At("recovery_authorized")))
        throw std::runtime_error("Catalog replacement observation is malformed or authorizes recovery");
    Digest(String(observation.At("request_key")));
    Path(String(observation.At("target")));
    Path(String(observation.At("backup")));
    Path(String(observation.At("candidate")));
    return response;
}

} }
