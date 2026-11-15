#include "receivercatalog.h"
#include <stdexcept>
#include "receivercataloginternal.h"
#include "receivercatalogreplacement_p.h"
#include "controlprotocol.h"
#include "configurationsha256.h"
#include "planio.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

namespace GetDAQ {
using namespace SupervisorIO;
using namespace ReceiverCatalogInternal;
using namespace ReceiverCatalogReplacementInternal;

std::string PublishReceiverCatalogReplacement(const std::string &oldPath,const std::string &newPath,
                                               const std::string &candidatePath) {
    Path(oldPath); Path(newPath); Path(candidatePath);
    const auto overlaps=[](const std::string &a,const std::string &b) {
        return a==b || a.compare(0,b.size()+1,b+'/')==0 || b.compare(0,a.size()+1,a+'/')==0;
    };
    Require(!overlaps(oldPath,newPath) && !overlaps(oldPath,candidatePath) && !overlaps(newPath,candidatePath),
        "Replacement target, new input, and candidate must be separate trees");
    const auto newRequestBytes=ReadFile(newPath+"/request.json",1024*1024);
    const auto newRequest=DecodeReceiverCatalogRequest(newRequestBytes);
    const auto requestKey=ReceiverCatalogRequestKey(newRequest);
    const auto backupPath=CatalogReplacementBackupPath(oldPath,newRequest.operationId);
    const auto journalPath=ReplacementTransactionPath(oldPath,newRequest.operationId);
    Require(!overlaps(backupPath,newPath) && !overlaps(backupPath,candidatePath) && journalPath!=candidatePath && journalPath!=newPath,
        "Replacement backup/journal overlaps an input tree");
    const auto targetParentPath=ParentPath(oldPath),candidateParentPath=ParentPath(candidatePath);
    const auto targetLeaf=LeafPath(oldPath),backupLeaf=LeafPath(backupPath),candidateLeaf=LeafPath(candidatePath);
    Directory targetParent(targetParentPath),candidateParent(candidateParentPath),newCatalog(newPath);
    struct stat targetParentInfo,candidateParentInfo,newCatalogInfo;
    Require(!::fstat(targetParent.Descriptor(),&targetParentInfo) && !::fstat(candidateParent.Descriptor(),&candidateParentInfo) &&
        !::fstat(newCatalog.Descriptor(),&newCatalogInfo) && targetParentInfo.st_dev==candidateParentInfo.st_dev &&
        targetParentInfo.st_dev==newCatalogInfo.st_dev,
        "Replacement target, candidate, and new catalog must share a filesystem");
    const auto currentNewReceipt=InspectReceiverCatalog(newPath);
    Require(ReadFile(newPath+"/request.json",1024*1024)==newRequestBytes &&
        ConfigurationSha256(currentNewReceipt)==ConfigurationSha256(InspectReceiverCatalog(newPath)),
        "New catalog changed before replacement publication");

    if (!PathPresent(journalPath)) {
        try { CreateDirectory(journalPath); }
        catch (...) { if (!PathPresent(journalPath)) throw; }
    }
    Directory transaction(journalPath);
    SupervisorIO::Fd lock(::openat(transaction.Descriptor(),"lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_CLOEXEC,0600));
    struct stat lockInfo;
    Require(lock.value>=0 && !::fstat(lock.value,&lockInfo) && S_ISREG(lockInfo.st_mode) && lockInfo.st_uid==::geteuid() &&
        (lockInfo.st_mode&0777)==0600 && lockInfo.st_nlink==1 && !::flock(lock.value,LOCK_EX|LOCK_NB),
        "Replacement transaction is busy or its lock is unsafe; observe the same operation later");
    Require(!::fsync(lock.value),"Cannot sync replacement transaction lock"); SyncDirectory(transaction);

    const auto cleanupIntentPath=journalPath+"/cleanup-intent.json";
    if (PathPresent(cleanupIntentPath)) {
        const auto finalReceipt=InspectReceiverCatalog(oldPath);
        const auto cleanup=ParseStrictJson(ReadFile(cleanupIntentPath,16*1024*1024));
        ExactSupervisorFields(cleanup,{"schema","kind","operation_id","target","backup","old_receipt_sha256","published_receipt_sha256","files"});
        Require(Unsigned(cleanup.At("schema"))==1 && String(cleanup.At("kind"))=="getdaq.catalog-replacement-cleanup-intent" &&
            String(cleanup.At("operation_id"))==newRequest.operationId && String(cleanup.At("target"))==oldPath &&
            String(cleanup.At("backup"))==backupPath && ConfigurationSha256(finalReceipt)==String(cleanup.At("published_receipt_sha256")) &&
            ReceiverCatalogRequestKey(DecodeReceiverCatalogRequest(ReadFile(oldPath+"/request.json",1024*1024)))==requestKey,
            "Published catalog finalization intent differs; preserve all paths");
        return finalReceipt;
    }

    const auto intentPath=journalPath+"/intent.json";
    if (!PathPresent(intentPath)) {
        Require(PathPresent(oldPath) && PathPresent(candidatePath) && !PathPresent(backupPath),
            "Fresh replacement publication requires old target and candidate, with no backup");
        const auto oldReceipt=InspectReceiverCatalog(oldPath),candidateReceipt=InspectReceiverCatalog(candidatePath);
        const auto candidateRequestBytes=ReadFile(candidatePath+"/request.json",1024*1024);
        const auto candidateManifest=CatalogJson(ReadFile(candidatePath+"/manifest.json",16*1024*1024));
        Require(candidateRequestBytes==newRequestBytes && String(candidateManifest.At("publication_target"))==oldPath &&
            String(candidateManifest.At("publication_backup"))==backupPath &&
            String(ParseStrictJson(candidateReceipt).At("operation_id"))==newRequest.operationId,
            "Candidate operation/target/backup differs from the requested publication");
        WriteNew(intentPath,ReplacementIntent(oldPath,backupPath,candidatePath,newRequest.operationId,requestKey,
            oldReceipt,currentNewReceipt,candidateReceipt));
        SyncDirectory(transaction);
    }

    const auto intentBytes=ReadFile(intentPath,65536);
    const auto intent=ParseStrictJson(intentBytes);
    ExactSupervisorFields(intent,{"schema","kind","operation_id","request_key","target","backup","candidate",
        "old_receipt_sha256","new_receipt_sha256","candidate_receipt_sha256"});
    Require(Unsigned(intent.At("schema"))==1 && String(intent.At("kind"))=="getdaq.catalog-replacement-intent" &&
        String(intent.At("operation_id"))==newRequest.operationId && String(intent.At("request_key"))==requestKey &&
        String(intent.At("target"))==oldPath && String(intent.At("backup"))==backupPath &&
        String(intent.At("candidate"))==candidatePath && String(intent.At("new_receipt_sha256"))==ConfigurationSha256(currentNewReceipt),
        "Replacement journal does not match this operation/input; preserve all paths");
    const auto oldDigest=String(intent.At("old_receipt_sha256"));
    const auto candidateDigest=String(intent.At("candidate_receipt_sha256"));
    Digest(oldDigest); Digest(candidateDigest);
    const auto markerBody=[&](const std::string &phase,const std::string &digest) {
        return "{\"phase\":"+EncodeJsonString(phase)+",\"operation_id\":"+EncodeJsonString(newRequest.operationId)+
            ",\"receipt_sha256\":"+EncodeJsonString(digest)+"}\n";
    };
    const auto oldMarker=markerBody("old-preserved",oldDigest);
    const auto publishedMarker=markerBody("candidate-published",candidateDigest);
    const auto verifiedMarker=markerBody("candidate-verified",candidateDigest);

    for (unsigned int step=0;step<3;++step) {
        const bool targetPresent=PathPresent(oldPath),backupPresent=PathPresent(backupPath),candidatePresent=PathPresent(candidatePath);
        const bool targetOld=targetPresent && CatalogMatches(oldPath,oldPath,oldDigest);
        const bool targetCandidate=targetPresent && CatalogMatches(oldPath,oldPath,candidateDigest);
        const bool backupOld=backupPresent && CatalogMatches(backupPath,oldPath,oldDigest);
        const bool candidateReady=candidatePresent && CatalogMatches(candidatePath,oldPath,candidateDigest);
        Require((!targetPresent || targetOld || targetCandidate) && (!backupPresent || backupOld) &&
            (!candidatePresent || candidateReady),
            "Replacement path identity is unknown; preserve target, backup, candidate, and journal");
        const bool oldPhase=MarkerPresent(transaction,"old-preserved.json",oldMarker);
        const bool publishedPhase=MarkerPresent(transaction,"candidate-published.json",publishedMarker);
        if (targetOld && !backupPresent && candidateReady) {
            Require(!oldPhase && !publishedPhase,"Replacement journal phase contradicts filesystem state");
            Require(!RenameNoReplace(targetParent.Descriptor(),targetLeaf.c_str(),targetParent.Descriptor(),backupLeaf.c_str()),
                "Cannot preserve old catalog at its operation-bound backup path");
            SyncDirectory(targetParent);
            EnsureMarker(transaction,"old-preserved.json",oldMarker);
            continue;
        }
        if (!targetPresent && backupOld && candidateReady) {
            Require(!publishedPhase,"Replacement journal says published while target is absent");
            EnsureMarker(transaction,"old-preserved.json",oldMarker);
            Require(!RenameNoReplace(candidateParent.Descriptor(),candidateLeaf.c_str(),targetParent.Descriptor(),targetLeaf.c_str()),
                "Cannot publish prepared candidate without replacing an unexpected target");
            SyncDirectory(candidateParent); if (candidateParentPath!=targetParentPath) SyncDirectory(targetParent);
            EnsureMarker(transaction,"candidate-published.json",publishedMarker);
            continue;
        }
        if (targetCandidate && backupOld && !candidatePresent) {
            EnsureMarker(transaction,"old-preserved.json",oldMarker);
            EnsureMarker(transaction,"candidate-published.json",publishedMarker);
            const auto finalReceipt=InspectReceiverCatalog(oldPath);
            Require(ConfigurationSha256(finalReceipt)==candidateDigest &&
                ReadFile(newPath+"/request.json",1024*1024)==newRequestBytes &&
                ConfigurationSha256(InspectReceiverCatalog(newPath))==ConfigurationSha256(currentNewReceipt),
                "Published candidate or new catalog changed during verification; preserve transaction");
            EnsureMarker(transaction,"candidate-verified.json",verifiedMarker);
            targetParent.Check(); candidateParent.Check(); transaction.Check();
            return finalReceipt;
        }
        throw std::runtime_error("Replacement transaction paths do not match a recoverable phase; preserve all data for inspection");
    }
    throw std::runtime_error("Replacement transaction did not reach a stable published state; inspect its journal");
}


} // namespace GetDAQ
