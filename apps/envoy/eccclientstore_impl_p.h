#ifndef ECCCLIENTSTORE_IMPL_P_H
#define ECCCLIENTSTORE_IMPL_P_H

#include "eccclientstore.h"
#include "eccclientstore_storage_p.h"
#include <QByteArray>
#include <QFile>
#include <QLockFile>
#include <QMap>
#include <QSet>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class EccClientStore::Impl
{
public:
    QString root;
    EccSystemClientOptions options;
    QByteArray policy, journalBytes;
    std::unique_ptr<QLockFile> lease;
    std::unique_ptr<EccClientStoreStorage::Guard> rootGuard, requestsGuard, policyGuard, journalGuard;
    std::unique_ptr<EccClientStoreStorage::Guard> leaseGuard;
    QByteArray leaseBytes;
    QMap<QString, std::shared_ptr<EccClientStoreStorage::Guard>> requestGuards;
    QMap<QString, GetDAQ::EccControlRequest> requests;
    QMap<QString, GetDAQ::EccControlOperation> observations;
    std::shared_ptr<EccClientStoreStorage::Guard> retirementContextGuard;
    QByteArray retirementContextBytes;
    std::string retirementContextKey;
    QString lastRetirementCompletion;
    QFile journal;
    std::uint64_t sequence=0;
    bool fault=false, reconciliation=false;
    QSet<QString> heldOperations;
    std::string instance;

    Impl(const QString &directory, const EccSystemClientOptions &config);
    void Bind(const GetDAQ::EccControlRequest &request);
    void Check();
    std::vector<GetDAQ::EccControlRequest> Pending() const;
    bool CanResumeCatalogReplacement(const std::string &operationId) const;
    void CheckRetirementContext(const GetDAQ::EccControlRequest &request) const;
    void CheckCompletion(const GetDAQ::EccControlRequest &request) const;
    void Append(const std::string &kind, const std::string &key, const std::string &payload);
};

#endif
