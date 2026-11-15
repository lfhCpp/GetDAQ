#include "datasenderclient_p.h"
#include "../../common/text/utf8.h"
#include "hardwareaudit.h"
#include "hardwareclient.h"
#include "strictjson.h"
#include <SourceControl.h>
#include <chrono>
#include <mutex>
#include <set>
#include <stdexcept>

namespace GetDAQ {
namespace DataSenderClientInternal {
void CheckRetirement(const FailedSourceRetirement &receipt,const SourceInfo &info) {
    const auto &source=receipt.source;
    if(receipt.version!=1 || !receipt.issued || !receipt.producerIdleVerified || !receipt.dataConnectionClosed || !receipt.retired ||
       source.version!=1 || source.sourceId!=info.sourceId || source.instanceId!=info.instanceId || source.receiverAddress!=info.receiverAddress ||
       source.receiverSession.empty() || source.receiverSession!=info.receiverSession || source.state!="unknown" || source.state!=info.state ||
       source.error!=info.error || source.issuedBytes!=info.issuedBytes || source.locallySentBytes!=info.locallySentBytes ||
       !source.failureStopIssued || !source.failureStopAcknowledged || !info.failureStopIssued || !info.failureStopAcknowledged ||
       source.finishIssued!=info.finishIssued || source.sourceEndAcknowledged!=info.sourceEndAcknowledged)
        throw std::runtime_error("Failed source retirement receipt lacks the original stopped/closed UNKNOWN binding");
}
}

using DataSenderClientInternal::CheckRetirement;


void DataSenderClient::StopIceProductionAfterFailure(const std::string &path) {
    StopIceProductionAfterFailure(path,false);
}
void DataSenderClient::StopIceProductionAfterFailure(const std::string &path,bool ownerUnknown) {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto prior=p.Status();
    const bool retained=prior.state==DataSenderSnapshot::Connected || prior.state==DataSenderSnapshot::Running || prior.state==DataSenderSnapshot::Stopped || prior.state==DataSenderSnapshot::Unknown;
    if(p.options.protocol!="ICE" || (!ownerUnknown && prior.state!=DataSenderSnapshot::Unknown) || !retained || prior.failureStopIssued || prior.receiverSession.empty()) throw std::logic_error("ICE failure stop requires same unknown owner/source with no prior attempt");
    p.Validate(true);HardwareAudit audit(path);
    audit.Append("{\"phase\":\"failure-stop-intent\",\"receiver_session\":"+JsonString(prior.receiverSession)+",\"source_instance_id\":"+JsonString(prior.sourceInstanceId)+",\"owner_outcome_unknown_confirmed\":"+(ownerUnknown ? "true" : "false")+",\"source_end_requested\":false}");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.state=DataSenderSnapshot::Unknown;p.snapshot.outcomeUnknown=true;p.snapshot.failureStopIssued=true;++p.snapshot.issuedCalls;}
    try {
        p.sourceProxy->stopIceSourceAfterFailure(prior.receiverSession);
        const auto info=p.sourceProxy->describeSource();
        if(info.instanceId!=prior.sourceInstanceId || info.receiverSession!=prior.receiverSession || info.state!="unknown" || !info.failureStopIssued || !info.failureStopAcknowledged) throw std::runtime_error("ICE failure stop lacks bound retained-UNKNOWN result");
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureStopAcknowledged=true;++p.snapshot.acknowledgedCalls;}
        audit.Append("{\"phase\":\"failure-stop-acknowledged\",\"outcome_unknown\":true,\"receiver_sealed\":false}");
    } catch(...) {try {audit.Append("{\"phase\":\"unknown\",\"automatic_retry\":false}");} catch(...) {}throw;}
}
void DataSenderClient::ValidateIceFailureRetirement() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto prior=p.Status();
    if(p.options.protocol!="ICE" || prior.state!=DataSenderSnapshot::Unknown || !prior.outcomeUnknown ||
       !prior.failureStopIssued || !prior.failureStopAcknowledged || prior.failureRetirementIssued || prior.receiverSession.empty())
        throw std::logic_error("ICE source retirement requires the same stopped UNKNOWN stream and no prior attempt");
    p.Validate(true);
    if(!DataSourceRecoveryPrx::checkedCast(p.sourceProxy))throw std::runtime_error("ICE source lacks optional failed-stream retirement capability");
}
void DataSenderClient::RetireIceSourceAfterFailure(const std::string &path) {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto prior=p.Status();
    if(p.options.protocol!="ICE" || prior.state!=DataSenderSnapshot::Unknown || !prior.outcomeUnknown ||
       !prior.failureStopIssued || !prior.failureStopAcknowledged || prior.failureRetirementIssued || prior.receiverSession.empty())
        throw std::logic_error("ICE source retirement requires the same stopped UNKNOWN stream and no prior attempt");
    p.Validate(true);const auto recovery=DataSourceRecoveryPrx::checkedCast(p.sourceProxy);
    if(!recovery)throw std::runtime_error("ICE source lacks optional failed-stream retirement capability");
    HardwareAudit audit(path);audit.Append("{\"phase\":\"failed-source-retirement-intent\",\"receiver_session\":"+JsonString(prior.receiverSession)+
        ",\"source_instance_id\":"+JsonString(prior.sourceInstanceId)+",\"source_end_requested\":false,\"original_unknown_preserved\":true}");
    {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureRetirementIssued=true;++p.snapshot.issuedCalls;}
    try {
        const auto receipt=recovery->retireFailedIceStream(prior.receiverSession);const auto info=p.sourceProxy->describeSource();CheckRetirement(receipt,info);
        if(info.instanceId!=prior.sourceInstanceId || info.receiverSession!=prior.receiverSession)throw std::runtime_error("Failed source retirement changed original identity/session");
        {std::lock_guard<std::mutex> lock(p.statusMutex);p.snapshot.failureRetirementAcknowledged=true;++p.snapshot.acknowledgedCalls;}
        audit.Append("{\"phase\":\"failed-source-retirement-acknowledged\",\"producer_idle_verified\":true,\"data_connection_closed\":true,\"source_end_requested\":false,\"outcome_unknown\":true,\"original_error\":"+JsonString(info.error)+
            ",\"issued_bytes\":"+JsonString(std::to_string(info.issuedBytes))+",\"locally_sent_bytes\":"+JsonString(std::to_string(info.locallySentBytes))+
            ",\"source_finish_issued\":"+(info.finishIssued ? "true" : "false")+",\"source_end_acknowledged\":"+(info.sourceEndAcknowledged ? "true" : "false")+"}");
        p.failureRetirementChain=audit.ChainSha256();
    } catch(...){try{audit.Append("{\"phase\":\"unknown\",\"automatic_retry\":false}");}catch(...){}throw;}
}
std::string DataSenderClient::VerifyIceFailureRetirement() {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);const auto status=p.Status();
    if(p.options.protocol!="ICE" || !status.outcomeUnknown || !status.failureRetirementAcknowledged || p.failureRetirementChain.empty())
        throw std::logic_error("Missing original acknowledged ICE source retirement");
    p.Validate(true);const auto recovery=DataSourceRecoveryPrx::checkedCast(p.sourceProxy);
    if(!recovery)throw std::runtime_error("Original source retirement capability missing");
    const auto info=p.sourceProxy->describeSource();CheckRetirement(recovery->describeFailedSourceRetirement(info.receiverSession),info);
    return "{\"sender_id\":"+JsonString(p.options.senderId)+",\"source_instance_id\":"+JsonString(info.instanceId)+
        ",\"receiver_session\":"+JsonString(info.receiverSession)+",\"state\":\"unknown\",\"original_error\":"+JsonString(info.error)+
        ",\"issued_bytes\":"+JsonString(std::to_string(info.issuedBytes))+",\"locally_sent_bytes\":"+JsonString(std::to_string(info.locallySentBytes))+
        ",\"failure_stop_acknowledged\":true,\"data_connection_closed\":true,\"failure_retired\":true,\"source_finish_issued\":"+(info.finishIssued ? "true" : "false")+
        ",\"source_end_acknowledged\":"+(info.sourceEndAcknowledged ? "true" : "false")+"}";
}
std::string DataSenderClient::CompletedFailureRetirementAuditChainSha256() const {
    auto &p=*fImpl;std::lock_guard<std::mutex> operation(p.operationMutex);
    if(!p.Status().failureRetirementAcknowledged || p.failureRetirementChain.empty())throw std::logic_error("No completed source retirement audit");
    return p.failureRetirementChain;
}
DataSourceObservation DataSenderClient::ObserveSource(unsigned int timeout) const {
    if((timeout && timeout<50) || timeout>2000)throw std::invalid_argument("Source observation timeout must be 0 or 50..2000ms");
    auto &p=*fImpl;const auto local=p.Status();DataSourceObservation result;
    result.senderId=p.options.senderId;result.protocol=p.options.protocol;result.localState=unsigned(local.state);
    result.sourceInstanceId=local.sourceInstanceId;result.receiverSession=local.receiverSession;
    if(p.options.protocol!="ICE") {result.phase="legacy";result.message="Legacy sender has no source-state capability";return result;}
    if(local.state==DataSenderSnapshot::Fresh || local.sourceInstanceId.empty() || local.receiverSession.empty())return result;
    if(local.state==DataSenderSnapshot::Connecting || local.state==DataSenderSnapshot::Starting || local.state==DataSenderSnapshot::Stopping || local.state==DataSenderSnapshot::Disconnecting) {result.phase="busy";return result;}
    if(!timeout) {result.phase="unavailable";result.message="Whole-source observation budget exceeded; no RPC issued";return result;}
    try {
        DataSenderClientInternal::Parser observationParser{int(timeout)};
        Ice::EncodingVersion encoding;encoding.major=1;encoding.minor=Ice::Byte(p.options.encodingMinor);
        // Discovery is permitted on a different read-only connection. Keep the
        // original command proxy/owner intact; never install this observation proxy.
        auto proxy=observationParser.communicator->stringToProxy(p.options.controlProxy)->ice_twoway()->ice_collocationOptimized(false)
            ->ice_timeout(int(timeout))->ice_encodingVersion(encoding)->ice_connectionId("getdaq-source-observation");
        const auto info=DataSourceControlPrx::uncheckedCast(proxy)->describeSource();
        const std::set<std::string> states={"fresh","connecting","connected","starting","running","stopping","stopped","disconnecting","disconnected","unknown"};
        if(info.version!=1 || info.sourceId!=p.options.senderId || info.receiverAddress!=p.options.receiverAddress || info.instanceId!=local.sourceInstanceId || info.receiverSession!=local.receiverSession ||
           !states.count(info.state) || info.issuedBytes<0 || info.locallySentBytes<0 || info.locallySentBytes>info.issuedBytes || info.error.size()>512 ||
           (info.sourceEndAcknowledged && (!info.finishIssued || !info.stopAcknowledged || !info.drainAcknowledged || (info.state!="disconnected" && info.state!="unknown"))) ||
           (info.failureStopAcknowledged && !info.failureStopIssued))throw std::runtime_error("Observed source capability/instance/session/result differs; no adoption");
        ValidateJsonUtf8(info.error);result.phase="available";result.state=info.state;result.error=info.error;
        result.issuedBytes=std::uint64_t(info.issuedBytes);result.locallySentBytes=std::uint64_t(info.locallySentBytes);
        result.startIssued=info.startIssued;result.stopAcknowledged=info.stopAcknowledged;result.drainAcknowledged=info.drainAcknowledged;
        result.finishIssued=info.finishIssued;result.sourceEndAcknowledged=info.sourceEndAcknowledged;result.failureStopIssued=info.failureStopIssued;result.failureStopAcknowledged=info.failureStopAcknowledged;
    }catch(const std::exception &e) {result.phase="unavailable";result.message=EccErrorText(e.what());}
    catch(...) {result.phase="unavailable";result.message="Nonstandard read-only source observation failure";}
    result.observedAtUnixMs=std::uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    return result;
}
}
