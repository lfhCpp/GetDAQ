#include "datasupervisor_p.h"
#include "receivercatalog.h"
#include "systemexperimentconfiguration.h"

#include <cerrno>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <sys/stat.h>

namespace GetDAQ {
using namespace DataSupervisorInternal;
using namespace SupervisorIO;

std::string DataSupervisor::Catalog(const std::string &path,const std::string &id,const std::string &experiment,unsigned int run,const std::string &expectedSession,bool replaceCompletedRun,const std::string &replacementPhase,const std::string &replacementId) {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock); if (!lock.owns_lock()) throw std::runtime_error("Supervisor operation already running");
    const bool resume=replaceCompletedRun && (replacementPhase=="publish" || replacementPhase=="finalize") &&
        p.CanResumeCatalog(replacementId,experiment,run,expectedSession,replacementPhase);
    if (!resume) p.Eligible();
    p.CheckStore();
    if ((!resume && p.snapshot.state!=DataSupervisorStatus::Stopped) || expectedSession!=p.session)
        throw std::runtime_error("Catalog requires the current Stopped receiver session");
    if (replaceCompletedRun && replacementPhase!="single" && replacementPhase!="prepare" && replacementPhase!="publish" && replacementPhase!="finalize") throw std::invalid_argument("Unsupported replacement catalog phase");
    if (replaceCompletedRun && replacementPhase!="single" && replacementId.empty()) throw std::invalid_argument("Replacement phase requires its stable replacement ID");
    if (!replaceCompletedRun && (replacementPhase!="single" || !replacementId.empty())) throw std::invalid_argument("Catalog phase requires replacement authorization");
    const auto catalogOperationId=replaceCompletedRun ? (replacementId.empty() ? id : replacementId) : id;
    ReceiverCatalogOptions options; options.receiver=p.options; options.stopped=Status();
    if (resume) { options.stopped.state=DataSupervisorStatus::Stopped; options.stopped.needsReconciliation=false; options.stopped.message.clear(); }
    options.executableSha256=p.executableHash;
    options.operationId=catalogOperationId; options.experiment=experiment; options.runNumber=run; options.stagedForReplacement=replaceCompletedRun;
    std::unique_ptr<Audit> audit;
    ReceiverCatalog catalog(options,[&](const std::string &stage,const std::string &source,std::uint64_t files) {
        if (stage=="linked" && files%1000!=0) return;
        if (audit) audit->Row("{\"phase\":"+EncodeJsonString(stage)+",\"source_id\":"+EncodeJsonString(source)+",\"linked_files\":"+EncodeJsonString(std::to_string(files))+"}");
        p.CheckStore();
    });
    const auto stage=replaceCompletedRun ? ReceiverCatalogReplacementStagePath(options) : std::string();
    const auto candidate=replaceCompletedRun ? ReceiverCatalogReplacementCandidatePath(options) : std::string();
    const auto exists=[](const std::string &file) { struct stat info; if (!::lstat(file.c_str(),&info)) return true; if (errno==ENOENT) return false; throw std::runtime_error("Cannot inspect replacement catalog path"); };
    if (replacementPhase!="finalize" && (!replaceCompletedRun || !exists(stage))) catalog.Validate(); // 首次目录变更前检查所有源并保留源锁。
    audit.reset(new Audit(path));
    try {
        p.Record("catalog-intent","\"operation_id\":"+EncodeJsonString(id)+",\"catalog_operation_id\":"+EncodeJsonString(catalogOperationId)+",\"phase\":"+EncodeJsonString(replacementPhase)+",\"request_key\":"+EncodeJsonString(ReceiverCatalogRequestKey(options)));
        std::string result;
        if (replaceCompletedRun && replacementPhase=="finalize") {
            const auto target=ReceiverCatalogTargetPath(options);
            result=InspectReceiverCatalog(target);
            const auto current=ParseStrictJson(result);
            if (current.members.size()!=10 || Unsigned(current.At("schema"))!=1 || String(current.At("kind"))!="getdaq.receiver-catalog-complete" ||
                !Boolean(current.At("complete")) || String(current.At("operation_id"))!=catalogOperationId ||
                String(current.At("request_key"))!=ReceiverCatalogRequestKey(options) || String(current.At("directory"))!=target)
                throw std::runtime_error("Published catalog identity differs from the requested replacement; old raw cleanup is forbidden");
            if (resume) {
                p.catalogFailureRecoverable=false; p.catalogFailureResumeAttempted=true;
                std::lock_guard<std::mutex> state(p.statusMutex);
                p.snapshot.needsReconciliation=false; p.snapshot.state=DataSupervisorStatus::Stopped; p.snapshot.message.clear();
            }
            const auto cleanup=FinalizeReceiverCatalogReplacement(target,catalogOperationId);
            p.Record("catalog-finalize-complete","\"operation_id\":"+EncodeJsonString(id)+",\"catalog_operation_id\":"+
                EncodeJsonString(catalogOperationId)+",\"cleanup_receipt\":"+EncodeJsonString(cleanup));
            result=InspectReceiverCatalog(target);
        } else if (!replaceCompletedRun) result=catalog.Execute();
        else {
            if (exists(stage)) {
                result=InspectReceiverCatalog(stage);
                const auto request=DecodeReceiverCatalogRequest(ReadFile(stage+"/request.json",1024*1024));
                if (!request.stagedForReplacement || request.operationId!=catalogOperationId || ReceiverCatalogRequestKey(request)!=ReceiverCatalogRequestKey(options))
                    throw std::runtime_error("Existing replacement stage belongs to a different operation; preserve it");
            } else result=catalog.Execute();
            const auto target=ReceiverCatalogTargetPath(options);
            if (replacementPhase=="publish") {
                // Do not rebuild a candidate during publish. The durable local
                // transaction journal is the only authority for a retry.
                if (exists(candidate)) result=InspectReceiverCatalog(candidate);
            } else {
                if (!exists(candidate)) PrepareReceiverCatalogReplacement(target,stage,candidate);
                result=InspectReceiverCatalog(candidate);
            }
            if (resume) {
                p.catalogFailureRecoverable=false; p.catalogFailureResumeAttempted=true;
                std::lock_guard<std::mutex> state(p.statusMutex);
                p.snapshot.needsReconciliation=false; p.snapshot.state=DataSupervisorStatus::Stopped; p.snapshot.message.clear();
            }
            if (replacementPhase!="prepare") result=PublishReceiverCatalogReplacement(target,stage,candidate);
        }
        p.Record("catalog-complete","\"operation_id\":"+EncodeJsonString(id)+",\"catalog_operation_id\":"+EncodeJsonString(catalogOperationId)+",\"phase\":"+EncodeJsonString(replacementPhase)+",\"result\":"+EncodeJsonString(result)); audit->Check();
        if (resume) { p.catalogFailureReplacementId.clear(); p.catalogFailureExperiment.clear(); p.catalogFailureSession.clear(); p.catalogFailureRun=0; p.catalogFailurePhase.clear(); }
        return result;
    } catch (const std::exception &e) {
        bool failureRecorded=true;
        try { p.Record("catalog-failed","\"message\":"+EncodeJsonString(e.what())+",\"automatic_retry\":false"); }
        catch (...) { failureRecorded=false; }
        p.catalogFailureRecoverable=false; p.catalogFailureResumeAttempted=resume;
        DataSupervisorStatus stateBeforeFailure;
        { std::lock_guard<std::mutex> state(p.statusMutex); stateBeforeFailure=p.snapshot; }
        if (!resume && failureRecorded && replaceCompletedRun && (replacementPhase=="publish" || replacementPhase=="finalize") && !p.storageFault &&
            expectedSession==p.session && !stateBeforeFailure.needsReconciliation && stateBeforeFailure.state==DataSupervisorStatus::Stopped) {
            bool sealed=!p.children.empty();
            for (const auto &owned:p.children) {
                const auto &child=*owned;
                if (!child.status.reaped || child.status.pid<=0 || child.status.exitCode!=0 || !child.status.sealVerified || child.status.sessionId!=expectedSession) sealed=false;
                try { child.directory->Check(); } catch (...) { sealed=false; }
            }
            if (sealed) {
                p.catalogFailureRecoverable=true; p.catalogFailureReplacementId=replacementId; p.catalogFailurePhase=replacementPhase;
                p.catalogFailureExperiment=experiment; p.catalogFailureRun=run; p.catalogFailureSession=expectedSession;
            }
        }
        { std::lock_guard<std::mutex> state(p.statusMutex); p.snapshot.needsReconciliation=true; }
        p.Publish(DataSupervisorStatus::Failed,e.what()); throw;
    }
}
bool DataSupervisor::CanResumeCatalogReplacement(const std::string &replacementId,const std::string &experiment,unsigned int run,const std::string &expectedSession,const std::string &phase) const {
    auto &p=*fImpl; std::unique_lock<std::mutex> lock(p.operation,std::try_to_lock);
    if (!lock.owns_lock()) return false;
    return p.CanResumeCatalog(replacementId,experiment,run,expectedSession,phase);
}

}
