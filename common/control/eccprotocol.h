#ifndef GETDAQ_ECCPROTOCOL_H
#define GETDAQ_ECCPROTOCOL_H
#include <string>
#include <cstdint>
#include <vector>
#include "configurationselection.h"
#include "configurationreadback.h"
namespace GetDAQ {
struct EccReceiverPlanTarget {
    std::string agentId,planSha256,planFile; // File is local to that data node.
};
// 可供Windows/Qt客户端复用的纯协议；不包含Ice、硬件或POSIX进程接口。
struct EccControlRequest {
    std::string operationId,command,instanceId,snapshotSha256,clientId,clientEpoch;
    std::string catalogExperiment;
    std::string coboId; // Explicit selected-board Configure/maintenance commands.
    unsigned int ledAsadId=0,ledType=0,ledState=0; // Only explicit set-cobo-led; enum values match HardwareStep.
    unsigned int ledFlipId=0; // Only explicit flip-cobo-led; 0..15 selects a GPIO LED bit, 16 means all LEDs.
    unsigned int ledPulseId=0,ledPulsePeriodMs=0; // Only pulse-cobo-led; 0..15 selects one LED, 16 all LEDs, period 0 stops it.
    int pulserVoltageMv=0; // Only explicit one-shot trigger-asad-pulser; bounded to -1000..1000 mV.
    std::string failedOperationId; // Required by recovery commands.
    std::string alarmOperationId; // Only explicit acquisition recovery acknowledging one original alarm.
    std::string successorScopeSha256,successorEpoch,successorPlanSha256; // Only explicit data-control handoff.
    std::string successorDataBindingSha256;
    std::string successorPlanFile; // Optional service-host reviewed plan lease for retire-data-plans.
    std::vector<EccReceiverPlanTarget> successorReceiverPlans; // Only retire-data-plans.
    unsigned int catalogRunNumber=0;
    bool catalogReplaceExisting=false; // Explicit Start-approved replacement of a matching completed Run.
};
struct EccSuccessorIdentity {
    std::string planFile,serviceId,controllerId,inputBindingSha256,snapshotSha256;
    std::string dataBindingSha256,scopeSha256,auditRoot,epoch;
};
EccSuccessorIdentity DecodeEccSuccessorIdentity(const std::string &bytes);
std::string EncodeEccSuccessorIdentity(const EccSuccessorIdentity &identity);
struct EccPlanTransitionReview {
    EccSuccessorIdentity successor;
    std::string successorBytes,successorSha256,targetsBytes;
    std::vector<EccReceiverPlanTarget> targets;
};
EccPlanTransitionReview ReviewEccPlanTransition(const std::string &successorBytes,const std::string &targetsBytes);
void ValidateEccControlRequest(const EccControlRequest &request);
EccControlRequest DecodeEccControlRequest(const std::string &bytes);
std::string EncodeEccControlRequest(const EccControlRequest &request);
std::string EccControlRequestKey(const EccControlRequest &request);
struct EccControlControllerStatus {
    unsigned int state=1,transition=0,error=0;
    bool outcomeUnknown=false,needsReconciliation=false;
    std::string operationId,message;
};
EccControlControllerStatus DecodeEccControlControllerStatus(const std::string &bytes);
std::string EncodeEccControlControllerStatus(const EccControlControllerStatus &status);
struct EccControlIdentity {
    std::string serviceId,clientId,instanceId,snapshotSha256,inputBindingSha256;
    std::string configurationSha256,artifactSha256,dataBindingSha256;
};
struct EccControlMaintenanceTarget {
    std::string id,nodeName;
    // Per-board lifecycle projection from the fixed ECC operation journal.
    // This is historical control evidence, not a live hardware readback.
    std::string lifecycleState="unknown",operationId,operation,operationPhase="idle";
    EccControlMaintenanceTarget() = default;
    EccControlMaintenanceTarget(const std::string &targetId,const std::string &expectedName) : id(targetId),nodeName(expectedName) {}
};
struct EccControlMaintenanceTargets {
    std::string instanceId,snapshotSha256,configurationSha256,artifactSha256;
    std::vector<EccControlMaintenanceTarget> cobos;
};
EccControlMaintenanceTargets DecodeEccControlMaintenanceTargets(const std::string &bytes);
std::string EncodeEccControlMaintenanceTargets(const EccControlMaintenanceTargets &targets);
EccControlIdentity DecodeEccControlIdentity(const std::string &bytes);
struct EccControlConfiguration {
    std::string instanceId,snapshotSha256,inputBindingSha256,configurationSha256,artifactSha256;
    bool hasSelection=false;
    ConfigurationSelection selection;
};
// 启动输入缓存；schema1不允许声称实时文件或全局硬件加载已验证。
EccControlConfiguration DecodeEccControlConfiguration(const std::string &bytes);
std::string EncodeEccControlConfiguration(const EccControlConfiguration &configuration);
struct EccControlOwnership {
    std::string instanceId,snapshotSha256,directory;
    bool enabled=false,heldAtLastCheck=false,faultLatched=false;
    std::vector<std::string> resources;
};
// Only cached same-host cooperating-ECC reservations; board/cross-host claims are rejected.
EccControlOwnership DecodeEccControlOwnership(const std::string &bytes);
struct EccControlReadbackEntry {
    std::string id,stage,auditFile;
    ConfigurationReadbackEvidence evidence;
};
struct EccControlConfigureReadbacks {
    std::string instanceId,snapshotSha256,configurationSha256,artifactSha256,phase,configureOperationId;
    EccControlControllerStatus controller;
    std::vector<EccControlReadbackEntry> entries;
};
// Authenticated cached historical Configure checks; never a live/full loaded-version claim.
EccControlConfigureReadbacks DecodeEccControlConfigureReadbacks(const std::string &bytes);
struct EccControlReceiverStatus {
    std::string id,sessionId,outputDirectory;
    int pid=0,exitCode=-1;
    bool reaped=false,sealVerified=false;
    bool sourceUnusedConfirmed=false,sourceEndConfirmed=false;
    std::uint64_t receivedBytes=0,frames=0,files=0;
};
struct EccControlCatalogReceipt {
    std::string requestKey,manifestSha256,operationId,directory;
    std::uint64_t linkedFiles=0;
};
EccControlCatalogReceipt DecodeEccControlCatalogReceipt(const std::string &bytes);
std::string EncodeEccControlCatalogReceipt(const EccControlCatalogReceipt &receipt);
struct EccControlDataNodeStatus {
    std::string agentId,instanceId,planSha256,state,sessionId,message;
    bool needsReconciliation=false;
    std::vector<EccControlReceiverStatus> receivers;
    bool catalogVerified=false;
    EccControlCatalogReceipt catalog;
};
struct EccControlSystemStatus {
    std::string instanceId,snapshotSha256,operationId,clientEpoch,message;
    bool operationActive=false,controlNeedsReconciliation=false;
    EccControlControllerStatus controller;
    std::string dataScopeSha256,dataEpoch,dataMessage;
    bool dataNeedsReconciliation=false;
    std::vector<EccControlDataNodeStatus> nodes;
};
EccControlSystemStatus DecodeEccControlSystemStatus(const std::string &bytes);
// Match the authenticated original retirement receipt to the persisted parent
// request and original live status; successor startup still verifies each node.
void ValidateEccPlanRetirementReceipt(const std::string &bytes,const EccControlRequest &request,
                                      const EccControlSystemStatus &original);
struct EccControlReceiverProcess {
    std::string id,state,message;
    int pid=0,exitCode=-1,signal=0;
};
struct EccControlReceiverNodeObservation {
    std::string agentId,instanceId,planSha256,message;
    bool available=false,busy=false;
    std::uint64_t observedAtUnixMs=0;
    EccControlDataNodeStatus lifecycle;
    std::vector<EccControlReceiverProcess> receivers;
};
struct EccControlReceiverObservation {
    std::string instanceId,snapshotSha256,dataBindingSha256;
    std::vector<EccControlReceiverNodeObservation> nodes;
};
EccControlReceiverObservation DecodeEccControlReceiverObservation(const std::string &bytes);
struct EccControlReceiverEvents {
    std::string instanceId,snapshotSha256,dataBindingSha256,agentId,planSha256,receiverInstanceId;
    std::string sourceId,sessionId;
    int pid=0;
    bool available=false;
    std::uint64_t acquisitionEvents=0,observedAtUnixMs=0,fileBytes=0;
};
EccControlReceiverEvents DecodeEccControlReceiverEvents(const std::string &bytes,bool fileBytes=false);
// Only explicit failed exits belonging to the retained lifecycle are actionable.
// Unavailable/busy nodes and successful exits are not inferred failures.
std::vector<std::string> FailedBoundReceivers(const EccControlSystemStatus &bound,
                                            const EccControlReceiverObservation &observed,
                                            const std::string &dataBindingSha256);
struct EccControlSourceObservation {
    struct Source {
        std::string id,protocol,phase,message,instanceId,receiverSession,state,error;
        unsigned int localState=0;
        std::uint64_t observedAtUnixMs=0,issuedBytes=0,locallySentBytes=0;
        bool startIssued=false,stopAcknowledged=false,drainAcknowledged=false,finishIssued=false;
        bool sourceEndAcknowledged=false,failureStopIssued=false,failureStopAcknowledged=false;
    };
    std::string instanceId,snapshotSha256,configurationSha256,artifactSha256,controllerOperationId;
    bool busy=false;
    std::vector<Source> sources;
};
EccControlSourceObservation DecodeEccControlSourceObservation(const std::string &bytes);
struct EccControlAlarmNode {
    std::string id;
    unsigned int subscription=0,pending=0,alarmMask=0;
    std::uint64_t received=0,durable=0,lost=0,invalid=0;
    bool stopRequested=false,queueOverflow=false,auditFailed=false,budgetExceeded=false;
};
struct EccControlAlarmStatus {
    std::string instanceId,snapshotSha256,operationId,phase,controllerOperationId;
    bool enabled=false,latched=false,hasObservation=false,stopRequested=false,controllerOutcomeUnknown=false;
    unsigned int controllerState=1;
    std::vector<EccControlAlarmNode> nodes;
};
EccControlAlarmStatus DecodeEccControlAlarmStatus(const std::string &bytes);
struct EccControlReceiverFailureStatus {
    std::string instanceId,snapshotSha256,operationId,phase;
    bool enabled=false,latched=false,hasEvidence=false;
    EccControlSystemStatus bound;
    EccControlReceiverObservation observation;
    std::vector<std::string> failedIds;
};
EccControlReceiverFailureStatus DecodeEccControlReceiverFailureStatus(const std::string &bytes);
struct EccControlOperation {
    EccControlRequest request;
    std::string requestKey,phase,message,resultJson;
};
std::string EncodeEccControlOperation(const EccControlOperation &operation);
EccControlOperation DecodeEccControlOperation(const std::string &bytes);
}
#endif
