#ifndef GETDAQ_CONFIGURATION_READBACK_FIXTURE_H
#define GETDAQ_CONFIGURATION_READBACK_FIXTURE_H
#include "eccprotocol.h"
#include "systemexperimentconfiguration.h"
namespace ConfigurationReadbackFixture {
// Metadata only; no hardware, raw receiver or frame producer.
inline std::string Cache(bool unknown=false,const std::string &phase="captured") {
    GetDAQ::EccControlControllerStatus controller;controller.state=4;controller.operationId="configure-1";
    controller.outcomeUnknown=controller.needsReconciliation=unknown;
    GetDAQ::ConfigurationReadbackEvidence evidence;
    evidence.auditSha256=std::string(64,'f');evidence.auditChainSha256=std::string(64,'e');
    evidence.nodeProxy="N:tcp -h 127.0.0.1 -p 1";evidence.startedAtUnixMs=9007199254740993ull;
    evidence.steps=5;evidence.writesAcknowledged=2;evidence.writesReadbackVerified=1;
    evidence.checksMatched=1;evidence.managerCommandsAcknowledged=1;evidence.delaysCompleted=1;
    const auto entries=phase=="captured" ? "[{\"id\":\"C0\",\"stage\":\"cobo-configure\",\"audit_file\":\"hardware-0.jsonl\",\"evidence\":"+
        GetDAQ::EncodeConfigurationReadbackEvidence(evidence)+"}]" : "[]";
    return "{\"schema\":1,\"kind\":\"getdaq.ecc-configure-readbacks\",\"instance_id\":\""+std::string(64,'a')+
        "\",\"snapshot_sha256\":\""+std::string(64,'b')+"\",\"configuration_sha256\":\""+std::string(64,'1')+
        "\",\"artifact_sha256\":\""+std::string(64,'2')+"\",\"controller\":"+GetDAQ::EncodeEccControlControllerStatus(controller)+
        ",\"readbacks\":{\"phase\":"+GetDAQ::EncodeJsonString(phase)+",\"configure_operation_id\":"+
        GetDAQ::EncodeJsonString(phase=="not-observed" ? "" : "configure-1")+
        ",\"historical\":true,\"live_hardware_checked\":false,\"actual_loaded_configuration_verified\":false,\"entries\":"+entries+"}}";
}
}
#endif
