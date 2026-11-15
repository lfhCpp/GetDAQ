#include "nativeecccoordinator_p.h"
#include "nativeeccfilesystemproof_p.h"
#include "nativeecccoordinator_recovery_p.h"
#include "hardwareaudit.h"
#include "configurationsha256.h"
#include "strictjson.h"

#include <algorithm>
#include <cerrno>
#include <set>
#include <stdexcept>
#include <sys/stat.h>

namespace GetDAQ {
using namespace NativeEccFilesystemProof;
using namespace NativeEccCoordinatorRecoveryInternal;

void NativeEccCoordinator::AbortFailedReceivers(const std::string &id) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock()) throw std::runtime_error("Native ECC command busy; receiver cleanup was not queued");
    ++p.observationEpoch;
    OperationId(id);p.CheckBinding();const auto status=Status();
    if(!status.outcomeUnknown || !p.acquisition || !p.productionStopProof || p.receiverAbortAttempted ||
       p.stoppedFailureOperation!=status.operationId)
        throw std::logic_error("Receiver cleanup requires the same retained failed acquisition and its completed production stop; no restart adoption or replay");
    p.CheckProductionStopProof();
    const auto path=p.options.auditRoot+"/operations/"+id;
    p.journal->CheckNewId(id,GetDAQ::ConfigurationSha256("getdaq.native-ecc-receiver-abort:1\n"+p.snapshotHash+'\n'+id));
    p.CheckUnusedPath(id);CreateHardwareAuditDirectory(path);Directory directory(path);HardwareAudit audit(path+"/operation.jsonl");
    const auto binding="\"operation_id\":"+JsonString(id)+",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"ecc_unknown_preserved\":true,\"acquisition_complete\":false";
    const auto intent="{\"schema_version\":1,\"operation\":\"abort-failed-receivers\",\"phase\":\"intent\","+binding+"}";
    audit.Append(intent);ImmutableFile proof(path+"/operation.jsonl",intent+'\n');p.receiverAbortAttempted=true;
    try {
        p.data.AbortFailedReceivers(path+"/data",status.operationId);
        p.CheckBinding();directory.Check();proof.Check();p.CheckProductionStopProof();
        const auto complete="{\"phase\":\"backend_complete\","+binding+"}";
        audit.Append(complete);p.receiverAbortDirectory.reset(new Directory(path));
        p.receiverAbortProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.receiverAbortOperation=id;p.receiverAbortStopProof=p.productionStopProof;p.CheckReceiverAbortProof();
    } catch(...) {try {audit.Append("{\"phase\":\"failed\","+binding+",\"automatic_retry\":false}");} catch(...) {}throw;}
}
void NativeEccCoordinator::RetireFailedReceivers(const std::string &id) {
    auto &p=*fImpl;std::unique_lock<std::mutex> lock(p.command,std::try_to_lock);
    if(!lock.owns_lock())throw std::runtime_error("Native ECC command busy; receiver retirement was not queued");
    ++p.observationEpoch;OperationId(id);p.CheckBinding();const auto status=Status();
    const bool closedContext=p.acquisition && (p.acquisition->GetState()==SystemAcquisitionClient::State::Broken ||
        p.acquisition->GetState()==SystemAcquisitionClient::State::Failed);
    if(!status.outcomeUnknown || !p.acquisition || !p.preparation || !p.productionStopProof ||
       p.stoppedFailureOperation!=status.operationId || p.receiverRetirementAttempted ||
       !closedContext ||
       p.preparation->GetState()!=SystemPreparationClient::State::Prepared)
        throw std::logic_error("Receiver retirement requires the same failed acquisition, confirmed connection cleanup and Abort; no replay or cold takeover");
    p.CheckFailureConnectionsProof();p.CheckReceiverAbortProof();
    // A later successful Broken readback deliberately retains State::Failed.
    // The original complete close proof and every original cached sender must
    // still confirm closed; this check never queries/adopts remote senders.
    for(const auto &client:p.acquisition->ObservationClients()) {
        const auto sender=client->Snapshot();
        const bool retired=sender.outcomeUnknown && sender.failureRetirementAcknowledged;
        if(retired){p.CheckSourceRetirementProof();client->VerifyIceFailureRetirement();}
        else if(sender.outcomeUnknown || (sender.state!=DataSenderSnapshot::Fresh && sender.state!=DataSenderSnapshot::Disconnected))
            throw std::logic_error("Receiver retirement requires every original sender confirmed closed; no UNKNOWN adoption");
    }
    p.journal->CheckNewId(id,GetDAQ::ConfigurationSha256("getdaq.native-ecc-receiver-retirement:1\n"+p.snapshotHash+'\n'+id));
    p.CheckUnusedPath(id);const auto path=p.options.auditRoot+"/operations/"+id;
    CreateHardwareAuditDirectory(path);Directory directory(path);HardwareAudit audit(path+"/operation.jsonl");
    const auto fields="\"operation_id\":"+JsonString(id)+",\"operation\":\"retire-failed-receivers\",\"failed_operation_id\":"+JsonString(status.operationId)+
        ",\"production_stop_operation_id\":"+JsonString(p.productionStopOperation)+",\"connection_cleanup_operation_id\":"+JsonString(p.failureBreakupOperation)+
        ",\"receiver_abort_operation_id\":"+JsonString(p.receiverAbortOperation)+",\"snapshot_sha256\":"+JsonString(p.snapshotHash)+
        ",\"configuration_sha256\":"+JsonString(p.bound.configurationSha256)+",\"artifact_sha256\":"+JsonString(p.bound.artifactSha256)+
        ",\"parent_ecc_unknown_preserved\":true,\"receiver_sealed\":false,\"run_complete\":false,\"new_receivers_started\":false";
    const auto intent="{\"phase\":\"intent\","+fields+"}";audit.Append(intent);ImmutableFile intentProof(path+"/operation.jsonl",intent+'\n');
    p.receiverRetirementAttempted=true;
    try {
        p.data.RetireFailedReceivers(path+"/data",status.operationId);
        p.CheckBinding();p.CheckFailureConnectionsProof();p.CheckReceiverAbortProof();directory.Check();intentProof.Check();
        p.data.VerifyStopped(); // Retired is stopped for a new data cycle, not sealed.
        p.CheckBinding();p.CheckFailureConnectionsProof();p.CheckReceiverAbortProof();directory.Check();intentProof.Check();
        const auto complete="{\"phase\":\"backend_complete\","+fields+",\"original_failed_data_sessions_retired\":true}";
        audit.Append(complete);p.receiverRetirementDirectory.reset(new Directory(path));
        p.receiverRetirementProof.reset(new ImmutableFile(path+"/operation.jsonl",intent+'\n'+complete+'\n'));
        p.receiverRetirementOperation=id;
    } catch(...) {try{audit.Append("{\"phase\":\"failed\","+fields+",\"automatic_retry\":false}");}catch(...){}throw;}
}

} // namespace GetDAQ
