#include "eccsuccessorplan.h"
#include "ecccontroljobs.h"
#include "fixedeccconfiguration.h"
#include "supervisorio.h"
#include "systemexperimentconfiguration.h"
#include <fcntl.h>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
namespace {
template<class F> void Reject(F action,const std::string &message) {
    try {action();} catch (const std::exception &e) {
        if (std::string(e.what()).find(message)!=std::string::npos) return;
        throw std::runtime_error(std::string("Wrong rejection: ")+e.what());
    }
    throw std::runtime_error("Expected rejection: "+message);
}
void Require(bool value) {if (!value) throw std::runtime_error("Successor identity assertion failed");}
}
int main(int argc,char **argv) {
    using namespace GetDAQ;using namespace SupervisorIO;
    try {
        if (argc!=4) throw std::invalid_argument("configuration plan_file scenario");
        FixedEccConfiguration config(argv[1]);const std::string file=argv[2],scenario=argv[3];
        const auto original=EccSuccessorPlan::Create(file,config);
        auto plan=std::make_shared<EccSuccessorPlan>(file,config);
        Require(!plan->Activated() && !plan->Faulted() && original==plan->Json());
        if (scenario.find("review-")==0) {
            EccControlRequest request;request.command="retire-data-plans";request.operationId="reviewed-retirement";request.clientId="console";
            request.instanceId=request.snapshotSha256=request.clientEpoch=std::string(64,'1');
            request.successorPlanFile=file;request.successorPlanSha256=plan->Sha256();request.successorScopeSha256=plan->ScopeSha256();request.successorEpoch=plan->Epoch();request.successorDataBindingSha256=plan->DataBindingSha256();
            EccReceiverPlanTarget target;target.agentId="node-0";target.planSha256=std::string(64,'2');target.planFile="/reviewed/data-plan.json";request.successorReceiverPlans.push_back(target);
            if(scenario=="review-lease") {
                EccSuccessorReview review(request);Require(review.Json()==original);
                Reject([&]{plan->ActivateForStartup();},"already held");review.Release();plan->ActivateForStartup();review.Check();
            } else if(scenario=="review-started") {
                plan->ActivateForStartup();Reject([&]{EccSuccessorReview review(request);},"held by another startup");
            } else if(scenario=="review-target") {
                request.successorScopeSha256=std::string(64,'9');
                Reject([&]{EccSuccessorReview review(request);},"differs from retirement");plan->ActivateForStartup();
            } else if(scenario=="review-tamper") {
                EccSuccessorReview review(request);Require(::chmod(file.c_str(),0600)==0);
                Fd output(::open(file.c_str(),O_WRONLY|O_APPEND|O_NOFOLLOW));Require(output.value>=0 && ::write(output.value," ",1)==1 && ::fchmod(output.value,0400)==0);
                Reject([&]{review.Check();},"identity/content");Require(::ftruncate(output.value,original.size())==0);
                Reject([&]{review.Check();},"latched");Reject([&]{review.Release();},"latched");Reject([&]{plan->ActivateForStartup();},"already held");
            } else throw std::invalid_argument("Unknown reviewed successor scenario");
        } else if (scenario=="exclusive") {
            Reject([&]{EccSuccessorPlan::Create(file,config);},"exclusively create");
            WriteNew(file+".copy",original);
            ::chmod((file+".copy").c_str(),0400);
            Reject([&]{EccSuccessorPlan copy(file+".copy",config);},"configuration/path");
        } else if (scenario=="hardlink") {
            Require(::link(file.c_str(),(file+".link").c_str())==0);
            Reject([&]{plan->Check();},"identity/content");
            Require(::unlink((file+".link").c_str())==0);
            Reject([&]{plan->Check();},"latched");
        } else if (scenario=="tamper" || scenario=="marker-tamper") {
            if (scenario=="marker-tamper") plan->ActivateForStartup();
            const auto target=scenario=="tamper" ? file : file+".activation.json";
            const auto saved=ReadFile(target,65536);Require(::chmod(target.c_str(),0600)==0);
            Fd output(::open(target.c_str(),O_WRONLY|O_TRUNC|O_NOFOLLOW));
            Require(output.value>=0 && ::write(output.value,"changed",7)==7);
            Require(::chmod(target.c_str(),0400)==0);
            Reject([&]{plan->Check();},"identity/content");
            Require(::ftruncate(output.value,0)==0 && ::pwrite(output.value,saved.data(),saved.size(),0)==static_cast<ssize_t>(saved.size()));
            Reject([&]{plan->Check();},"latched");
        } else if (scenario=="existing-root") {
            CreateDirectory(config.ControllerOptions().auditRoot);
            Reject([&]{plan->ActivateForStartup();},"absent audit root");Require(plan->Faulted());
        } else if (scenario=="activation" || scenario=="binding" || scenario=="receipt-gate") {
            Reject([&]{RemoteReceiverManager unactivated(config.ReceiverOptions(),plan);},"not activated");
            const auto epoch=plan->Epoch();plan->ActivateForStartup();Require(plan->Activated());plan->Check();
            Reject([&]{plan->ActivateForStartup();},"already activated");
            auto second=std::make_shared<EccSuccessorPlan>(file,config);
            Reject([&]{second->ActivateForStartup();},"already held");
            RemoteReceiverManager data(config.ReceiverOptions(),plan);
            Require(data.Status().controllerEpoch==epoch);
            const auto compiled=PlanNativeEccConfiguration(config.ControllerOptions(),data.BindingSha256());
            if (scenario=="binding") {
                Reject([&]{data.BindController(std::string(64,'a'),plan->AuditRoot());},"differs from frozen");
            } else if (scenario=="receipt-gate") {
                CreateDirectory(plan->AuditRoot());CreateDirectory(plan->AuditRoot()+"/operations");
                WriteNew(plan->AuditRoot()+"/snapshot.json",compiled.snapshotJson);
                data.BindController(compiled.snapshotSha256,plan->AuditRoot());
                Reject([&]{data.VerifyStopped();},"original complete handoff receipt");
                Reject([&]{data.AdoptControlHandoff("{}");},"field");
                Reject([&]{data.VerifyStopped();},"original complete handoff receipt");
            }
            // Reopening a consumed plan after its live lock is released still
            // cannot restart the same receiver epoch using its durable marker.
        } else if (scenario=="jobs-invalid-receipt") {
            Reject([&]{EccControlJobs jobs(config,plan,"{}");},"not activated");
            struct stat st;Require(::lstat(plan->AuditRoot().c_str(),&st)!=0);
            plan->ActivateForStartup();
            CreateDirectory(config.ControllerOptions().hostControlOwnershipRoot);
            Reject([&]{EccControlJobs jobs(config,plan,"{}");},"field");
            Require(::lstat((plan->AuditRoot()+"/snapshot.json").c_str(),&st)==0);
            Require(::lstat((plan->AuditRoot()+"/network").c_str(),&st)!=0);
            Require(::lstat((file+".activation.json").c_str(),&st)==0);
        } else if (scenario=="consumed") {
            plan->ActivateForStartup();plan.reset();
            EccSuccessorPlan reopened(file,config);
            Reject([&]{reopened.ActivateForStartup();},"activation marker");
        } else throw std::invalid_argument("Unknown successor test scenario");
        std::cout<<"PASS "<<scenario<<"; no receiver or raw payload\n";return 0;
    } catch (const std::exception &e) {std::cerr<<e.what()<<'\n';return 1;}
}
