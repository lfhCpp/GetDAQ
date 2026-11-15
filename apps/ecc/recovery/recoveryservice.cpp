#include "recoveryservice.h"
#include "recoveryfiles_p.h"
#include "recoverytaskstore_p.h"
#include "recoveryservicepolicy_p.h"
#include "coldhandoffprotocol.h"
#include "eccprotocol.h"
#include "planio.h"
#include "configurationsha256.h"
#include "../../../common/text/utf8.h"
#include <QProcess>
#include <QTimer>
#include <QDebug>
#include <fcntl.h>
#include <unistd.h>
#include <map>
namespace GetDAQ {
namespace {
using namespace SupervisorIO;
using RecoveryPrivate::Job;
using RecoveryPrivate::Jobs;
using RecoveryPrivate::PinnedFile;
using RecoveryPrivate::RecoveryServicePolicy;
using RecoveryPrivate::RecoveryTaskStore;
using RecoveryPrivate::RequireRecovery;
std::string Error(const char *message) {return "{\"schema\":1,\"error\":"+EncodeJsonString(EccErrorText(message))+"}\n";}

}
class RecoveryService::Impl {
public:
    RecoveryService *owner;
    RecoveryServicePolicy policy;
    HttpServer server;
    QProcess child;
    Jobs jobs;
    std::string active;
    QByteArray diagnostic;
    bool fault=false,finishing=false;
    explicit Impl(RecoveryService *o,const std::string &file) : owner(o),policy(file),server(o),child(o) {
        jobs=RecoveryTaskStore::Load(policy.Root(),policy.Identity().policySha256);
        Check();
        QObject::connect(&server,&HttpServer::RequestSignal,o,[this](QString method,QString path,QByteArray bytes,HttpConnection *connection){Handle(method,path,bytes,connection);});
        QObject::connect(&child,&QProcess::readyReadStandardOutput,o,[this]{Drain();});
        QObject::connect(&child,&QProcess::readyReadStandardError,o,[this]{Drain();});
        QObject::connect(&child,static_cast<void(QProcess::*)(int,QProcess::ExitStatus)>(&QProcess::finished),o,[this](int code,QProcess::ExitStatus status){Finish(code,status==QProcess::NormalExit);});
        QObject::connect(&child,&QProcess::errorOccurred,o,[this](QProcess::ProcessError e){if(e==QProcess::FailedToStart)Finish(-1,false);});
    }
    ~Impl() {server.close();if(child.state()!=QProcess::NotRunning){qCritical()<<"Recovery shutdown waiting for native child; it is not cancelled";child.waitForFinished(-1);}}
    void Check() {
        RequireRecovery(!fault,"Recovery metadata fault is latched");
        try {
            policy.Check();
            RecoveryTaskStore::Check(policy.Root(),jobs);
        }catch(...){fault=true;throw;}
    }
    bool Unresolved() const {
        for(const auto &entry:jobs)if(entry.second.operation.request.action=="submit" && entry.second.operation.phase=="unknown") {
            bool resolved=false;for(const auto &other:jobs)if(other.second.operation.request.action=="inspect" && other.second.operation.request.attemptId==entry.first && other.second.operation.phase=="complete")resolved=true;
            if(!resolved)return true;
        }return false;
    }
    void Save(Job &job) {RecoveryTaskStore::Save(policy.Root(),job);}
    void Drain() {
        const auto out=child.readAllStandardOutput(),err=child.readAllStandardError();
        // 只保留有界诊断。日志溢出不取消原生控制操作。
        const int remaining=65536-diagnostic.size();diagnostic.append((out+err).left(remaining));
    }
    void Finish(int code,bool normal) {
        if(active.empty() || finishing)return;
        finishing=true;Drain();
        auto &job=jobs.at(active);
        try {
            Check();WriteNew(policy.Root().Path()+"/job-"+active+"/diagnostic.log",diagnostic.toStdString());
            if(normal && code==0) {
                PinnedFile proof(policy.Root().Path()+"/attempt-"+job.operation.request.attemptId+"/receipt.json",1024*1024);
                JsonLimits limits;limits.stringBytes=1024*1024;const auto v=ParseStrictJson(proof.bytes,limits);
                RequireRecovery(String(v.At("original_scope_sha256"))==policy.OriginalStatus().dataScopeSha256 && String(v.At("original_epoch"))==policy.OriginalStatus().dataEpoch &&
                    String(v.At("successor_scope_sha256"))==policy.OriginalRequest().successorScopeSha256 && String(v.At("successor_epoch"))==policy.OriginalRequest().successorEpoch,"Cold native receipt target differs");
                if(policy.OriginalRequest().command=="retire-data-plans")ValidateEccPlanRetirementReceipt(proof.bytes,policy.OriginalRequest(),policy.OriginalStatus());
                job.operation.phase="complete";job.operation.receipt=proof.bytes;job.operation.receiptSha256=ConfigurationSha256(proof.bytes);
                job.operation.message="Native proof complete; original failure and old hardware fence remain";
            }else {job.operation.phase=normal ? "failed" : "unknown";job.operation.message="Native child did not prove completion; inspect saved evidence and diagnostic.log; no automatic retry";}
            Save(job);
        }catch(const std::exception &e) {fault=true;job.operation.phase="unknown";job.operation.receipt.clear();job.operation.receiptSha256.clear();job.operation.message="Recovery metadata/result could not be persisted; no replay";qCritical().noquote()<<e.what();}
        active.clear();finishing=false;
    }
    ColdHandoffOperation Submit(const ColdHandoffRequest &request) {
        Check();const auto existing=jobs.find(request.taskId);
        if(existing!=jobs.end()) {RequireRecovery(existing->second.operation.requestKey==ColdHandoffRequestKey(request),"Task ID already binds a different request");return existing->second.operation;}
        RequireRecovery(request.instanceId==policy.Identity().instanceId && request.policySha256==policy.Identity().policySha256,"Recovery instance/policy differs; observe the original task ID");
        RequireRecovery(active.empty() && jobs.size()<256,"Recovery service busy/history full; not queued");
        if(request.action=="submit")RequireRecovery(!Unresolved(),"An earlier submit remains unknown; inspect that original attempt before a new submit");
        else {const auto originalJob=jobs.find(request.attemptId);RequireRecovery(originalJob!=jobs.end() && originalJob->second.operation.request.action=="submit","Inspect requires an original submit in this fixed service history");}
        RecoveryTaskStore::CreateRequest(policy.Root(),request);
        Job job;job.operation.request=request;job.operation.requestKey=ColdHandoffRequestKey(request);job.operation.phase="accepted";
        auto &saved=jobs.emplace(request.taskId,job).first->second;
        try {
            Save(saved);saved.operation.phase="running";Save(saved);Check();active=request.taskId;diagnostic.clear();
            const QStringList args={QString::fromStdString(request.action=="submit" ? "cold-complete-submit" : "cold-complete-inspect"),QString::fromStdString(policy.OldConfig()),QString::fromStdString(policy.NewConfig()),QString::fromStdString(policy.SuccessorPlan()),QString::fromStdString(policy.OriginalEvidence()),QString::fromStdString(request.attemptId),QString::fromStdString(policy.Root().Path()+"/attempt-"+request.attemptId)};
            child.start(QString::fromStdString(policy.Executable()),args);
        }catch(...) {fault=true;throw;}
        return saved.operation;
    }
    void Handle(const QString &method,const QString &path,const QByteArray &bytes,HttpConnection *connection) {
        const auto reply=[&](int code,const std::string &body){connection->Reply(code,QByteArray::fromStdString(body));};
        try {
            if(!policy.Authorize(connection->RequestHeader("authorization").toStdString())){reply(401,Error("Authentication required"));return;}
            Check();
            if(method=="GET" && bytes.isEmpty()) {
                if(path=="/v1/cold-handoff/identity"){reply(200,EncodeColdHandoffIdentity(policy.Identity()));return;}
                const QString prefix="/v1/cold-handoff/tasks/";
                if(path.startsWith(prefix)) {
                    const auto found=jobs.find(path.mid(prefix.size()).toStdString());if(found==jobs.end()){reply(404,Error("Original recovery task not found; absence does not authorize resubmission"));return;}
                    reply(200,EncodeColdHandoffOperation(found->second.operation));return;
                }
                reply(404,Error("Unknown recovery endpoint"));return;
            }
            if(method!="POST"){reply(405,Error("Use GET identity/task or POST tasks"));return;}
            if(path!="/v1/cold-handoff/tasks"){reply(404,Error("Unknown recovery endpoint"));return;}
            if(connection->RequestHeader("content-type")!="application/json"){reply(400,Error("Use application/json"));return;}
            ColdHandoffRequest request;
            try {request=DecodeColdHandoffRequest(bytes.toStdString());}catch(const std::exception &e){reply(400,Error(e.what()));return;}
            reply(202,EncodeColdHandoffOperation(Submit(request)));
        }catch(const std::exception &e){reply(fault ? 503 : 409,Error(e.what()));}
    }
};
RecoveryService::RecoveryService(const std::string &file,QObject *parent) : QObject(parent),fImpl(new Impl(this,file)) {}
RecoveryService::~RecoveryService() {}
void RecoveryService::Start() {
    fImpl->Check();RequireRecovery(!fImpl->server.isListening() && fImpl->server.listen(QHostAddress(QString::fromStdString(fImpl->policy.BindAddress())),quint16(fImpl->policy.Port())),"Cannot listen on recovery endpoint");
    const int fd=int(fImpl->server.socketDescriptor()),flags=::fcntl(fd,F_GETFD);RequireRecovery(flags>=0 && !::fcntl(fd,F_SETFD,flags|FD_CLOEXEC),"Cannot protect recovery listener descriptor");
}
bool RecoveryService::Active() const {return !fImpl->active.empty();}
unsigned int RecoveryService::Port() const {return fImpl->server.serverPort();}
}
