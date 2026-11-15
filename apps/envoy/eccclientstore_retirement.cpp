#include "eccclientstore_impl_p.h"
#include "eccclientstore_storage_p.h"
#include "eccclientstore_recoverypolicy.h"
#include "systemexperimentconfiguration.h"
#include <QFileInfo>
#include <stdexcept>
using namespace EccClientStoreStorage;
namespace { using namespace GetDAQ; void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);} QString Failure(const std::exception &e){return QString::fromUtf8(e.what()).left(512);} }

bool EccClientStore::PlanRetirement(GetDAQ::EccControlRequest *request,bool *complete) const {
    auto &p=*fImpl;p.Check();bool found=false;
    for(auto it=p.requests.constBegin();it!=p.requests.constEnd();++it)if(it->command=="retire-data-plans") {
        Require(!found,"Multiple retirement intents in one original client policy");found=true;if(request)*request=it.value();
        if(complete) {
            *complete=p.observations.contains(it.key()) && p.observations[it.key()].phase=="complete";
            for(const auto &operation:p.observations)if(operation.request.command=="complete-data-handoff" &&
                operation.request.failedOperationId==it->operationId && operation.phase=="complete")*complete=true;
        }
    }
    return found;
}
bool EccClientStore::PlanRetirementContext(std::string *status) const {
    auto &p=*fImpl;p.Check();if(p.retirementContextKey.empty())return false;
    GetDAQ::EccControlRequest request;Require(PlanRetirement(&request),"Original retirement intent missing");p.CheckRetirementContext(request);
    if(status)*status=p.retirementContextBytes.toStdString();
    return true;
}
bool EccClientStore::PlanRetirementCompletion(GetDAQ::EccControlRequest *request,std::string *phase) const {
    auto &p=*fImpl;p.Check();if(p.lastRetirementCompletion.isEmpty())return false;
    if(request)*request=p.requests[p.lastRetirementCompletion];
    if(phase)*phase=p.observations.contains(p.lastRetirementCompletion) ? p.observations[p.lastRetirementCompletion].phase : "unconfirmed";
    return true;
}
bool EccClientStore::CanCompletePlanRetirement() const {
    auto &p=*fImpl;p.Check();GetDAQ::EccControlRequest original;bool complete=false;
    if(!PlanRetirement(&original,&complete) || complete || !p.Pending().empty())return false;
    GetDAQ::EccControlRequest request;request.command="complete-data-handoff";request.failedOperationId=original.operationId;
    request.instanceId=original.instanceId;request.snapshotSha256=original.snapshotSha256;request.clientId=original.clientId;request.clientEpoch=original.clientEpoch;
    try {p.CheckCompletion(request);return true;}catch(const std::exception &){return false;}
}
bool EccClientStore::SavePlanRetirementReceipt(const GetDAQ::EccControlRequest &request,const GetDAQ::EccControlSystemStatus &original,
                                            const std::string &receipt,QString *path,QString *error) {
    auto &p=*fImpl;
    try {
        p.Check();GetDAQ::EccControlRequest saved;bool complete=false;
        Require(PlanRetirement(&saved,&complete) && complete && EncodeEccControlRequest(saved)==EncodeEccControlRequest(request),"Receipt requires the original completed persisted retirement intent");
        const auto before=p.retirementContextKey.empty() ? original : DecodeEccControlSystemStatus(p.retirementContextBytes.toStdString());
        ValidateEccPlanRetirementReceipt(receipt,request,before);
        const auto file=p.root+"/plan-retirement-"+QString::fromStdString(request.operationId)+".json";
        const auto bytes=QByteArray::fromStdString(receipt);
        if(QFileInfo::exists(file)) {Guard guard(file,false);Require(Read(file,1024*1024)==bytes,"Existing retirement receipt differs; do not overwrite");}
        else WriteNew(file,bytes);
        p.Check();if(path)*path=file;return true;
    } catch(const std::exception &e) {p.fault=p.reconciliation=true;if(error)*error=Failure(e);return false;}
}
