#include "eccservice.h"
#include <stdexcept>
#include "eccservice_p.h"
#include "../native/fixedscanruntimeidentity.h"
#include "systemexperimentconfiguration.h"
#include <QFutureWatcher>
#include <QPointer>
#include <QTimer>
#include <QtConcurrent/QtConcurrentRun>

namespace GetDAQ {
using namespace EccServiceInternal;
void EccService::HandlePost(const QString &path,const QByteArray &body,HttpConnection *connection) {
    const auto reply=[&](int code,const std::string &json) { connection->Reply(code,QByteArray::fromStdString(json)); };
    try {
        if(path=="/v1/shutdown") {
            JsonValue request;
            try {
                JsonLimits limits;limits.stringBytes=256;
                request=ParseStrictJson(body.toStdString(),limits);
                if(request.type!=JsonValue::Object || request.members.size()!=5 ||
                   !request.members.count("schema") || !request.members.count("kind") ||
                   !request.members.count("instance_id") || !request.members.count("snapshot_sha256") ||
                   !request.members.count("handoff_operation_id") || SupervisorIO::Unsigned(request.At("schema"))!=1 ||
                   SupervisorIO::String(request.At("kind"))!="getdaq.ecc-service-shutdown")
                    throw std::invalid_argument("Invalid ECC service shutdown request");
                const auto instance=SupervisorIO::String(request.At("instance_id"));
                const auto snapshot=SupervisorIO::String(request.At("snapshot_sha256"));
                const auto handoff=SupervisorIO::String(request.At("handoff_operation_id"));
                SupervisorIO::Digest(instance);SupervisorIO::Digest(snapshot);SupervisorOperationId(handoff);
                if(fSubmissions || fObservations || fSourceObservations || fScanIdentityObservations) {
                    reply(409,Error("ECC service has an active submission or observation; shutdown refused"));return;
                }
                fJobs->ValidateServiceShutdown(instance,snapshot,handoff);
                fShutdownRequested=true;
                const auto response="{\"schema\":1,\"kind\":\"getdaq.ecc-service-shutdown-accepted\",\"instance_id\":\""+instance+
                    "\",\"snapshot_sha256\":\""+snapshot+"\",\"handoff_operation_id\":\""+handoff+"\"}\n";
                reply(200,response);
                // Let HttpConnection flush and close the authenticated response before app.exec() exits.
                QTimer::singleShot(100,this,[this]{emit ShutdownRequestedSignal();});
            } catch(const std::invalid_argument &e) { reply(400,Error(e.what())); }
              catch(const std::exception &e) { reply(409,Error(e.what())); }
            return;
        }
        if(path=="/v1/scan-runtime-identity") {
            ScanRuntimePointIdentityRequest request;
            try { request=DecodeScanRuntimePointIdentityRequest(body.toStdString()); }
            catch(const std::exception &e) { reply(400,Error(e.what())); return; }
            if(fScanIdentityObservations || fObservations || fSourceObservations || fSubmissions) {
                reply(409,Error("ECC runtime identity observation busy; not queued")); return;
            }
            ++fScanIdentityObservations;
            const QPointer<HttpConnection> target(connection);
            auto *watcher=new QFutureWatcher<Result>(this);
            EccControlJobs *jobs=fJobs.get();
            FixedEccConfiguration *configuration=&fConfiguration;
            connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target] {
                const auto result=watcher->result(); --fScanIdentityObservations;
                if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));
                watcher->deleteLater();
            });
            watcher->setFuture(QtConcurrent::run([jobs,configuration,request] {
                Result result;
                try {
                    ScanRuntimePointIdentityResponse response;
                    response.mode=request.mode;
                    response.identityJson=jobs->IdentityJson();
                    response.statusJson=jobs->StatusJson();
                    response.receiversJson=jobs->ReceiversJson();
                    response.observedControllerState=
                        DecodeEccControlSystemStatus(response.statusJson).controller.state;
                    response.expectation=BuildFixedScanRuntimeIdentityExpectation(
                        request,*configuration,response.identityJson,response.statusJson);
                    result.code=200;
                    result.json=EncodeScanRuntimePointIdentityResponse(response);
                } catch(const std::invalid_argument &e) {
                    result.code=412; result.json=Error(e.what());
                } catch(const std::exception &e) {
                    result.code=503; result.json=Error(e.what());
                } catch(...) {
                    result.code=500; result.json=Error("Non-standard ECC runtime identity observation failure");
                }
                return result;
            }));
            return;
        }
        if (path!="/v1/operations") { reply(404,Error("Unknown ECC control endpoint")); return; }
        EccControlRequest request; try { request=DecodeEccControlRequest(body.toStdString()); } catch (const std::exception &e) { reply(400,Error(e.what())); return; }
        if(fScanIdentityObservations) { reply(409,Error("ECC runtime identity observation busy; operation not queued")); return; }
        if (fSubmissions>=8) { reply(409,Error("ECC submission busy; not queued")); return; }
        ++fSubmissions; const QPointer<HttpConnection> target(connection); auto *watcher=new QFutureWatcher<Result>(this); EccControlJobs *jobs=fJobs.get();
        connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target] { const auto result=watcher->result(); --fSubmissions; if (target) target->Reply(result.code,QByteArray::fromStdString(result.json)); watcher->deleteLater(); });
        watcher->setFuture(QtConcurrent::run([jobs,request] {
            Result result; try { result.json=EncodeEccControlOperation(jobs->Submit(request)); }
            catch (const std::invalid_argument &e) { result.code=412; result.json=Error(e.what()); }
            catch (const std::exception &e) { result.code=409; result.json=Error(e.what()); }
            catch (...) { result.code=500; result.json=Error("Non-standard ECC acceptance failure"); } return result;
        }));
    } catch (const std::invalid_argument &e) { reply(400,Error(e.what())); }
      catch (const std::exception &e) { reply(503,Error(e.what())); }
}
}
