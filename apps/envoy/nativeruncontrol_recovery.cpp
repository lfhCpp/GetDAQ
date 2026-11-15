#include "nativeruncontrol.h"
#include "nativealarmrecoverypolicy.h"
#include "nativecatalogrecoverypolicy.h"
#include "nativerecoveryevidencepolicy.h"
#include "nativerecoverysnapshotpolicy.h"
#include "protocol.h"
#include "runrecorder.h"
#include <QDir>
#include <QUuid>
#include <stdexcept>

void NativeRunControl::Recover() {
    if (fActive || !fConnected || fBusy || !fHaveStatus || !fClient || !fStore) return;
    QString error; RunConfig saved; QJsonObject journal;
    if (!saved.Load(QDir(fConfig.RunDirectory()).filePath("run.yml"),&error) || !RunRecorder::ReadJournal(fConfig,&journal,&error)) { emit LogSignal(error); return; }
    if (!GetDAQ::MatchesNativeRecoverySnapshot(saved,fConfig) || journal.value("phase").toString()=="Complete") { emit LogSignal("Native recovery requires the exact incomplete Run snapshot"); return; }
    const QString failedCatalog=journal.value("native_catalog_failed_operation_id").toString();
    if(!Available() && failedCatalog.isEmpty()) {emit LogSignal("Native recovery requires fresh status and no unrelated pending/reconciliation hold");return;}
    if(!failedCatalog.isEmpty() && !fStore->CanResumeCatalogReplacement(failedCatalog.toStdString()))
        emit LogSignal("The saved replacement catalog failure is not eligible for live recovery; inspect the original operation and data-host state");
    try {
        const auto identity=GetDAQ::DecodeEccControlIdentity(journal.value("native_identity").toString().toUtf8().toStdString());
        const auto ready=GetDAQ::DecodeEccControlSystemStatus(journal.value("receiver_ready").toString().toUtf8().toStdString());
        const auto recorded=journal.value("native_configuration").toString().toUtf8();
        if (fConfig.nativeClient.configurationStatusEnabled || !recorded.isEmpty()) {
            if (fConfigurationBody.isEmpty()) {fClient->QueryConfiguration();emit LogSignal("Observe the ECC configuration record, then retry local Run recovery");return;}
            const auto configuration=GetDAQ::DecodeEccControlConfiguration(recorded.toStdString());
            const auto current=GetDAQ::DecodeEccControlConfiguration(fConfigurationBody.toStdString());
            if (GetDAQ::EncodeEccControlConfiguration(configuration)!=GetDAQ::EncodeEccControlConfiguration(current))
                throw std::runtime_error("Native recovery startup configuration record differs");
        }
        const QString evidenceError=GetDAQ::NativeRecoveryEvidenceError(identity,ready,fSystem);
        if(!evidenceError.isEmpty())throw std::runtime_error(evidenceError.toStdString());
        fConfig.description=saved.description; fConfig.fields=saved.fields; fJournal=journal; fJournal["duration_incomplete"]=true;
        fDuration=journal.value("duration_seconds").toDouble(); fRunClock.invalidate(); fActive=true;
        fPhase=failedCatalog.isEmpty() ? "Native recovered Run: timing incomplete, inspect before Stop/Finalize" :
            "Native replacement Run recovered: only its retained catalog operation may be resumed";
        if (!Journal(fPhase)) { Fail("Native Run recovered, but local recovery record could not be saved"); return; } Publish();
    } catch (const std::exception &e) { emit LogSignal(QString::fromUtf8(e.what())); }
}
void NativeRunControl::ResumeCatalogReplacement(const QString &failedOperationId,const QString &operationId) {
    const std::string savedFailedOperationId=fJournal.value("native_catalog_failed_operation_id").toString().toStdString();
    const bool recoveryContextReady=fConnected && !fBusy && fHaveStatus && fClient && fStore && fActive;
    const bool statusFresh=fStatusClock.isValid() && fStatusClock.elapsed()<=fConfig.nativeClient.statusMaxAgeMs;
    const bool replacementEligible=fStore && fStore->CanResumeCatalogReplacement(failedOperationId.toStdString());
    const bool commandAvailable=fClient && fClient->CanSubmit("resume-catalog-replacement");
    if(!GetDAQ::CanResumeNativeCatalogReplacement(fSystem,savedFailedOperationId,
            failedOperationId.toStdString(),operationId.toStdString(),
            fJournal.value("replace_completed_run").toBool(),recoveryContextReady,
            statusFresh,replacementEligible,commandAvailable)) {
        emit LogSignal("Catalog recovery requires the exact saved failed replacement, fresh known Prepared status, and no unrelated hold");Publish();return;
    }
    fCommand="resume-catalog-replacement";fTarget=3;fOperationId=operationId;
    fBusy=true;fPhase="Resuming the exact failed catalog replacement";fDeadline.start(fConfig.stepTimeoutMs);Publish();
    if(!fClient->Submit(fCommand,operationId,QString(),0,failedOperationId))
        Fail("Catalog recovery was not submitted; preserve the original failed operation and inspect local/ECC journals");
}
bool NativeRunControl::HandleCatalogReplacementResult(const QString &id,const QString &phase,
    const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result) {
    if(fCommand=="catalog" && phase=="failed" && operation.request.catalogReplaceExisting) {
        fJournal["native_catalog_failed_operation_id"]=id;
        fJournal["native_catalog_failure_message"]=QString::fromStdString(operation.message).left(512);
        if(!Journal("Replacement catalog failed; exact original operation retained")) {
            Fail("Catalog replacement failed and its recovery reference could not be saved; preserve the original ECC operation");return true;
        }
    }
    if(fCommand=="resume-catalog-replacement") {
        if(phase!="complete" || result.state!=3 || result.transition || result.error || result.outcomeUnknown || result.needsReconciliation ||
           operation.request.failedOperationId!=fJournal.value("native_catalog_failed_operation_id").toString().toStdString()) {
            Fail("Catalog replacement recovery did not return a known Prepared result; preserve both operations and inspect");return true;
        }
        fJournal["native_catalog_recovery_operation_id"]=id;
        fJournal["native_catalog_recovery_complete"]=true;
        if(!Journal("Catalog Finalize resumed; verifying fresh data-host receipts")) {
            Fail("Remote catalog recovery completed, but the local Run journal could not record it; inspect before retrying");return true;
        }
        fOperationId.clear();fBusy=false;fCommand=fActive ? "seal-after-status" : QString();
        fPhase="Catalog replacement recovered; verifying every data-host receipt";
        QueryStatusAfterOperation();Publish();return true;
    }
    return false;
}

bool NativeRunControl::HandleRecoveryOperationResult(const QString &phase,const QByteArray &body,
    const GetDAQ::EccControlOperation &operation,const GetDAQ::EccControlControllerStatus &result) {
    if(fCommand=="breakup-after-failure" && phase=="complete") {
        if(!result.outcomeUnknown){Fail("Failure Breakup unexpectedly cleared UNKNOWN");return true;}
        if(!fJournal.isEmpty()) {
            fJournal["native_failure_breakup_operation"]=QString::fromUtf8(body);fJournal["duration_incomplete"]=true;
            if(!Journal("Native failure connections closed; Run remains interrupted")){Fail("Cannot save failure connection cleanup record");return true;}
        }
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Failed acquisition connections closed; UNKNOWN retained";
        QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="retire-failed-receivers" && phase=="complete") {
        if(!result.outcomeUnknown) {Fail("Data retirement unexpectedly cleared parent UNKNOWN");return true;}
        if(!fJournal.isEmpty()) {
            fJournal["native_receiver_retirement_operation"]=QString::fromUtf8(body);fJournal["duration_incomplete"]=true;
            if(!Journal("Failed data sessions retired; hardware/Run recovery remains pending")) {Fail("Cannot save incomplete Run retirement record");return true;}
        }
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Original data sessions retired; UNKNOWN retained";
        QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="unprepare-acquisition-after-failure" && phase=="complete") {
        if(!result.outcomeUnknown){Fail("Acquisition unprepare unexpectedly cleared parent UNKNOWN");return true;}
        if(!fJournal.isEmpty()) {
            fJournal["native_acquisition_unprepare_operation"]=QString::fromUtf8(body);fJournal["duration_incomplete"]=true;
            if(!Journal("Original failed acquisition powered off; Run remains interrupted")){Fail("Cannot save acquisition power-off record");return true;}
        }
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Original acquisition powered off; UNKNOWN retained";
        QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="unprepare-after-failure" && phase=="complete") {
        if(!result.outcomeUnknown){Fail("Failure unprepare reply unexpectedly cleared UNKNOWN");return true;}
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Failed preparation powered off; UNKNOWN retained";QueryStatusAfterOperation();Publish();return true;
    }
    if(((fCommand=="reconcile-description" || fCommand=="release-stopped-alarm") || (fCommand=="reconcile-preparation" || fCommand=="reconcile-preparation-with-alarm") || ((fCommand=="reconcile-acquisition" || fCommand=="reconcile-acquisition-with-sources") || fCommand=="reconcile-acquisition-with-alarm")) && phase=="complete") {
        if(result.state!=2 || result.outcomeUnknown || result.needsReconciliation || result.operationId!=operation.request.operationId) {
            Fail("Description reconciliation did not establish the bound Described state");return true;
        }
        if(((fCommand=="reconcile-acquisition" || fCommand=="reconcile-acquisition-with-sources") || fCommand=="reconcile-acquisition-with-alarm" || fCommand=="release-stopped-alarm")) {
            if(fCommand=="reconcile-acquisition-with-sources")fObservations.ResetSources();
            if(!fJournal.isEmpty()) {
                fJournal["native_acquisition_recovery_operation"]=QString::fromUtf8(body);
                if(fCommand=="reconcile-acquisition-with-sources")fJournal["native_source_recovery_operation"]=QString::fromUtf8(body);
                if(fCommand=="reconcile-acquisition-with-alarm" || fCommand=="release-stopped-alarm")fJournal["native_alarm_acknowledgement_operation"]=QString::fromUtf8(body);
                fJournal["hardware_recovery_complete"]=true;fJournal["duration_incomplete"]=true;
                if(!Journal("Interrupted")){Fail("Hardware recovered remotely, but original interrupted Run record could not be saved");return true;}
            }
            fActive=false;fRunClock.invalidate();fObservations.ResetReceiverFailure();fObservations.ResetAlarms();
        }
        if(fCommand=="reconcile-preparation-with-alarm")fObservations.ResetAlarms();
        fRecovery.awaitingMetadata=true;
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Described: original failure verified; fresh Prepare required";
        QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="retire-failed-sources" && phase=="complete") {
        if(!result.outcomeUnknown){Fail("Source retirement unexpectedly cleared UNKNOWN");return true;}
        if(!fJournal.isEmpty()){fJournal["native_source_retirement_operation"]=QString::fromUtf8(body);fJournal["duration_incomplete"]=true;
            if(!Journal("Original failed source connections retired; Run incomplete")){Fail("Source retirement complete remotely but local original Run record could not be saved");return true;}}
        fOperationId.clear();fBusy=false;fCommand.clear();fPhase="Original source connections retired; UNKNOWN retained";QueryStatusAfterOperation();Publish();return true;
    }
    if(fCommand=="abort-failed-receivers" && phase=="complete") {
        if(!result.outcomeUnknown && !result.needsReconciliation) {Fail("Receiver cleanup reply unexpectedly cleared hardware reconciliation");return true;}
        if(!fJournal.isEmpty()) {
            fJournal["native_receiver_cleanup_operation"]=QString::fromUtf8(GetDAQ::EncodeEccControlOperation(operation).c_str());
            fJournal["duration_incomplete"]=true;
            if(!Journal("Failed receiver cleanup recorded; Run incomplete")) {Fail("Cleanup complete remotely, but local incomplete Run record could not be saved");return true;}
        }
        fOperationId.clear();fBusy=false;fPhase="Native failed receivers cleaned; UNKNOWN retained";QueryStatusAfterOperation();Publish();return true;
    }
    if (fCommand=="stop-production-after-failure" && phase=="complete") {
        fOperationId.clear(); fBusy=false; fPhase="Native production stop recorded; UNKNOWN retained"; QueryStatusAfterOperation(); Publish(); return true;
    }
    return false;
}
