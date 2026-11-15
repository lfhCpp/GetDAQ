#ifndef ECCCLIENTSTORE_RECOVERYPOLICY_H
#define ECCCLIENTSTORE_RECOVERYPOLICY_H
#include "eccsystemclient.h"
#include <QByteArray>
#include <QMap>
#include <QSet>

namespace EccClientStoreRecoveryPolicy {
bool CanResumeCatalogReplacement(const std::string &operationId,
    const QMap<QString, GetDAQ::EccControlRequest> &requests,
    const QMap<QString, GetDAQ::EccControlOperation> &observations,
    const QSet<QString> &heldOperations, const std::string &instance,
    const EccSystemClientOptions &options, bool noPendingOperations);

void CheckRetirementContext(const GetDAQ::EccControlRequest &request,
    const std::string &retirementContextKey, const QByteArray &retirementContextBytes);

void CheckCompletion(const GetDAQ::EccControlRequest &request,
    const QMap<QString, GetDAQ::EccControlRequest> &requests,
    const QMap<QString, GetDAQ::EccControlOperation> &observations,
    const QSet<QString> &heldOperations, const std::string &retirementContextKey,
    const QByteArray &retirementContextBytes, const QString &lastRetirementCompletion);
}
#endif
