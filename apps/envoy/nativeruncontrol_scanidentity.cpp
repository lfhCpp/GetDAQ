#include "nativeruncontrol.h"
#include "scanruntimeidentitycoordinator.h"
#include "scanexecutionsnapshot.h"
#include "scanexecutionpolicy.h"
#include "scanexecutionjournal.h"
#include "scanoperationrunner.h"
#include "systemscanplan.h"
#include "configurationsha256.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <stdexcept>

namespace {
std::string ReadScanFile(const QString &path,qint64 maximumBytes,const char *description) {
    const QFileInfo info(path);
    if(info.isSymLink() || !info.isFile() || info.size()<=0 || info.size()>maximumBytes)
        throw std::runtime_error(std::string(description)+" is missing, linked, non-regular, empty, or oversized");
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly))throw std::runtime_error(std::string("Cannot read ")+description);
    const QByteArray bytes=file.readAll();
    if(file.error()!=QFile::NoError || bytes.size()!=info.size())
        throw std::runtime_error(std::string(description)+" could not be read completely");
    return bytes.toStdString();
}
}

void NativeRunControl::QueryScanRuntimeIdentity(const QString &journalPath,
                                               const QString &planDirectory,
                                               const QByteArray &snapshotBytes,
                                               unsigned int pointIndex) {
    fScan.identityResponseBody.clear();
    if(!fConnected || fShuttingDown || !fClient || fBusy || fActive || fDeferredProgressState) {
        emit ScanRuntimeIdentityBoundSignal({}, {}, "Scan identity observation requires an idle connected native controller");return;
    }
    if(!fConfig.nativeClient.receiverStatusEnabled || !fIdentityMatched || fIdentityBody.isEmpty() ||
       !fIdentityClock.isValid() || fIdentityClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
       !fHaveStatus || !fStatusClock.isValid() || fStatusClock.elapsed()>fConfig.nativeClient.statusMaxAgeMs ||
       !fObservations.ReceiversFresh(fConfig.nativeClient.statusMaxAgeMs) ||
       fObservations.ReceiverBody().isEmpty() || fClient->Busy() || fClient->Pending()) {
        emit ScanRuntimeIdentityBoundSignal({}, {}, "Fresh matched ECC identity, status, and receiver observations are required");return;
    }
    try {
        const QFileInfo planInfo(planDirectory);
        if(planInfo.isSymLink() || !planInfo.isDir() || !QDir::isAbsolutePath(planDirectory) ||
           QDir::cleanPath(planInfo.absoluteFilePath())!=planInfo.absoluteFilePath())
            throw std::invalid_argument("Scan plan directory must be an absolute non-linked directory");
        if(snapshotBytes.isEmpty() || snapshotBytes.size()>1024*1024)
            throw std::invalid_argument("Frozen scan execution snapshot is missing or oversized");
        const QString manifestPath=QDir(planDirectory).filePath(QStringLiteral("scan-manifest.json"));
        const QString policyPath=QDir(planDirectory).filePath(QStringLiteral("scan-execution-policy.json"));
        const std::string planBytes=ReadScanFile(manifestPath,1024*1024,"scan manifest");
        const std::string policyBytes=ReadScanFile(policyPath,4096,"scan execution policy");
        const GetDAQ::SystemScanPlanSummary planSummary=GetDAQ::ValidateSystemScanPlan(
            planBytes,[&](const std::string &name) {
                return ReadScanFile(QDir(planDirectory).filePath(QString::fromUtf8(name.c_str())),
                                    64LL*1024*1024,"scan point artifact");
            });
        const GetDAQ::ScanExecutionPolicy policy=GetDAQ::ScanExecutionPolicy::Decode(
            policyBytes,planBytes,planSummary);
        const GetDAQ::ScanExecutionSnapshot snapshot=GetDAQ::ScanExecutionSnapshot::Decode(
            snapshotBytes.toStdString(),planBytes,policyBytes,[&](const std::string &name) {
                return ReadScanFile(QDir(planDirectory).filePath(QString::fromUtf8(name.c_str())),
                                    64LL*1024*1024,"scan point artifact");
            });
        std::vector<std::string> agentIds;
        bool targetCoBoConfigured=false;
        for(const auto &node:fConfig.nodes)if(!node.nativeSenderId.isEmpty()) {
            agentIds.push_back(node.nativeAgentId.toStdString());
            if(!node.mutant && node.nativeSenderId.toStdString()==snapshot.TargetCoBo())targetCoBoConfigured=true;
        }
        if(!targetCoBoConfigured)
            throw std::invalid_argument("Scan target CoBo is not part of the currently configured native deployment");
        fScan.identityStore=GetDAQ::ScanExecutionJournalStore::Open(journalPath.toUtf8().toStdString());
        const GetDAQ::ScanExecutionJournal scanJournal=fScan.identityStore->Current();
        const bool revalidateExistingBinding=!scanJournal.RuntimeIdentitySha256().empty();
        const GetDAQ::ScanRuntimeIdentityDeployment deployment=
            GetDAQ::CaptureScanRuntimeIdentityDeployment(
                fConfig.nativeClient.serviceId.toStdString(),fConfig.nativeClient.clientId.toStdString(),
                fConfig.nativeClient.snapshotSha256.toStdString(),fConfig.nativeClient.inputBindingSha256.toStdString(),
                agentIds,fIdentityBody.toStdString(),fStatusBody.toStdString(),fObservations.ReceiverBody().toStdString(),
                revalidateExistingBinding);
        const GetDAQ::ScanRuntimeIdentityExpectation expected=
            GetDAQ::BindScanRuntimeIdentityExpectation(snapshot,pointIndex,deployment);
        fScan.runtimeSnapshot.reset(new GetDAQ::ScanExecutionSnapshot(snapshot));
        fScan.runtimeExpectation.reset(new GetDAQ::ScanRuntimeIdentityExpectation(expected));
        fScan.eventsPerPoint=policy.EventsPerPoint();
        fScan.maximumDurationMs=policy.MaximumDurationMsPerPoint();
        fScan.thresholdManualOnly=policy.Kind()=="getdaq.aget-threshold-scan";
        fScan.eventMonitor.reset();
        fScan.runtimeJournalPath=QFileInfo(journalPath).absoluteFilePath();
        fScan.identityPointIndex=pointIndex;
        const bool adoptExistingInstance =
            scanJournal.State()==GetDAQ::ScanPointState::Reviewed &&
            !revalidateExistingBinding;
        fScan.identityCoordinator.reset(new GetDAQ::ScanRuntimeIdentityCoordinator(
            *fScan.identityStore,snapshot,expected,revalidateExistingBinding,
            adoptExistingInstance));
        const GetDAQ::ScanRuntimePointIdentityRequest request=revalidateExistingBinding
            ? fScan.identityCoordinator->PrepareRevalidationQuery()
            : adoptExistingInstance
                ? fScan.identityCoordinator->PrepareAdoptionQuery(
                    ("gui-"+QUuid::createUuid().toString(QUuid::WithoutBraces)+"-adopt-ecc").toStdString())
                : fScan.identityCoordinator->PrepareQuery();
        fBusy=true;fPhase="Checking scan-point ECC identity";Publish();
        if(!fClient->QueryScanRuntimeIdentity(request,expected) && fScan.identityCoordinator)
            FinishScanRuntimeIdentityQuery({},"ECC client refused the read-only scan identity request");
    } catch(const std::exception &e) {
        fScan.identityCoordinator.reset();fScan.identityStore.reset();
        fScan.runtimeSnapshot.reset();fScan.runtimeExpectation.reset();fScan.runtimeJournalPath.clear();
        emit ScanRuntimeIdentityBoundSignal({}, {}, QString::fromUtf8(e.what()).left(512));
    }
}
void NativeRunControl::FinishScanRuntimeIdentityQuery(const QByteArray &responseBytes,const QString &error) {
    if(!fScan.identityCoordinator)return;
    QString failure=error;QByteArray journalBytes;
    if(failure.isEmpty())try {
        const GetDAQ::ScanExecutionJournal updated=fScan.identityCoordinator->BindResponse(responseBytes.toStdString());
        fScan.identityResponseBody=responseBytes;
        journalBytes=QByteArray::fromStdString(updated.Encode());
    } catch(const std::exception &e) {failure=QString::fromUtf8(e.what()).left(512);}
    fScan.identityCoordinator.reset();fScan.identityStore.reset();fBusy=false;fPhase="Native monitoring";
    if(!failure.isEmpty())emit LogSignal("Scan runtime identity was not bound: "+failure);
    emit ScanRuntimeIdentityBoundSignal(journalBytes,failure.isEmpty()?responseBytes:QByteArray(),failure);
    if(!failure.isEmpty()) {fScan.identityResponseBody.clear();fScan.runtimeSnapshot.reset();fScan.runtimeExpectation.reset();fScan.runtimeJournalPath.clear();}
    Publish();
}
