#include "receivercatalog.h"
#include "receivercataloginternal.h"
#include "receivercatalogreplacement_p.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include "supervisorio.h"
#include <cerrno>
#include <fcntl.h>
#include <memory>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
using namespace ReceiverCatalogReplacementInternal;

std::string InspectReceiverCatalogReplacement(const std::string &oldPath,const std::string &newPath,
                                               const std::string &candidatePath,const std::string &operationId) {
    Path(oldPath); Path(newPath); Path(candidatePath); SupervisorOperationId(operationId);
    const auto overlaps=[](const std::string &a,const std::string &b) {
        return a==b || a.compare(0,b.size()+1,b+'/')==0 || b.compare(0,a.size()+1,a+'/')==0;
    };
    Require(!overlaps(oldPath,newPath) && !overlaps(oldPath,candidatePath) && !overlaps(newPath,candidatePath),
        "Replacement inspection paths overlap");
    const auto requestBytes=ReadFile(newPath+"/request.json",1024*1024);
    const auto request=DecodeReceiverCatalogRequest(requestBytes);
    Require(request.operationId==operationId,
        "Replacement inspection operation does not match the staged catalog");
    const auto requestKey=ReceiverCatalogRequestKey(request);
    const auto backup=CatalogReplacementBackupPath(oldPath,operationId);
    const auto journalPath=ReplacementTransactionPath(oldPath,operationId);
    Require(!overlaps(backup,newPath) && !overlaps(backup,candidatePath),"Replacement backup overlaps an input");

    std::unique_ptr<Directory> transaction;
    SupervisorIO::Fd lock;
    bool hasJournal=PathPresent(journalPath);
    if (hasJournal) {
        transaction.reset(new Directory(journalPath));
        lock.value=::open((journalPath+"/lock").c_str(),O_RDONLY|O_NOFOLLOW|O_CLOEXEC);
        struct stat info;
        Require(lock.value>=0 && !::fstat(lock.value,&info) && S_ISREG(info.st_mode) && info.st_uid==::geteuid() &&
            (info.st_mode&0777)==0600 && info.st_nlink==1 && !::flock(lock.value,LOCK_SH|LOCK_NB),
            "Replacement transaction is busy or its inspection lock is unsafe; observe later");
        transaction->Check();
    }

    const auto newReceipt=InspectReceiverCatalog(newPath);
    Require(ReadFile(newPath+"/request.json",1024*1024)==requestBytes &&
        ReceiverCatalogRequestKey(DecodeReceiverCatalogRequest(ReadFile(newPath+"/request.json",1024*1024)))==requestKey,
        "Staged replacement catalog changed during inspection");
    const auto candidatePresent=PathPresent(candidatePath),targetPresent=PathPresent(oldPath),backupPresent=PathPresent(backup);
    std::string candidateReceipt;
    if (candidatePresent) {
        candidateReceipt=InspectReceiverCatalog(candidatePath);
        const auto candidateRequest=DecodeReceiverCatalogRequest(ReadFile(candidatePath+"/request.json",1024*1024));
        Require(candidateRequest.operationId==operationId && ReceiverCatalogRequestKey(candidateRequest)==requestKey,
            "Replacement candidate belongs to a different request");
    }

    std::string phase="unknown",oldSha,candidateSha= candidateReceipt.empty() ? std::string() : ConfigurationSha256(candidateReceipt);
    bool oldMarkerPresent=false,publishedMarkerPresent=false;
    bool oldComplete=false,backupComplete=false,targetPublished=false;
    if (!hasJournal) {
        if (targetPresent && candidatePresent) {
            const auto oldReceipt=InspectReceiverCatalog(oldPath);
            oldSha=ConfigurationSha256(oldReceipt); oldComplete=true;
            phase="prepared";
        }
    } else {
        const auto intentPath=journalPath+"/intent.json";
        if (PathPresent(intentPath)) {
            const auto intent=ParseStrictJson(ReadFile(intentPath,65536));
            ExactSupervisorFields(intent,{"schema","kind","operation_id","request_key","target","backup","candidate",
                "old_receipt_sha256","new_receipt_sha256","candidate_receipt_sha256"});
            oldSha=String(intent.At("old_receipt_sha256"));Digest(oldSha);
            const auto expectedCandidate=String(intent.At("candidate_receipt_sha256"));Digest(expectedCandidate);
            Require(Unsigned(intent.At("schema"))==1 && String(intent.At("kind"))=="getdaq.catalog-replacement-intent" &&
                String(intent.At("operation_id"))==operationId && String(intent.At("request_key"))==requestKey &&
                String(intent.At("target"))==oldPath && String(intent.At("backup"))==backup &&
                String(intent.At("candidate"))==candidatePath &&
                String(intent.At("new_receipt_sha256"))==ConfigurationSha256(newReceipt) &&
                (candidateSha.empty() || candidateSha==expectedCandidate),
                "Replacement transaction intent differs from the inspected inputs");
            candidateSha=expectedCandidate;
            oldComplete=CatalogMatches(oldPath,oldPath,oldSha);
            backupComplete=CatalogMatches(backup,oldPath,oldSha);
            targetPublished=CatalogMatches(oldPath,oldPath,candidateSha);
            const bool candidateReady=CatalogMatches(candidatePath,oldPath,candidateSha);
            const bool cleanupIntentPresent=PathPresent(journalPath+"/cleanup-intent.json");
            Require((!targetPresent || oldComplete || targetPublished) && (!backupPresent || backupComplete || cleanupIntentPresent) &&
                (!candidatePresent || candidateReady),"Replacement transaction contains an unrecognized catalog identity");
            const auto marker=[&](const std::string &name,const std::string &value) {
                return MarkerPresent(*transaction,name,"{\"phase\":"+EncodeJsonString(value)+",\"operation_id\":"+
                    EncodeJsonString(operationId)+",\"receipt_sha256\":"+EncodeJsonString(value=="old-preserved" ? oldSha : candidateSha)+"}\n");
            };
            oldMarkerPresent=marker("old-preserved.json","old-preserved");
            publishedMarkerPresent=marker("candidate-published.json","candidate-published");
            Require(!publishedMarkerPresent || oldMarkerPresent,"Published marker exists without the old-preserved marker");
            if (targetPublished && backupComplete && !candidatePresent) phase="published";
            else if (!targetPresent && backupComplete && candidatePresent && !publishedMarkerPresent) phase="old_preserved";
            else if (oldComplete && !backupPresent && candidateReady && !oldMarkerPresent && !publishedMarkerPresent) phase="prepared";
            else phase="unknown";

            const auto cleanupPath=journalPath+"/cleanup-intent.json";
            if (PathPresent(cleanupPath)) {
                const auto cleanup=ParseStrictJson(ReadFile(cleanupPath,16*1024*1024));
                ExactSupervisorFields(cleanup,{"schema","kind","operation_id","target","backup","old_receipt_sha256","published_receipt_sha256","files"});
                Require(Unsigned(cleanup.At("schema"))==1 && String(cleanup.At("kind"))=="getdaq.catalog-replacement-cleanup-intent" &&
                    String(cleanup.At("operation_id"))==operationId && String(cleanup.At("target"))==oldPath &&
                    String(cleanup.At("backup"))==backup && String(cleanup.At("old_receipt_sha256"))==oldSha &&
                    String(cleanup.At("published_receipt_sha256"))==candidateSha && cleanup.At("files").type==JsonValue::Array &&
                    targetPublished,"Replacement cleanup intent differs from the published catalog");
                phase="finalize_incomplete";
                const auto completePath=journalPath+"/cleanup-complete.json";
                if (PathPresent(completePath)) {
                    const auto complete=ParseStrictJson(ReadFile(completePath,65536));
                    ExactSupervisorFields(complete,{"schema","kind","operation_id","old_receipt_sha256","published_receipt_sha256","superseded_files","removed_link_bytes","raw_payload_bytes_read"});
                    Require(Unsigned(complete.At("schema"))==1 && String(complete.At("kind"))=="getdaq.catalog-replacement-cleanup-complete" &&
                        String(complete.At("operation_id"))==operationId && String(complete.At("old_receipt_sha256"))==oldSha &&
                        String(complete.At("published_receipt_sha256"))==candidateSha &&
                        Unsigned(complete.At("superseded_files"))==cleanup.At("files").items.size() && String(complete.At("raw_payload_bytes_read"))=="0",
                        "Replacement cleanup completion marker differs from its intent");
                    phase="finalized";
                }
            }
        } else if (targetPresent && candidatePresent && !backupPresent) {
            oldSha=ConfigurationSha256(InspectReceiverCatalog(oldPath)); oldComplete=true; phase="prepared";
        }
        transaction->Check();
    }

    return "{\"schema\":1,\"kind\":\"getdaq.catalog-replacement-observation\",\"operation_id\":"+EncodeJsonString(operationId)+
        ",\"request_key\":"+EncodeJsonString(requestKey)+",\"phase\":"+EncodeJsonString(phase)+
        ",\"target\":"+EncodeJsonString(oldPath)+",\"backup\":"+EncodeJsonString(backup)+
        ",\"candidate\":"+EncodeJsonString(candidatePath)+",\"old_receipt_sha256\":"+EncodeJsonString(oldSha)+
        ",\"published_receipt_sha256\":"+EncodeJsonString(candidateSha)+",\"transaction_journal_present\":"+(hasJournal ? "true" : "false")+
        ",\"old_preserved_marker_present\":"+(oldMarkerPresent ? "true" : "false")+
        ",\"candidate_published_marker_present\":"+(publishedMarkerPresent ? "true" : "false")+
        ",\"raw_payload_bytes_read\":\"0\",\"recovery_authorized\":false}\n";
}


} // namespace GetDAQ
