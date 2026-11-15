#include "ecccontroljobs_p.h"
#include <stdexcept>
namespace GetDAQ {
void EccControlJobs::Impl::CheckHandoffCompletion(const EccControlRequest &r,bool replay) const {
    const auto original=jobs.find(r.failedOperationId);
    if(!handoffFenced || r.failedOperationId!=handoffOperationId || original==jobs.end() || original->second.phase!="failed" ||
       (original->second.request.command!="handoff-data-control" && original->second.request.command!="retire-data-plans") ||
       original->second.request.instanceId!=r.instanceId || original->second.request.clientId!=r.clientId || original->second.request.clientEpoch!=r.clientEpoch)
        throw std::runtime_error("Completion requires the exact original failed handoff and client binding");
    if(!handoffCompletionOperationId.empty() && handoffCompletionOperationId!=r.operationId) {
        const auto prior=jobs.find(handoffCompletionOperationId);
        if(prior==jobs.end() || prior->second.phase!="failed")
            throw std::runtime_error("Previous explicit handoff completion is still pending or already complete");
    }
    if(!replay) {
        if(original->second.request.instanceId!=instance || inputFault || storageFault || alarmHandled || receiverHandled)
            throw std::runtime_error("Completion requires the retained live controller without independent hardware/input/storage holds");
        controller.CheckHandoffCompletionBoundary();
    }
}
std::string EccControlJobs::Impl::CheckAlarmRecoveryBinding(const EccControlRequest &r,bool replay) const {
    const bool preparation=r.command=="reconcile-preparation-with-alarm";
    if(!configuration.AlarmStopEnabled() || !alarmHandled || (preparation ? receiverHandled : !receiverHandled) || alarmOperationId!=r.alarmOperationId ||
       alarmObservation=="null" || (!replay && (alarmPhase!="complete" || (!preparation && receiverPhase!="complete"))))
        throw std::runtime_error("Explicit alarm recovery requires the same completed original alarm and receiver policy stops");
    const auto stopped=jobs.find(alarmOperationId);
    if(stopped==jobs.end() || stopped->second.phase!="complete" || stopped->second.request.command!="stop-production-after-failure")
        throw std::runtime_error("Alarm recovery requires the original UNKNOWN acquisition stop, not a normal or cold alarm");
    const auto state=DecodeEccControlControllerStatus(stopped->second.resultJson);
    const auto observed=ParseStrictJson(alarmObservation);
    if(!state.outcomeUnknown || (preparation && state.state!=2) || state.operationId!=r.failedOperationId ||
       !Boolean(observed.At("controller_outcome_unknown")) || !Boolean(observed.At("stop_requested")) ||
       String(observed.At("controller_operation_id"))!=r.failedOperationId)
        throw std::runtime_error("Alarm recovery original operation binding differs");
    return ConfigurationSha256("getdaq.ecc-alarm-observation:1\n"+alarmObservation);
}
std::string EccControlJobs::Impl::CheckStoppedAlarmBinding(const EccControlRequest &r,bool replay) const {
    if(!configuration.AlarmStopEnabled() || !alarmHandled || receiverHandled || inputFault || storageFault || alarmObservation=="null" || alarmOperationId!=r.alarmOperationId)
        throw std::runtime_error("Stopped alarm release requires only the original alarm hold");
    const auto observed=ParseStrictJson(alarmObservation);
    if(Boolean(observed.At("controller_outcome_unknown")) || !Boolean(observed.At("stop_requested")))
        throw std::runtime_error("Stopped alarm release cannot acknowledge UNKNOWN production");
    if(!alarmOperationId.empty()) {
        const auto stopped=jobs.find(alarmOperationId);
        if(stopped==jobs.end() || stopped->second.phase!="complete" || stopped->second.request.command!="stop" || (!replay && alarmPhase!="complete"))
            throw std::runtime_error("Stopped alarm release requires the original completed normal Stop");
        const auto state=DecodeEccControlControllerStatus(stopped->second.resultJson);
        if(state.state!=4 || state.transition || state.outcomeUnknown || state.needsReconciliation || state.operationId!=r.failedOperationId || r.failedOperationId!=alarmOperationId)
            throw std::runtime_error("Stopped alarm release original Stop binding differs");
    } else if((!replay && alarmPhase!="held-no-run") || Unsigned(observed.At("controller_state"))!=4 || String(observed.At("controller_operation_id"))!=r.failedOperationId)
        throw std::runtime_error("Stopped alarm release requires the original known Ready observation");
    const auto original=jobs.find(r.failedOperationId);
    if(original==jobs.end() || original->second.phase!="complete")throw std::runtime_error("Original known operation completion missing");
    const auto proof=requestProofs.find(r.failedOperationId);
    if(proof==requestProofs.end())throw std::runtime_error("Original known operation request missing");
    proof->second->Check();
    return ConfigurationSha256("getdaq.ecc-alarm-observation:1\n"+alarmObservation);
}
} // namespace GetDAQ
