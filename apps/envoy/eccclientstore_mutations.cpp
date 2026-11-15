#include "eccclientstore_impl_p.h"
#include "eccclientstore_observationpolicy.h"
#include "eccclientstore_recoverypolicy.h"
#include "eccclientstore_storage_p.h"
#include "configurationsha256.h"
#include <stdexcept>
using namespace EccClientStoreStorage;
namespace { using namespace GetDAQ; void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);} QString Failure(const std::exception &e){return QString::fromUtf8(e.what()).left(512);} }

bool EccClientStore::WriteIntent(const std::string &bytes, QString *error,const std::string &retirementStatus) {
    auto &p=*fImpl;
    EccControlRequest request; QString id;
    try {
        p.Check(); request=DecodeEccControlRequest(bytes); p.Bind(request); id=QString::fromStdString(request.operationId);
        if(request.command=="complete-data-handoff")p.CheckCompletion(request);
        else Require(!PlanRetirement(nullptr),"Original client has a plan retirement intent; no new control commands");
        Require(p.Pending().empty(), "Pending ECC intent must be observed; no replay/new command");
        const bool catalogRecovery=request.command=="resume-catalog-replacement" && p.CanResumeCatalogReplacement(request.failedOperationId);
        Require(!p.reconciliation || catalogRecovery || request.command=="complete-data-handoff" || request.command=="abort-failed-receivers" || request.command=="retire-failed-receivers" || request.command=="retire-failed-sources" || request.command=="unprepare-acquisition-after-failure" || request.command=="stop-production-after-failure" || request.command=="unprepare-after-failure" || request.command=="breakup-after-failure" || (request.command=="reconcile-description" || request.command=="release-stopped-alarm" || (request.command=="reconcile-preparation" || request.command=="reconcile-preparation-with-alarm") || (request.command=="reconcile-acquisition" || request.command=="reconcile-acquisition-with-sources") || request.command=="reconcile-acquisition-with-alarm"), "ECC client requires reconciliation before positive commands");
        Require(!p.requests.contains(id) && p.requests.size()<8192, "ECC client operation ID exists or budget exhausted");
    } catch (const std::exception &e) { if (error) *error=Failure(e); return false; }
    try {
        if(request.command=="retire-data-plans" && !retirementStatus.empty()) {
            Require(p.retirementContextKey.empty() && retirementStatus.size()<=1024*1024,"Retirement context already exists or exceeds limit");
            p.retirementContextBytes=QByteArray::fromStdString(retirementStatus);p.retirementContextKey=EccControlRequestKey(request);
            p.CheckRetirementContext(request);
            WriteNew(p.root+"/plan-retirement-context.json",p.retirementContextBytes);
            p.retirementContextGuard=std::make_shared<Guard>(p.root+"/plan-retirement-context.json",false);
            p.Append("retirement-context",p.retirementContextKey,ConfigurationSha256(retirementStatus));
        }
        WriteNew(p.root+"/requests/"+id+".json",QByteArray::fromStdString(EncodeEccControlRequest(request))); p.requests[id]=request;
        p.requestGuards[id]=std::make_shared<Guard>(p.root+"/requests/"+id+".json",false);
        p.Append("intent",EccControlRequestKey(request),EncodeEccControlRequest(request));
        if(request.command=="complete-data-handoff")p.lastRetirementCompletion=id;
        p.Check(); return true;
    } catch (const std::exception &e) { p.fault=p.reconciliation=true; if (error) *error=Failure(e); return false; }
}
bool EccClientStore::Observe(const std::string &bytes, QString *error) {
    auto &p=*fImpl;
    try { p.Check(); const auto operation=DecodeEccControlOperation(bytes); GetDAQ::ApplyEccClientObservation(operation,p.requests,p.observations,p.heldOperations,p.reconciliation); p.Append("observation",operation.requestKey,bytes); p.Check(); return true; }
    catch (const std::exception &e) { p.fault=p.reconciliation=true; if (error) *error=Failure(e); return false; }
}
