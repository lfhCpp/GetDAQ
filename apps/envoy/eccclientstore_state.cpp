#include "eccclientstore_impl_p.h"
#include "eccclientstore_recoverypolicy.h"
#include "eccclientstore_observationpolicy.h"
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include <QDir>
#include <QFileInfo>
#include <stdexcept>
using namespace EccClientStoreStorage;
namespace { using namespace GetDAQ;
void Require(bool value, const char *message) { if (!value) throw std::runtime_error(message); }
std::string Text(const JsonValue &v) { Require(v.type==JsonValue::String, "Invalid ECC client journal text"); return v.text; }
bool Terminal(const EccControlOperation &op) { return op.phase=="complete" || op.phase=="failed"; }
}

EccClientStore::Impl::Impl(const QString &directory, const EccSystemClientOptions &config) : root(directory), options(config), journal(directory+"/observations.jsonl") {
        Require(QDir::isAbsolutePath(root) && QDir::cleanPath(root)==root, "ECC client root must be an absolute normalized path");
        const bool existing=QFileInfo::exists(root);
        if (!existing) Directory(root);
        rootGuard.reset(new Guard(root, true)); lease.reset(new QLockFile(root+"/client.lock")); lease->setStaleLockTime(0);
        Require(lease->tryLock(0), "Another GUI owns this ECC client record root");
        Require(QFile::setPermissions(root+"/client.lock", QFile::ReadOwner | QFile::WriteOwner), "Cannot protect ECC client lease");
        leaseGuard.reset(new Guard(root+"/client.lock",false)); leaseBytes=Read(root+"/client.lock",8192);
        policy=QByteArray::fromStdString("{\"schema\":1,\"kind\":\"getdaq.ecc-client-records\",\"service_id\":"+EncodeJsonString(config.serviceId.toUtf8().toStdString())+
            ",\"client_id\":"+EncodeJsonString(config.clientId.toUtf8().toStdString())+",\"client_epoch\":"+EncodeJsonString(config.clientEpoch.toStdString())+
            ",\"snapshot_sha256\":"+EncodeJsonString(config.snapshotSha256.toStdString())+",\"input_binding_sha256\":"+EncodeJsonString(config.inputBindingSha256.toStdString())+
            ",\"address\":"+EncodeJsonString(config.address.toStdString())+",\"port\":"+std::to_string(config.port)+"}\n");
        if (!existing) { Directory(root+"/requests"); WriteNew(root+"/policy.json", policy); WriteNew(root+"/observations.jsonl", {}); }
        requestsGuard.reset(new Guard(root+"/requests", true)); policyGuard.reset(new Guard(root+"/policy.json", false)); journalGuard.reset(new Guard(journal.fileName(), false));
        Require(Read(root+"/policy.json", 16384)==policy, "ECC client fixed policy differs");
        journalBytes=Read(journal.fileName(), 64*1024*1024); Require(journalBytes.isEmpty() || journalBytes.endsWith('\n'), "Incomplete ECC client journal; no repair/replay");
        const auto lines=journalBytes.split('\n');
        for (int row=0; row<lines.size()-1; ++row) {
            const auto &line=lines[row]; Require(!line.isEmpty(), "Blank ECC client journal record");
            JsonLimits limits;limits.stringBytes=65536;const auto v=ParseStrictJson(line.toStdString(),limits); Require(v.type==JsonValue::Object && v.members.size()==5 && v.At("schema").type==JsonValue::Number && v.At("schema").text=="1" &&
                v.At("sequence").type==JsonValue::Number && v.At("sequence").text==std::to_string(sequence++), "ECC client journal sequence/schema differs");
            const auto kind=Text(v.At("kind")), payload=Text(v.At("payload")), key=Text(v.At("request_key"));
            if(kind=="retirement-context") {
                Require(retirementContextKey.empty(),"Repeated retirement context");
                for(const auto &request:requests)Require(request.command!="retire-data-plans","Retirement context was appended after its original intent; no retrofit");
                retirementContextBytes=Read(root+"/plan-retirement-context.json",1024*1024);
                Require(ConfigurationSha256(retirementContextBytes.toStdString())==payload && key.size()==64,"Retirement context digest differs");
                DecodeEccControlSystemStatus(retirementContextBytes.toStdString());
                retirementContextKey=key;retirementContextGuard=std::make_shared<Guard>(root+"/plan-retirement-context.json",false);
            } else if (kind=="intent") {
                const auto request=DecodeEccControlRequest(payload); Bind(request); const auto id=QString::fromStdString(request.operationId);
                Require(!requests.contains(id) && requests.size()<8192 && key==EccControlRequestKey(request), "ECC client repeated/different intent or budget exhausted");
                requests[id]=request; requestGuards[id]=std::make_shared<Guard>(root+"/requests/"+id+".json", false);
                if(request.command=="retire-data-plans" && !retirementContextKey.empty())CheckRetirementContext(request);
                if(request.command=="complete-data-handoff") {CheckCompletion(request);lastRetirementCompletion=id;}
            } else if (kind=="observation") {
                const auto operation=DecodeEccControlOperation(payload); Require(key==operation.requestKey, "ECC client observation key differs"); GetDAQ::ApplyEccClientObservation(operation,requests,observations,heldOperations,reconciliation);
            } else throw std::runtime_error("Unsupported ECC client journal record");
        }
        if(!retirementContextKey.empty()) {
            bool found=false;for(const auto &request:requests)if(request.command=="retire-data-plans") {CheckRetirementContext(request);found=true;}
            Require(found,"Retirement context has no committed intent; preserve incomplete evidence");
        }
        Require(journal.open(QIODevice::WriteOnly | QIODevice::Append), "Cannot open ECC client observation journal"); Check();
    }

void EccClientStore::Impl::Bind(const EccControlRequest &request) {
        Require(request.clientId==options.clientId.toUtf8().toStdString() && request.clientEpoch==options.clientEpoch.toStdString() && request.snapshotSha256==options.snapshotSha256.toStdString(), "ECC intent client/snapshot scope differs");
        if (instance.empty()) instance=request.instanceId;
        Require(request.instanceId==instance, "ECC intent instance differs; no automatic takeover");
    }

void EccClientStore::Impl::Check() {
        Require(!fault, "ECC client storage fault is latched");
        try {
            rootGuard->Check(); requestsGuard->Check(); policyGuard->Check(); journalGuard->CheckHandle(journal.handle()); leaseGuard->Check();
            Require(Read(root+"/client.lock",8192)==leaseBytes, "ECC client lease changed");
            Require(Read(root+"/policy.json",16384)==policy && Read(journal.fileName(),64*1024*1024)==journalBytes, "ECC client policy/journal content changed");
            if(retirementContextGuard) {
                retirementContextGuard->Check();Require(Read(root+"/plan-retirement-context.json",1024*1024)==retirementContextBytes,"Original retirement context changed");
            } else Require(!QFileInfo::exists(root+"/plan-retirement-context.json"),"Uncommitted retirement context exists; preserve evidence");
            const auto files=QDir(root+"/requests").entryList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot);
            Require(files.size()==requests.size(), "ECC client request set changed or contains an orphan");
            for (auto it=requests.constBegin(); it!=requests.constEnd(); ++it) {
                requestGuards[it.key()]->Check(); Require(Read(root+"/requests/"+it.key()+".json",8192)==QByteArray::fromStdString(EncodeEccControlRequest(it.value())), "ECC client accepted request content changed");
            }
        } catch (...) { fault=reconciliation=true; throw; }
    }

std::vector<EccControlRequest> EccClientStore::Impl::Pending() const {
        std::vector<EccControlRequest> pending;
        for (auto it=requests.constBegin(); it!=requests.constEnd(); ++it) if (!observations.contains(it.key()) || !Terminal(observations[it.key()])) pending.push_back(it.value());
        return pending;
    }

bool EccClientStore::Impl::CanResumeCatalogReplacement(const std::string &operationId) const {
        return EccClientStoreRecoveryPolicy::CanResumeCatalogReplacement(
            operationId, requests, observations, heldOperations, instance, options, Pending().empty());
    }

void EccClientStore::Impl::CheckRetirementContext(const EccControlRequest &request) const {
        EccClientStoreRecoveryPolicy::CheckRetirementContext(request, retirementContextKey, retirementContextBytes);
    }

void EccClientStore::Impl::CheckCompletion(const EccControlRequest &request) const {
        EccClientStoreRecoveryPolicy::CheckCompletion(request, requests, observations, heldOperations,
            retirementContextKey, retirementContextBytes, lastRetirementCompletion);
    }

void EccClientStore::Impl::Append(const std::string &kind, const std::string &key, const std::string &payload) {
        const auto row=QByteArray::fromStdString("{\"schema\":1,\"sequence\":"+std::to_string(sequence)+",\"kind\":"+EncodeJsonString(kind)+",\"request_key\":"+EncodeJsonString(key)+",\"payload\":"+EncodeJsonString(payload,65536)+"}\n");
        Require(journalBytes.size()+row.size()<=64*1024*1024, "ECC client journal budget exhausted"); Require(journal.write(row)==row.size(), "Cannot append complete ECC client journal"); SyncFile(journal); journalBytes+=row; ++sequence;
    }
