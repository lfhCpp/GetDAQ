#include "repositoryservice.h"
#include <stdexcept>
#include "strictjson.h"
#include "systemexperimentconfiguration.h"
#include <QFutureWatcher>
#include <QPointer>
#include <QHostAddress>
#include <QThreadPool>
#include <QtConcurrent/QtConcurrentRun>
namespace {
struct Result {int code=200;std::string json;};
std::string Error(const char *message) {return "{\"schema_version\":1,\"error\":"+GetDAQ::EncodeJsonString(message)+"}\n";}
}
RepositoryService::RepositoryService(const std::string &root,const std::string &credential,QObject *parent,const std::string &readCredential):QObject(parent),fRepository(root),fCredential(credential),fServer(this) {
    if(!readCredential.empty())fReadCredential.reset(new GetDAQ::RepositoryCredential(readCredential));
    fRepositoryId=GetDAQ::ParseStrictJson(fRepository.IdentityJson()).At("repository_id").text;
    connect(&fServer,&HttpServer::RequestSignal,this,&RepositoryService::Handle);
}
RepositoryService::~RepositoryService() {fServer.close();QThreadPool::globalInstance()->waitForDone();}
void RepositoryService::Start(const QString &address,quint16 port) {
    const QHostAddress parsed(address);if (parsed.protocol()!=QAbstractSocket::IPv4Protocol || parsed.toString()!=address) throw std::invalid_argument("Repository requires canonical IPv4 bind address");
    if (!fServer.listen(parsed,port)) throw std::runtime_error("Cannot listen on configuration repository endpoint");
}
void RepositoryService::Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection) {
    try {
        const auto auth=connection->RequestHeader("authorization").toStdString();
        if (!fCredential.Authorize(auth) && !(method=="GET" && fReadCredential && fReadCredential->Authorize(auth))) {connection->Reply(401,QByteArray::fromStdString(Error("Authentication required")));return;}
        if (fBusy) {connection->Reply(409,QByteArray::fromStdString(Error("Repository busy; request not queued")));return;}
        if (method=="GET" && body.isEmpty() && path=="/v1/identity") {connection->Reply(200,QByteArray::fromStdString(fRepository.IdentityJson()));return;}
        if (method=="GET" && body.isEmpty() && path=="/v1/deployment-capabilities") {connection->Reply(200,"{\"protocol\":1,\"object_content\":true,\"distributed_releases\":true}");return;}
        if (method=="POST" && connection->RequestHeader("x-repository-id").toStdString()!=fRepositoryId) {connection->Reply(412,QByteArray::fromStdString(Error("Repository identity differs")));return;}
        const auto verb=method.toStdString(),resource=path.toStdString();
        const bool put=verb=="POST" && resource.compare(0,12,"/v1/objects/")==0;
        const bool commit=verb=="POST" && resource=="/v1/commit";
        const bool object=verb=="GET" && resource.compare(0,12,"/v1/objects/")==0 && body.isEmpty();
        const bool version=verb=="GET" && resource.compare(0,13,"/v1/versions/")==0 && body.isEmpty();
        const bool list=verb=="GET" && resource=="/v1/versions" && body.isEmpty();
        const bool content=verb=="GET" && resource.compare(0,12,"/v1/content/")==0 && body.isEmpty();
        const bool deploy=verb=="POST" && resource=="/v1/deployments";
        const bool deployment=verb=="GET" && resource.compare(0,16,"/v1/deployments/")==0 && body.isEmpty();
        if (!put && !commit && !object && !version && !list && !content && !deploy && !deployment) {connection->Reply(404,QByteArray::fromStdString(Error("Unknown configuration endpoint")));return;}
        if ((put && connection->RequestHeader("content-type")!="application/octet-stream") || ((commit || deploy) && connection->RequestHeader("content-type")!="application/json")) {connection->Reply(400,QByteArray::fromStdString(Error("Invalid configuration content type")));return;}
        fBusy=true;const QPointer<HttpConnection> target(connection);auto watcher=new QFutureWatcher<Result>(this);const auto bytes=body.toStdString();
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target,content] {const auto result=watcher->result();fBusy=false;if (target) target->Reply(result.code,QByteArray::fromStdString(result.json),content && result.code==200?"application/octet-stream":"application/json");watcher->deleteLater();});
        watcher->setFuture(QtConcurrent::run([this,resource,bytes,put,commit,object,version,list,content,deploy,deployment] {
            Result result;
            try {
                if (put) {fRepository.PutObject(resource.substr(12),bytes);result.json="{\"stored\":true}\n";}
                else if (object) result.json=std::string("{\"present\":")+(fRepository.HasObject(resource.substr(12)) ? "true}\n" : "false}\n");
                else if (version) result.json=fRepository.VersionJson(resource.substr(13));
                else if (list) result.json=fRepository.ListJson();
                else if (content) result.json=fRepository.ReadObject(resource.substr(12));
                else if (deployment) result.json=fRepository.DeploymentManifest(resource.substr(16));
                else if (deploy) {
                    const auto request=GetDAQ::ParseStrictJson(bytes);
                    if(request.type!=GetDAQ::JsonValue::Object || request.members.size()!=1 || request.At("manifest_objects").type!=GetDAQ::JsonValue::Array)throw std::invalid_argument("Invalid deployment registration request");
                    std::vector<std::string> chunks;for(const auto &item:request.At("manifest_objects").items){if(item.type!=GetDAQ::JsonValue::String)throw std::invalid_argument("Invalid manifest object");chunks.push_back(item.text);}result.json=fRepository.RegisterDeployment(chunks);
                }
                else if (commit) {
                    const auto request=GetDAQ::ParseStrictJson(bytes);
                    if (request.type!=GetDAQ::JsonValue::Object || request.members.size()!=2 || request.At("schema_version").type!=GetDAQ::JsonValue::Number || request.At("schema_version").text!="1" || request.At("plan_objects").type!=GetDAQ::JsonValue::Array) throw std::invalid_argument("Invalid repository commit request");
                    std::vector<std::string> plan;for (const auto &item:request.At("plan_objects").items) {if (item.type!=GetDAQ::JsonValue::String) throw std::invalid_argument("Invalid plan object digest");plan.push_back(item.text);}result.json=fRepository.Commit(plan);
                }
            } catch (const std::out_of_range &e) {result.code=404;result.json=Error(e.what());}
            catch (const std::invalid_argument &e) {result.code=400;result.json=Error(e.what());}
            catch (const std::exception &e) {result.code=409;result.json=Error(e.what());}
            return result;
        }));
    } catch (const std::exception &e) {connection->Reply(503,QByteArray::fromStdString(Error(e.what())));}
}
