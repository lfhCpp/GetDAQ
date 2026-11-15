#include "ecccontroljobs_p.h"
#include <stdexcept>
namespace GetDAQ {
void EccControlJobs::Impl::Run(const EccControlRequest &r,const std::shared_ptr<EccControlRequestProof> &proof){
        std::unique_lock<std::mutex> operation(operationMutex); const auto fields=",\"operation_id\":"+EncodeJsonString(r.operationId)+",\"request_key\":"+EncodeJsonString(EccControlRequestKey(r));
        const bool negative=r.command=="stop-production-after-failure" || r.command=="abort-failed-receivers" || r.command=="retire-failed-receivers" || r.command=="retire-failed-sources" || r.command=="unprepare-acquisition-after-failure" || (r.command=="unprepare-after-failure" || r.command=="breakup-after-failure");
        try {
            const bool stoppedAlarmRelease=r.command=="release-stopped-alarm";
            const bool preparationAlarmRecovery=r.command=="reconcile-preparation-with-alarm";
            const bool alarmRecovery=r.command=="reconcile-acquisition-with-alarm" || preparationAlarmRecovery;
            bool resetReceiverPolicy=false;std::string recoveredReceiverStop,recoveredAlarmStop,alarmDigest;
            if(r.command=="reconcile-description" || r.command=="reconcile-preparation" || (r.command=="reconcile-acquisition" || r.command=="reconcile-acquisition-with-sources") || alarmRecovery) {
                std::lock_guard<std::mutex> state(stateMutex);
                if((alarmHandled && !alarmRecovery) || inputFault || storageFault)throw std::runtime_error("Acquisition recovery cannot clear an independent alarm/input/storage hold");
                if(alarmRecovery){alarmDigest=CheckAlarmRecoveryBinding(r);recoveredAlarmStop=alarmOperationId;}
                if(receiverHandled && r.command!="reconcile-acquisition" && r.command!="reconcile-acquisition-with-sources" && !alarmRecovery)
                    throw std::runtime_error("Description/preparation recovery cannot clear an independent receiver hold");
                if(receiverHandled) {
                    const auto stopped=jobs.find(receiverOperationId);
                    if(stopped==jobs.end() || stopped->second.phase!="complete" || receiverEvidence=="null")
                        throw std::runtime_error("Acquisition recovery requires the confirmed original receiver policy stop");
                    JsonLimits limits;limits.stringBytes=1024*1024;
                    const auto held=ParseStrictJson(receiverEvidence,limits);
                    const auto bound=DecodeEccControlSystemStatus(String(held.At("bound_status")));
                    if(bound.controller.operationId!=r.failedOperationId)throw std::runtime_error("Recovery receiver hold belongs to a different original operation");
                    resetReceiverPolicy=true;recoveredReceiverStop=receiverOperationId;
                }
            }
            if(stoppedAlarmRelease){std::lock_guard<std::mutex> state(stateMutex);alarmDigest=CheckStoppedAlarmBinding(r);recoveredAlarmStop=alarmOperationId;}
            Check(negative); proof->Check(); Record("running",fields,negative); { std::lock_guard<std::mutex> lock(stateMutex); jobs.at(r.operationId).phase="running"; }
            if(inputFault && negative) {
                const auto original=controller.Status().operationId;
                SupervisorOperationId(original);
                if(!inputFaultRecorded){Record("input-fault-held",",\"failed_operation_id\":"+EncodeJsonString(original)+",\"input_binding_sha256\":"+EncodeJsonString(configuration.BindingSha256()),true);inputFaultRecorded=true;}
                controller.HoldControlInputFailure(original);
            }
            if(stoppedAlarmRelease) controller.ReleaseStoppedAlarm(r.operationId,r.failedOperationId);
            else if (r.command=="stop-production-after-failure") controller.StopProductionAfterFailure(r.operationId);
            else if(r.command=="abort-failed-receivers") controller.AbortFailedReceivers(r.operationId);
            else if(r.command=="retire-failed-receivers") controller.RetireFailedReceivers(r.operationId);
            else if(r.command=="retire-failed-sources") controller.RetireFailedSources(r.operationId);
            else if(r.command=="unprepare-acquisition-after-failure") controller.UnprepareAcquisitionAfterFailure(r.operationId);
            else if(r.command=="unprepare-after-failure") controller.UnprepareAfterFailure(r.operationId);
            else if(r.command=="breakup-after-failure") controller.BreakupAfterFailure(r.operationId);
            else if(r.command=="reconcile-description") controller.ReconcileDescription(r.operationId,r.failedOperationId);
            else if(r.command=="reconcile-preparation" || preparationAlarmRecovery) controller.ReconcilePreparation(r.operationId,r.failedOperationId);
            else if(r.command=="reconcile-acquisition-with-sources") controller.ReconcileAcquisitionWithSources(r.operationId,r.failedOperationId);
            else if(r.command=="reconcile-acquisition" || alarmRecovery) controller.ReconcileAcquisition(r.operationId,r.failedOperationId);
            else if (r.command=="resume-catalog-replacement") {
                EccControlRequest original;
                {
                    std::lock_guard<std::mutex> state(stateMutex);
                    const auto found=jobs.find(r.failedOperationId);
                    if (catalogRecoveryOriginalId!=r.failedOperationId || found==jobs.end() || found->second.phase!="failed" ||
                        found->second.request.command!="catalog" || !found->second.request.catalogReplaceExisting ||
                        found->second.request.instanceId!=r.instanceId || found->second.request.snapshotSha256!=r.snapshotSha256 ||
                        found->second.request.clientId!=r.clientId || found->second.request.clientEpoch!=r.clientEpoch)
                        throw std::runtime_error("Catalog recovery requires the exact original failed replacement operation in this live ECC process");
                    original=found->second.request;
                }
                const auto hardware=controller.ControlStatus();
                if (hardware.state!=EccState::Prepared || hardware.transition || hardware.error || hardware.needsReconciliation || hardware.outcomeUnknown ||
                    inputFault || storageFault || alarmHandled || receiverHandled)
                    throw std::runtime_error("Catalog recovery is blocked by an independent ECC/input/storage/alarm/receiver hold");
                const auto originalPath=configuration.ControllerOptions().auditRoot+"/operations/"+r.failedOperationId+"/catalog_publish";
                if (!data.HasRetainedCatalogPublishAudit(originalPath))
                    throw std::runtime_error("Original Publish proof is not retained by this live ECC process; cold recovery is unsupported");
                const auto path=configuration.ControllerOptions().auditRoot+"/operations/"+r.operationId;
                CreateDirectory(path);
                data.ResumeCatalogReplacement(path+"/catalog-resume",originalPath,original.catalogExperiment,original.catalogRunNumber);
            }
            else if (r.command=="catalog") {
                const auto status=controller.Status();
                if (status.state!=EccState::Prepared || status.needsReconciliation || status.outcomeUnknown || !data.CanCatalog()) throw std::runtime_error("Catalog requires Prepared and all current sealed sessions with fixed archive roots");
                const auto path=configuration.ControllerOptions().auditRoot+"/operations/"+r.operationId; CreateDirectory(path);
                data.CatalogReceivers(path+"/catalog",r.catalogExperiment,r.catalogRunNumber,r.catalogReplaceExisting);
            } else if ((r.command=="handoff-data-control" || r.command=="retire-data-plans")) {
                const auto status=controller.Status();
                if ((status.state!=EccState::Described && status.state!=EccState::Idle) || status.needsReconciliation || status.outcomeUnknown || !NormalSealedData(data.Status()))
                    throw std::runtime_error("Data control handoff requires known Idle/Described after normal Reset and a sealed original cycle");
                const auto path=configuration.ControllerOptions().auditRoot+"/operations/"+r.operationId;CreateDirectory(path);
                if (r.command=="retire-data-plans") data.RetirePlans(path+"/data-handoff",r.successorReceiverPlans,r.successorScopeSha256,r.successorEpoch);
                else data.HandoffControl(path+"/data-handoff",r.successorScopeSha256,r.successorEpoch);
            } else if(r.command=="complete-data-handoff") {
                {std::lock_guard<std::mutex> state(stateMutex);CheckHandoffCompletion(r);}
                const auto path=configuration.ControllerOptions().auditRoot+"/operations/"+r.operationId;CreateDirectory(path);
                data.CompletePendingHandoff(path+"/data-handoff");
            } else if(r.command=="reprepare-cobo")controller.ReprepareCoBo(r.operationId,r.coboId);
            else if(r.command=="configure-cobo")controller.ConfigureCoBo(r.operationId,r.coboId);
            else if(r.command=="set-cobo-led")controller.SetCoBoLed(r.operationId,r.coboId,r.ledAsadId,r.ledType,r.ledState);
            else if(r.command=="blink-cobo-leds")controller.BlinkCoBoLeds(r.operationId,r.coboId);
            else if(r.command=="flip-cobo-led")controller.FlipCoBoLed(r.operationId,r.coboId,r.ledFlipId);
            else if(r.command=="pulse-cobo-led")controller.PulseCoBoLed(r.operationId,r.coboId,r.ledPulseId,r.ledPulsePeriodMs);
            else if(r.command=="trigger-asad-pulser")controller.TriggerAsAdPulser(r.operationId,r.coboId,r.pulserVoltageMv);
            else controller.Execute(Command(r.command),r.operationId);
            Check(negative); proof->Check(); auto result=ControllerJson(); std::string phase="complete";
            auto resetFields=((r.command=="reconcile-acquisition" || r.command=="reconcile-acquisition-with-sources") || (alarmRecovery && !preparationAlarmRecovery)) ? ",\"receiver_policy_reset\":"+std::string(resetReceiverPolicy ? "true" : "false")+
                ",\"receiver_stop_operation_id\":"+EncodeJsonString(recoveredReceiverStop) : "";
            if(alarmRecovery || stoppedAlarmRelease)resetFields+=",\"alarm_policy_reset\":true,\"alarm_stop_operation_id\":"+EncodeJsonString(recoveredAlarmStop)+
                ",\"alarm_observation_sha256\":"+EncodeJsonString(alarmDigest);
            Record("complete",fields+",\"message\":\"\",\"result\":"+EncodeJsonString(result)+resetFields,negative);
            proof->ReleaseReview();
            if(r.command=="complete-data-handoff")requestProofs.at(r.failedOperationId)->ReleaseReview();
            std::lock_guard<std::mutex> lock(stateMutex); jobs.at(r.operationId).phase.swap(phase); jobs.at(r.operationId).resultJson.swap(result); active=false;
            if(controller.Status().outcomeUnknown || controller.Status().needsReconciliation || data.NeedsReconciliation())needsReconciliation=true;
            else if(r.command=="reconcile-description" || r.command=="reconcile-preparation" || (r.command=="reconcile-acquisition" || r.command=="reconcile-acquisition-with-sources") || alarmRecovery || stoppedAlarmRelease) {
                needsReconciliation=false;message="Original failure reconciled by bound read-only verification";
                if(alarmRecovery || stoppedAlarmRelease){alarmHandled=false;alarmOperationId.clear();alarmObservation="null";alarmPhase="watching";}
                if(resetReceiverPolicy){receiverHandled=false;receiverOperationId.clear();receiverEvidence="null";receiverPhase="watching";}
            }
            if (r.command=="resume-catalog-replacement") {
                catalogRecoveryOriginalId.clear();catalogRecoveryInProgress=false;
                if (!controller.Status().outcomeUnknown && !controller.Status().needsReconciliation && !data.NeedsReconciliation()) {
                    needsReconciliation=false;message="Catalog replacement publish recovered and all node receipts verified";
                }
            }
            operation.unlock();
            if (r.operationId==alarmOperationId) { alarmPhase="complete"; message="Hardware alarm held; automatic stop complete, inspect before recovery"; }
            if (r.operationId==receiverOperationId) { receiverPhase="complete"; message="Receiver failure held; production stop complete, data/run recovery still required"; }
        } catch (...) {
            std::string failure; try { throw; } catch (const std::exception &e) { failure=EccErrorText(e.what()); } catch (...) { failure="Non-standard ECC command failure"; }
            const auto result=ControllerJson(); bool stored=true; try { Record("failed",fields+",\"message\":"+EncodeJsonString(failure)+",\"result\":"+EncodeJsonString(result),negative); } catch (...) { stored=false; }
            bool catalogRecoveryEligible=false;
            if (r.command=="catalog" && r.catalogReplaceExisting && stored) {
                try {
                    const auto hardware=controller.ControlStatus();
                    const auto originalPath=configuration.ControllerOptions().auditRoot+"/operations/"+r.operationId+"/catalog_publish";
                    catalogRecoveryEligible=hardware.state==EccState::Prepared && !hardware.transition && !hardware.error &&
                        !hardware.needsReconciliation && !hardware.outcomeUnknown && !inputFault && !storageFault && !alarmHandled && !receiverHandled &&
                        data.HasRetainedCatalogPublishAudit(originalPath);
                } catch (...) { catalogRecoveryEligible=false; }
            }
            std::lock_guard<std::mutex> lock(stateMutex); auto &op=jobs.at(r.operationId); op.phase=stored ? "failed" : "unknown"; op.message=failure; op.resultJson=result; active=false;
            const auto status=controller.Status(); if (!stored || status.outcomeUnknown || status.needsReconciliation || data.NeedsReconciliation()) needsReconciliation=true;
            if (r.command=="resume-catalog-replacement") {catalogRecoveryOriginalId.clear();catalogRecoveryInProgress=false;}
            else if (r.command=="catalog" && catalogRecoveryEligible && stored && needsReconciliation) catalogRecoveryOriginalId=r.operationId;
            if (r.operationId==alarmOperationId) { alarmPhase=stored ? "failed" : "unknown"; message="Hardware alarm held; automatic stop unconfirmed, no replay"; }
            if (r.operationId==receiverOperationId) { receiverPhase=stored ? "failed" : "unknown"; message="Receiver failure held; production stop unconfirmed, no replay"; }
            if (operation.owns_lock()) operation.unlock();
        }
    }
}
