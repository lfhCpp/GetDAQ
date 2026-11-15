#include "supervisorservice.h"
#include <stdexcept>
#include "receiverplantransition.h"
#include "planio.h"
#include "systemexperimentconfiguration.h"
#include "configurationsha256.h"
#include "../../../common/text/utf8.h"
#include <QFutureWatcher>
#include <QHostAddress>
#include <QPointer>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>
#include <arpa/inet.h>
#include <fcntl.h>
namespace GetDAQ {
SupervisorServiceConfig SupervisorServiceConfig::Load(const std::string &path) {
    const auto v=ParseStrictJson(SupervisorIO::ReadFile(path,1024*1024));
    std::set<std::string> fields={"schema","kind","agent_id","controller_id","bind","port","credential_file","receiver_plan"};if(v.members.count("transfers"))fields.insert("transfers");ExactSupervisorFields(v,fields);
    if (SupervisorIO::Unsigned(v.At("schema"))!=1 || SupervisorIO::String(v.At("kind"))!="getdaq.supervisor-service") throw std::invalid_argument("Unsupported supervisor service configuration");
    SupervisorServiceConfig c; const auto plan=DecodeSupervisorPlan(v.At("receiver_plan")); c.control.receiver=plan.options; c.control.executableSha256=plan.executableSha256;
    c.control.agentId=SupervisorIO::String(v.At("agent_id")); c.control.controllerId=SupervisorIO::String(v.At("controller_id"));
    c.bindAddress=SupervisorIO::String(v.At("bind")); c.port=SupervisorUnsigned32(v.At("port")); c.credentialFile=SupervisorIO::String(v.At("credential_file"));
    if(fields.count("transfers")) {
        const auto &t=v.At("transfers");ExactSupervisorFields(t,{"backup_root","audit_root","executable","executable_sha256"});c.transfersEnabled=true;
        c.transfers.agentId=c.control.agentId;c.transfers.deploymentId=c.control.receiver.deploymentId;c.transfers.sourceRoot=c.control.receiver.catalogRoot;
        c.transfers.backupRoot=SupervisorIO::String(t.At("backup_root"));c.transfers.auditRoot=SupervisorIO::String(t.At("audit_root"));c.transfers.executable=SupervisorIO::String(t.At("executable"));c.transfers.executableSha256=SupervisorIO::String(t.At("executable_sha256"));
    }
    c.Validate(); return c;
}
void SupervisorServiceConfig::Validate() const {
    using namespace SupervisorIO;
    Text(control.agentId,128); Text(control.controllerId,128); Digest(control.executableSha256); Path(credentialFile); ValidateOptions(control.receiver);
    sockaddr_in a={}; char canonical[INET_ADDRSTRLEN];
    if (::inet_pton(AF_INET,bindAddress.c_str(),&a.sin_addr)!=1 || !::inet_ntop(AF_INET,&a.sin_addr,canonical,sizeof(canonical)) || canonical!=bindAddress || !port || port>65535)
        throw std::invalid_argument("Supervisor service requires canonical IPv4/port");
    for (const auto &s:control.receiver.sources) if (port==s.dataPort || port==s.statusPort || port==s.exportPort) throw std::invalid_argument("Control service port conflicts with receiver plan");
    if (ConfigurationSha256(ReadFile(control.receiver.receiverExecutable,64*1024*1024))!=control.executableSha256) throw std::invalid_argument("Service receiver executable SHA differs");
    if (!control.receiver.iceReceiverExecutable.empty() && ConfigurationSha256(ReadFile(control.receiver.iceReceiverExecutable,64*1024*1024))!=control.receiver.iceReceiverSha256)
        throw std::invalid_argument("Service ICE executable SHA differs");
    SupervisorCredential credential(credentialFile); credential.Check();
    if(transfersEnabled)ValidateTransferJobOptions(transfers);
}
SupervisorService::SupervisorService(const SupervisorServiceConfig &c,QObject *parent) : SupervisorService(c,"",parent) {}
SupervisorService::SupervisorService(const SupervisorServiceConfig &c,const std::string &proof,QObject *parent) : QObject(parent),fConfig(c),fCredential(c.credentialFile),fServer(this) {
    if (!proof.empty()) fPredecessor.reset(new SupervisorPlanPredecessor(proof,c.control));
    connect(&fServer,&HttpServer::RequestSignal,this,&SupervisorService::Handle);
}
SupervisorService::~SupervisorService() {
    fServer.close();
    // Qt任务只进行持久接受；等待它们退出，再结束原生后台动作。窗口/服务退出不kill receiver。
    QThreadPool::globalInstance()->waitForDone(); fTransfers.reset(); fControl.reset();
}
void SupervisorService::Start() {
    fConfig.Validate();
    if (fControl) throw std::logic_error("Supervisor service already started");
    if (!fServer.listen(QHostAddress(QString::fromStdString(fConfig.bindAddress)),quint16(fConfig.port))) throw std::runtime_error("Cannot listen on supervisor control endpoint");
    const int descriptor=int(fServer.socketDescriptor()),flags=::fcntl(descriptor,F_GETFD);
    if (flags<0 || ::fcntl(descriptor,F_SETFD,flags|FD_CLOEXEC)) { fServer.close(); throw std::runtime_error("Cannot close control listener on receiver exec"); }
    try {
        if (fPredecessor) fPredecessor->Activate();
        fControl.reset(new SupervisorControl(fConfig.control,fPredecessor));
        if(fConfig.transfersEnabled)fTransfers.reset(new TransferJobs(fConfig.transfers));
    } catch (...) { fServer.close(); throw; }
}
namespace {
struct SubmitResult { int code=202; std::string json; };
std::string Error(const std::string &error) { return "{\"schema\":1,\"error\":"+EncodeJsonString(EccErrorText(error.c_str()))+"}\n"; }
}
void SupervisorService::Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection) {
    const auto reply=[&](int code,const std::string &json) { connection->Reply(code,QByteArray::fromStdString(json)); };
    try {
        if (!fCredential.Authorize(connection->RequestHeader("authorization").toStdString())) { reply(401,Error("Authentication required")); return; }
        if(path.startsWith("/v1/transfers")) {
            if(!fTransfers){reply(404,Error("Native transfer tasks disabled"));return;}
            if(method=="GET" && body.isEmpty() && path=="/v1/transfers/identity") {reply(200,EncodeTransferIdentity(fTransfers->Identity()));return;}
            const QString operationPrefix="/v1/transfers/operations/",sourcePrefix="/v1/transfers/source/";
            if(method=="GET" && body.isEmpty() && path.startsWith(operationPrefix)) {
                try {reply(200,EncodeTransferOperation(fTransfers->Operation(path.mid(operationPrefix.size()).toStdString())));}catch(const std::out_of_range &){reply(404,Error("Transfer operation not found"));}return;
            }
            if(method=="GET" && body.isEmpty() && path.startsWith(sourcePrefix)) {
                const auto parts=path.mid(sourcePrefix.size()).split('/');bool okay=false;const auto run=parts.size()==2 ? parts[1].toUInt(&okay) : 0;
                if(!okay){reply(400,Error("Use source/EXPERIMENT/RUN_NUMBER"));return;}
                // Metadata-only source inspection may scan a large file set; keep it off the HTTP thread.
                if(fSubmissions>=8){reply(409,Error("Metadata submissions busy"));return;}++fSubmissions;
                const QPointer<HttpConnection> target(connection);auto watcher=new QFutureWatcher<SubmitResult>(this);auto jobs=fTransfers.get();const auto experiment=parts[0].toStdString();
                connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target]{const auto result=watcher->result();--fSubmissions;if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));watcher->deleteLater();});
                watcher->setFuture(QtConcurrent::run([jobs,experiment,run]{SubmitResult result;try{result.code=200;result.json=jobs->SourceReceipt(experiment,run);}catch(const std::exception &e){result.code=409;result.json=Error(e.what());}return result;}));return;
            }
            const bool reconciliation=path=="/v1/transfers/reconciliations";
            const bool resume=path=="/v1/transfers/resumptions";
            if(method!="POST" || (path!="/v1/transfers/operations" && !reconciliation && !resume)){reply(405,Error("Use transfer identity/source/operation GET or operation/recovery POST"));return;}
            if(connection->RequestHeader("content-type")!="application/json"){reply(400,Error("Use application/json"));return;}
            TransferRequest request;TransferReconciliationRequest verify;
            try{if(resume)verify=DecodeTransferResumeRequest(body.toStdString());else if(reconciliation)verify=DecodeTransferReconciliationRequest(body.toStdString());else request=DecodeTransferRequest(body.toStdString());}catch(const std::exception &e){reply(400,Error(e.what()));return;}
            if(fSubmissions>=8){reply(409,Error("Metadata submissions busy"));return;}++fSubmissions;
            const QPointer<HttpConnection> target(connection);auto watcher=new QFutureWatcher<SubmitResult>(this);auto jobs=fTransfers.get();
            connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target]{const auto result=watcher->result();--fSubmissions;if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));watcher->deleteLater();});
            watcher->setFuture(QtConcurrent::run([jobs,request,verify,reconciliation,resume]{SubmitResult result;try{result.json=EncodeTransferOperation(resume ? jobs->Resume(verify) : (reconciliation ? jobs->Reconcile(verify) : jobs->Submit(request)));}catch(const std::exception &e){result.code=409;result.json=Error(e.what());}return result;}));return;
        }
        if (method=="GET" && body.isEmpty()) {
            if (path=="/v1/plan-retirement" || path=="/v1/predecessor" || path=="/v1/plan-retirement-completion" || path=="/v1/control-handoff-completion") {
                if (fMetadataQueries) {reply(409,Error("Plan proof query busy; not queued"));return;}
                ++fMetadataQueries;const bool previous=path=="/v1/predecessor",completion=path=="/v1/plan-retirement-completion",handoffCompletion=path=="/v1/control-handoff-completion";
                const QPointer<HttpConnection> target(connection);auto *watcher=new QFutureWatcher<SubmitResult>(this);auto *control=fControl.get();
                connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target] {
                    const auto result=watcher->result();--fMetadataQueries;if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));watcher->deleteLater();
                });
                watcher->setFuture(QtConcurrent::run([control,previous,completion,handoffCompletion] {
                    SubmitResult result;result.code=200;
                    try {result.json=previous ? control->PredecessorJson() : handoffCompletion ? control->ControlHandoffCompletionJson() : completion ? control->PlanRetirementCompletionJson() : control->PlanRetirementJson();}
                    catch(const std::exception &e) {result.code=503;result.json=Error(e.what());}
                    return result;
                }));return;
            }
            if (path=="/v1/identity") { reply(200,fControl->IdentityJson()); return; }
            if (path=="/v1/status") { reply(200,fControl->StatusJson()); return; }
            if (path=="/v1/receivers") { reply(200,fControl->ReceiversJson()); return; }
            const bool fileBytes=path.startsWith("/v1/receiver-file-bytes/");
            const QString eventPrefix=fileBytes ? "/v1/receiver-file-bytes/" : "/v1/receiver-events/";
            if (path.startsWith(eventPrefix)) {
                bool okay=false; const auto indexText=path.mid(eventPrefix.size()); const auto index=indexText.toUInt(&okay);
                if (!okay || indexText!=QString::number(index)) { reply(400,Error("Use receiver-events/SOURCE_INDEX")); return; }
                if (fMetadataQueries) { reply(409,Error("Receiver event observation busy; not queued")); return; }
                ++fMetadataQueries; const QPointer<HttpConnection> target(connection); auto *watcher=new QFutureWatcher<SubmitResult>(this); auto *control=fControl.get();
                connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target] {
                    const auto result=watcher->result(); --fMetadataQueries; if (target) target->Reply(result.code,QByteArray::fromStdString(result.json)); watcher->deleteLater();
                });
                watcher->setFuture(QtConcurrent::run([control,index,fileBytes] {
                    SubmitResult result; result.code=200;
                    try { result.json=control->ReceiverEventsJson(index,fileBytes); }
                    catch (const std::out_of_range &e) { result.code=404; result.json=Error(e.what()); }
                    catch (const std::exception &e) { result.code=503; result.json=Error(e.what()); }
                    return result;
                })); return;
            }
            const QString prefix="/v1/operations/";
            if (path.startsWith(prefix)) {
                try { reply(200,EncodeSupervisorControlOperation(fControl->Operation(path.mid(prefix.size()).toStdString()))); }
                catch (const std::out_of_range &) { reply(404,Error("Operation not found")); }
                return;
            }
            const QString catalogObservationPrefix="/v1/catalog-replacement-observations/";
            if(path.startsWith(catalogObservationPrefix)) {
                const auto id=path.mid(catalogObservationPrefix.size()).toStdString();
                try { SupervisorOperationId(id); }
                catch(const std::exception &e) { reply(400,Error(e.what())); return; }
                if(fMetadataQueries){reply(409,Error("Catalog replacement observation busy; not queued"));return;}
                ++fMetadataQueries;const QPointer<HttpConnection> target(connection);auto *watcher=new QFutureWatcher<SubmitResult>(this);auto *control=fControl.get();
                connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target]{const auto result=watcher->result();--fMetadataQueries;if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));watcher->deleteLater();});
                watcher->setFuture(QtConcurrent::run([control,id]{SubmitResult result;result.code=200;try{result.json=control->CatalogReplacementObservationJson(id);}catch(const std::out_of_range &e){result.code=404;result.json=Error(e.what());}catch(const std::invalid_argument &e){result.code=412;result.json=Error(e.what());}catch(const std::exception &e){result.code=409;result.json=Error(e.what());}return result;}));return;
            }
            reply(404,Error("Unknown control endpoint")); return;
        }
        if (method!="POST") { reply(405,Error("Use GET status/operation or POST operations")); return; }
        if (path!="/v1/operations") { reply(404,Error("Unknown control endpoint")); return; }
        if (connection->RequestHeader("content-type")!="application/json") { reply(400,Error("Use application/json")); return; }
        SupervisorControlRequest request;
        try { request=DecodeSupervisorControlRequest(body.toStdString()); }
        catch (const std::exception &e) { reply(400,Error(e.what())); return; }
        if (fSubmissions>=8) { reply(409,Error("Control submissions busy; not queued")); return; }
        ++fSubmissions;
        const QPointer<HttpConnection> target(connection); auto *watcher=new QFutureWatcher<SubmitResult>(this); SupervisorControl *control=fControl.get();
        connect(watcher,&QFutureWatcher<SubmitResult>::finished,this,[this,watcher,target] {
            const auto result=watcher->result(); --fSubmissions; if (target) target->Reply(result.code,QByteArray::fromStdString(result.json)); watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run([control,request] {
            SubmitResult result;
            try { result.json=EncodeSupervisorControlOperation(control->Submit(request)); }
            catch (const std::invalid_argument &e) { result.code=412; result.json=Error(e.what()); }
            catch (const std::exception &e) { result.code=409; result.json=Error(e.what()); }
            catch (...) { result.code=500; result.json=Error("Non-standard control acceptance failure"); }
            return result;
        }));
    } catch (const std::invalid_argument &e) { reply(400,Error(e.what())); }
    catch (const std::exception &e) { reply(503,Error(e.what())); }
}
}
