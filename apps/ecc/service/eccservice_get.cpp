#include "eccservice.h"
#include <stdexcept>
#include "eccservice_p.h"
#include <QFutureWatcher>
#include <QPointer>
#include <QtConcurrent/QtConcurrentRun>

namespace GetDAQ {
using namespace EccServiceInternal;
void EccService::HandleGet(const QString &path,HttpConnection *connection) {
    const auto reply=[&](int code,const std::string &json) { connection->Reply(code,QByteArray::fromStdString(json)); };
    try {
            if (path=="/v1/identity") { reply(200,fJobs->IdentityJson()); return; }
            if (path=="/v1/status") { reply(200,fJobs->StatusJson()); return; }
            if (path=="/v1/alarms") { reply(200,fJobs->AlarmsJson()); return; }
            if (path=="/v1/receiver-failure") { reply(200,fJobs->ReceiverFailureJson()); return; }
            const bool fileBytes=path.startsWith("/v1/receiver-file-bytes/");
            const QString receiverEventsPrefix=fileBytes ? "/v1/receiver-file-bytes/" : "/v1/receiver-events/";
            if (path.startsWith(receiverEventsPrefix)) {
                std::string senderId;
                try { senderId=DecodeReceiverSourcePath(path.mid(receiverEventsPrefix.size())); }
                catch (const std::exception &e) { reply(400,Error(e.what())); return; }
                if (fObservations || fSourceObservations || fScanIdentityObservations) { reply(409,Error("ECC receiver event observation busy; not queued")); return; }
                ++fSourceObservations; const QPointer<HttpConnection> target(connection); auto *watcher=new QFutureWatcher<Result>(this); EccControlJobs *jobs=fJobs.get();
                connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target] {
                    const auto result=watcher->result(); --fSourceObservations; if (target) target->Reply(result.code,QByteArray::fromStdString(result.json)); watcher->deleteLater();
                });
                watcher->setFuture(QtConcurrent::run([jobs,senderId,fileBytes] {
                    Result result; result.code=200;
                    try { result.json=jobs->ReceiverEventsJson(senderId,fileBytes); }
                    catch (const std::out_of_range &e) { result.code=404; result.json=Error(e.what()); }
                    catch (const std::exception &e) { result.code=503; result.json=Error(e.what()); }
                    catch (...) { result.code=500; result.json=Error("Non-standard ECC receiver event observation failure"); }
                    return result;
                })); return;
            }
            if(path=="/v1/sources" || path=="/v1/hardware-state") {
                if(fSourceObservations || fObservations || fScanIdentityObservations) {reply(409,Error("Source observation busy; not queued"));return;}
                ++fSourceObservations;const QPointer<HttpConnection> target(connection);auto *watcher=new QFutureWatcher<Result>(this);EccControlJobs *jobs=fJobs.get();
                connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target] {
                    const auto result=watcher->result();--fSourceObservations;if(target)target->Reply(result.code,QByteArray::fromStdString(result.json));watcher->deleteLater();
                });
                const bool hardware=path=="/v1/hardware-state";
                watcher->setFuture(QtConcurrent::run([jobs,hardware] {
                    Result result;result.code=200;try {result.json=hardware ? jobs->HardwareStateJson() : jobs->SourcesJson();}
                    catch(const std::exception &e) {result.code=503;result.json=Error(e.what());}catch(...) {result.code=500;result.json=Error("Nonstandard source observation failure");}return result;
                }));return;
            }
            if (path=="/v1/receivers" || path=="/v1/control-handoff") {
                if (fObservations || fSourceObservations || fScanIdentityObservations) { reply(409,Error("Receiver observation busy; not queued")); return; }
                ++fObservations; const QPointer<HttpConnection> target(connection);
                auto *watcher=new QFutureWatcher<Result>(this); EccControlJobs *jobs=fJobs.get();
                connect(watcher,&QFutureWatcher<Result>::finished,this,[this,watcher,target] {
                    const auto result=watcher->result(); --fObservations;
                    if (target) target->Reply(result.code,QByteArray::fromStdString(result.json));
                    watcher->deleteLater();
                });
                const bool handoff=path=="/v1/control-handoff";
                watcher->setFuture(QtConcurrent::run([jobs,handoff] {
                    Result result; result.code=200;
                    try { result.json=handoff ? jobs->ControlHandoffJson() : jobs->ReceiversJson(); }
                    catch (const std::exception &e) { result.code=503; result.json=Error(e.what()); }
                    catch (...) { result.code=500; result.json=Error("Non-standard receiver observation failure"); }
                    return result;
                })); return;
            }
            if (path=="/v1/configuration") { reply(200,fJobs->ConfigurationJson()); return; }
            if (path=="/v1/cobo-maintenance-targets") { reply(200,fJobs->MaintenanceTargetsJson()); return; }
            if (path=="/v1/ownership") { reply(200,fJobs->OwnershipJson()); return; }
            if (path=="/v1/configure-readbacks") { reply(200,fJobs->ConfigureReadbacksJson()); return; }
            const QString prefix="/v1/operations/";
            if (path.startsWith(prefix)) { try { reply(200,EncodeEccControlOperation(fJobs->Operation(path.mid(prefix.size()).toStdString()))); } catch (const std::out_of_range &) { reply(404,Error("Operation not found")); } return; }
            reply(404,Error("Unknown ECC control endpoint")); return;
    } catch (const std::invalid_argument &e) { reply(400,Error(e.what())); }
      catch (const std::exception &e) { reply(503,Error(e.what())); }
}
}
