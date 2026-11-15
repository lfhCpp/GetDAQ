#include "eccclientstore_observationpolicy.h"
#include <QString>
#include <stdexcept>
namespace {
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
bool Terminal(const GetDAQ::EccControlOperation &op) { return op.phase=="complete" || op.phase=="failed"; }
}
void GetDAQ::ApplyEccClientObservation(const EccControlOperation &op,
    const QMap<QString, EccControlRequest> &requests,
    QMap<QString, EccControlOperation> &observations,
    QSet<QString> &heldOperations, bool &reconciliation)
{
    const auto id=QString::fromStdString(op.request.operationId);
    Require(requests.contains(id) && EncodeEccControlRequest(requests[id])==EncodeEccControlRequest(op.request), "ECC client observation lacks matching original intent");
    if (observations.contains(id) && Terminal(observations[id])) {
        Require(!Terminal(op) || EncodeEccControlOperation(observations[id])==EncodeEccControlOperation(op), "ECC client terminal observation changed"); return; // 忽略晚到的accepted/running，不回退已确认结果。
    }
    const auto status=op.resultJson.empty() ? EccControlControllerStatus() : DecodeEccControlControllerStatus(op.resultJson);
    if((op.request.command=="reprepare-cobo" || op.request.command=="set-cobo-led" || op.request.command=="blink-cobo-leds" || op.request.command=="flip-cobo-led" || op.request.command=="pulse-cobo-led") && op.phase=="complete")
        Require(status.state==3 && !status.transition && !status.error && !status.outcomeUnknown && !status.needsReconciliation,"Maintenance completion must retain known Prepared");
    if((op.request.command=="retire-data-plans" || op.request.command=="complete-data-handoff") && op.phase=="complete")
        Require((status.state==1 || status.state==2) && !status.transition && !status.outcomeUnknown && !status.error,"Retirement completion lacks known Idle/Described controller result");
    if(op.request.command=="release-stopped-alarm" && op.phase=="complete") {
        Require(status.state==2 && !status.outcomeUnknown && !status.needsReconciliation && !status.transition &&
            status.operationId==op.request.operationId,"Stopped alarm release lacks verified Described result");
        // Known Stop/Configure did not create a client UNKNOWN hold. Do not
        // clear unrelated failed cleanup or later hardware failures here.
        for(auto it=heldOperations.begin();it!=heldOperations.end();) {
            const auto &attempt=observations[*it];
            if(attempt.phase=="failed" && attempt.request.command==op.request.command &&
               attempt.request.instanceId==op.request.instanceId && attempt.request.snapshotSha256==op.request.snapshotSha256 &&
               attempt.request.clientId==op.request.clientId && attempt.request.clientEpoch==op.request.clientEpoch &&
               attempt.request.failedOperationId==op.request.failedOperationId && attempt.request.alarmOperationId==op.request.alarmOperationId &&
               !attempt.resultJson.empty() && !DecodeEccControlControllerStatus(attempt.resultJson).outcomeUnknown)
                it=heldOperations.erase(it);
            else ++it;
        }
    }
    if(((op.request.command=="reconcile-acquisition" || op.request.command=="reconcile-acquisition-with-sources") || op.request.command=="reconcile-acquisition-with-alarm") && op.phase=="complete") {
        Require(status.state==2 && !status.outcomeUnknown && !status.needsReconciliation && !status.transition &&
            status.operationId==op.request.operationId,"Acquisition recovery lacks verified Described result");
        const auto original=QString::fromStdString(op.request.failedOperationId);
        Require(observations.contains(original),"Missing original acquisition intent/result");
        const auto &previous=observations[original];
        Require(previous.request.instanceId==op.request.instanceId && previous.request.snapshotSha256==op.request.snapshotSha256 &&
            previous.request.clientId==op.request.clientId && previous.request.clientEpoch==op.request.clientEpoch &&
            (previous.phase=="complete" || previous.phase=="failed" || previous.phase=="unknown"),"Acquisition recovery original ownership differs");
        QSet<QString> removable;
        for(const auto &held:heldOperations) {
            const auto &item=observations[held];
            if(item.resultJson.empty() || item.request.instanceId!=op.request.instanceId || item.request.snapshotSha256!=op.request.snapshotSha256 ||
               item.request.clientId!=op.request.clientId || item.request.clientEpoch!=op.request.clientEpoch)continue;
            const auto before=DecodeEccControlControllerStatus(item.resultJson);
            if(before.operationId!=op.request.failedOperationId || !before.outcomeUnknown)continue;
            const bool cleanup=item.request.command=="stop-production-after-failure" || item.request.command=="breakup-after-failure" ||
                item.request.command=="abort-failed-receivers" || item.request.command=="retire-failed-receivers" || item.request.command=="retire-failed-sources" || item.request.command=="unprepare-acquisition-after-failure";
            const bool recovery=(item.request.command=="reconcile-acquisition" ||
                (op.request.command=="reconcile-acquisition-with-sources" && item.request.command==op.request.command) ||
                (op.request.command=="reconcile-acquisition-with-alarm" && item.request.command==op.request.command && item.request.alarmOperationId==op.request.alarmOperationId)) &&
                item.request.failedOperationId==op.request.failedOperationId;
            if(held==original || (cleanup && item.phase=="complete") || (recovery && item.phase=="failed"))
                removable.insert(held);
        }
        for(const auto &held:removable)heldOperations.remove(held);
    }
    if((op.request.command=="reconcile-description" || (op.request.command=="reconcile-preparation" || op.request.command=="reconcile-preparation-with-alarm")) && op.phase=="complete") {
        const bool preparationRecovery=(op.request.command=="reconcile-preparation" || op.request.command=="reconcile-preparation-with-alarm");
        Require(status.state==2 && !status.outcomeUnknown && !status.needsReconciliation &&
                !status.transition && status.operationId==op.request.operationId,
                "Description recovery result does not match verified Described");
        const auto original=QString::fromStdString(op.request.failedOperationId);
        if(heldOperations.contains(original)) {
            Require(observations.contains(original),"Missing original description observation");
            const auto &failed=observations[original];
            const auto previous=DecodeEccControlControllerStatus(failed.resultJson);
            Require(failed.request.command==(preparationRecovery ? "prepare" : "describe") && failed.phase=="failed" &&
                    previous.state==(preparationRecovery ? 2 : 1) && previous.outcomeUnknown && previous.operationId==failed.request.operationId &&
                    failed.request.instanceId==op.request.instanceId && failed.request.snapshotSha256==op.request.snapshotSha256 &&
                    failed.request.clientId==op.request.clientId && failed.request.clientEpoch==op.request.clientEpoch,
                    "Recovery does not match the original failed Describe intent");
            heldOperations.remove(original);
            // Failed read-only attempts against this same original Describe
            // are part of the same hold; unrelated failures remain held.
            for(auto it=heldOperations.begin();it!=heldOperations.end();) {
                const auto &attempt=observations[*it];
                const bool sameRecovery=attempt.request.command==op.request.command ||
                    (op.request.command=="reconcile-preparation-with-alarm" && attempt.request.command=="reconcile-preparation");
                const bool sameAlarm=attempt.request.command!="reconcile-preparation-with-alarm" || attempt.request.alarmOperationId==op.request.alarmOperationId;
                if(sameRecovery && sameAlarm && attempt.phase=="failed" &&
                   attempt.request.failedOperationId==op.request.failedOperationId &&
                   attempt.request.instanceId==op.request.instanceId && attempt.request.snapshotSha256==op.request.snapshotSha256 &&
                   attempt.request.clientId==op.request.clientId && attempt.request.clientEpoch==op.request.clientEpoch)
                    it=heldOperations.erase(it);
                else if(preparationRecovery && attempt.phase=="complete" && !attempt.resultJson.empty() &&
                    (attempt.request.command=="stop-production-after-failure" || attempt.request.command=="unprepare-after-failure") &&
                    attempt.request.instanceId==op.request.instanceId && attempt.request.snapshotSha256==op.request.snapshotSha256 &&
                    attempt.request.clientId==op.request.clientId && attempt.request.clientEpoch==op.request.clientEpoch) {
                    const auto retained=DecodeEccControlControllerStatus(attempt.resultJson);
                    if(retained.state==2 && retained.outcomeUnknown && retained.operationId==op.request.failedOperationId)
                        it=heldOperations.erase(it);
                    else ++it;
                }
                else ++it;
            }
        }
    }
    if(op.phase=="complete" && heldOperations.contains(id) && observations.contains(id) &&
       observations[id].phase=="unknown" &&
       (op.request.command=="describe" || op.request.command=="prepare" || op.request.command=="configure")) {
        const unsigned int target=op.request.command=="describe" ? 2U :
            op.request.command=="prepare" ? 3U : 4U;
        Require(!op.resultJson.empty() && status.state==target && status.operationId==op.request.operationId &&
                !status.transition && !status.error && !status.outcomeUnknown && !status.needsReconciliation,
                "Lifecycle unknown can only clear from the exact known target result for the original operation ID");
        heldOperations.remove(id);
    }
    if(op.request.command=="resume-catalog-replacement" && op.phase=="complete") {
        const auto original=QString::fromStdString(op.request.failedOperationId);
        Require(observations.contains(original) && observations[original].phase=="failed" &&
                observations[original].request.command=="catalog" && observations[original].request.catalogReplaceExisting &&
                observations[original].request.instanceId==op.request.instanceId &&
                observations[original].request.snapshotSha256==op.request.snapshotSha256 &&
                observations[original].request.clientId==op.request.clientId &&
                observations[original].request.clientEpoch==op.request.clientEpoch,
                "Catalog recovery completion does not match the retained failed replacement");
        Require(status.state==3 && !status.transition && !status.error && !status.outcomeUnknown && !status.needsReconciliation,
                "Catalog recovery completion lacks a known Prepared controller");
        heldOperations.remove(original);
    }
    observations[id]=op;
    if(op.phase=="unknown" || (!op.resultJson.empty() && (status.outcomeUnknown || status.needsReconciliation)))
        heldOperations.insert(id);
    reconciliation=!heldOperations.isEmpty();


}
