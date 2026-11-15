#include "eccclientstore_impl_p.h"
#include <utility>
#include <QString>

void EccClientStore::Check() { fImpl->Check(); }
std::vector<GetDAQ::EccControlRequest> EccClientStore::Pending() const { return fImpl->Pending(); }
bool EccClientStore::FindRequest(const std::string &operationId,
                                GetDAQ::EccControlRequest *request) const {
    auto &p=*fImpl;p.Check();
    const auto it=p.requests.constFind(QString::fromStdString(operationId));
    if(it==p.requests.constEnd())return false;
    if(request)*request=it.value();
    return true;
}
bool EccClientStore::CanResumeCatalogReplacement(const std::string &operationId,
                                                  GetDAQ::EccControlOperation *operation) const {
    auto &p=*fImpl;p.Check();
    if(!p.CanResumeCatalogReplacement(operationId))return false;
    if(operation)*operation=p.observations[QString::fromStdString(operationId)];
    return true;
}
bool EccClientStore::NeedsReconciliation() const { return fImpl->fault || fImpl->reconciliation; }
