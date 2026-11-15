#ifndef ECCCLIENTSTORE_H
#define ECCCLIENTSTORE_H
#include "eccsystemclient.h"
#include <memory>
#include <vector>
// 低频控制记录：不可覆盖请求、顺序观察日志和同目录单写者。
// 不存凭据、不重放命令、不读原始数据；同控制线程使用。
class EccClientStore
{
public:
    EccClientStore(const QString &root, const EccSystemClientOptions &options);
    ~EccClientStore();
    bool WriteIntent(const std::string &bytes, QString *error,const std::string &retirementStatus=std::string());
    bool Observe(const std::string &operationBytes, QString *error);
    std::vector<GetDAQ::EccControlRequest> Pending() const;
    bool FindRequest(const std::string &operationId,
                     GetDAQ::EccControlRequest *request) const;
    bool CanResumeCatalogReplacement(const std::string &operationId,
                                     GetDAQ::EccControlOperation *operation=nullptr) const;
    bool NeedsReconciliation() const;
    bool PlanRetirement(GetDAQ::EccControlRequest *request,bool *complete=nullptr) const;
    bool PlanRetirementContext(std::string *status) const;
    bool PlanRetirementCompletion(GetDAQ::EccControlRequest *request,std::string *phase=nullptr) const;
    bool CanCompletePlanRetirement() const;
    bool SavePlanRetirementReceipt(const GetDAQ::EccControlRequest &request,const GetDAQ::EccControlSystemStatus &original,
                                  const std::string &receipt,QString *path,QString *error);
    void Check();
private:
    class Impl;
    std::unique_ptr<Impl> fImpl;
};
#endif
