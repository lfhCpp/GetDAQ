#ifndef ECCCLIENTSTORE_OBSERVATIONPOLICY_H
#define ECCCLIENTSTORE_OBSERVATIONPOLICY_H
#include "eccsystemclient.h"
#include <QMap>
#include <QSet>
namespace GetDAQ {
void ApplyEccClientObservation(const EccControlOperation &operation,
    const QMap<QString, EccControlRequest> &requests,
    QMap<QString, EccControlOperation> &observations,
    QSet<QString> &heldOperations, bool &reconciliation);
}
#endif
