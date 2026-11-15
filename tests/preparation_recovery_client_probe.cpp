#include "eccsystemclient.h"
#include "eccclientstore.h"
#include <QCoreApplication>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QElapsedTimer>
#include <QThread>
#include <functional>
#include <vector>
#include <utility>
#include <stdexcept>
#include <iostream>
namespace {
void Require(bool value,const char *message){if(!value)throw std::runtime_error(message);}
void Wait(const std::function<bool()> &ready){QElapsedTimer timer;timer.start();while(!ready()){Require(timer.elapsed()<30000,"Qt input-fault client deadline");QCoreApplication::processEvents();QThread::msleep(10);}}
}
int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    try {
        Require(argc==3,"Pass options JSON and result JSON");QFile file(QString::fromLocal8Bit(argv[1]));Require(file.open(QIODevice::ReadOnly),"Read client options");const auto v=QJsonDocument::fromJson(file.readAll()).object();file.close();
        EccSystemClientOptions o;o.address="127.0.0.1";o.port=v.value("port").toInt();o.credentialFile=v.value("credential").toString();o.serviceId=v.value("service_id").toString();o.clientId=v.value("client_id").toString();o.clientEpoch=v.value("client_epoch").toString();o.snapshotSha256=v.value("snapshot_sha256").toString();o.timeoutMs=6000;
        file.setFileName(QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath()+"/identity.json");Require(file.open(QIODevice::ReadOnly),"Read fixture's startup identity binding");const auto bound=GetDAQ::DecodeEccControlIdentity(file.readAll().toStdString());file.close();
        Require(bound.serviceId==o.serviceId.toStdString() && bound.clientId==o.clientId.toStdString() && bound.snapshotSha256==o.snapshotSha256.toStdString(),"Fixture identity selection differs");o.inputBindingSha256=QString::fromStdString(bound.inputBindingSha256);
        const auto storeRoot=v.value("store").toString().isEmpty() ? QFileInfo(QString::fromLocal8Bit(argv[1])).absolutePath()+"/client-records" : v.value("store").toString();
        EccClientStore store(storeRoot,o);
        EccSystemClient client(o,[&](const std::string &bytes,QString *error){return store.WriteIntent(bytes,error);});
        bool identity=false,status=false,originalObserved=false,recovered=false;QString failed;
        QObject::connect(&client,&EccSystemClient::IdentitySignal,[&](bool matched,const QByteArray &,const QString &error){if(!error.isEmpty())failed=error;identity=matched;});
        QObject::connect(&client,&EccSystemClient::StatusSignal,[&](const QByteArray &body,const QString &error){if(!error.isEmpty())failed=error;status=!body.isEmpty();});
        QObject::connect(&client,&EccSystemClient::OperationSignal,[&](const QString &id,const QString &phase,const QByteArray &body,const QString &error){
            if(!error.isEmpty()){failed=error;return;}
            if(!body.isEmpty()){QString why;if(!store.Observe(body.toStdString(),&why))failed=why;}
            if(id=="prepare" && phase=="failed")originalObserved=true;
            if(id=="qt-preparation-recovery" && phase=="complete")recovered=true;
        });
        Require(client.QueryIdentity(),"Query identity");Wait([&]{return identity || !failed.isEmpty();});Require(failed.isEmpty() && identity,"Identity failed");
        Require(client.QueryStatus(),"Query held Prepare");Wait([&]{return status || !failed.isEmpty();});
        Require(failed.isEmpty() && client.CanSubmit("reconcile-preparation") && !client.CanSubmit("prepare"),"Held Prepare admission differs");
        GetDAQ::EccControlRequest original;original.operationId="prepare";original.command="prepare";original.instanceId=bound.instanceId;original.snapshotSha256=bound.snapshotSha256;original.clientId=o.clientId.toStdString();original.clientEpoch=o.clientEpoch.toStdString();QString recordError;
        Require(store.WriteIntent(GetDAQ::EncodeEccControlRequest(original),&recordError),"Record original Prepare intent without resend");
        Require(client.Observe(original),"Observe original Prepare result");Wait([&]{return originalObserved || !failed.isEmpty();});
        Require(failed.isEmpty() && originalObserved && store.NeedsReconciliation() && store.Pending().empty(),"Original Prepare hold not durable");
        for(const auto &entry:std::vector<std::pair<std::string,std::string>>{{"negative-prepare","stop-production-after-failure"},{"failure-cleanup","unprepare-after-failure"}}) {
            auto negative=original;negative.operationId=entry.first;negative.command=entry.second;
            Require(store.WriteIntent(GetDAQ::EncodeEccControlRequest(negative),&recordError),"Record original negative intent without resend");
            Require(client.Observe(negative),"Observe retained negative operation");
            Wait([&]{return !client.Busy() || !failed.isEmpty();});
            Require(failed.isEmpty() && store.Pending().empty() && store.NeedsReconciliation(),"Negative observation gate differs");
        }
        Require(client.Submit("reconcile-preparation","qt-preparation-recovery",QString(),0,"prepare"),"Submit bound Prepare recovery");
        QElapsedTimer deadline;deadline.start();
        while(!recovered && failed.isEmpty()) {
            Require(deadline.elapsed()<30000,"Recovery observation deadline");QCoreApplication::processEvents();
            if(!client.Busy()) {const auto pending=store.Pending();if(!pending.empty())client.Observe(pending.front());}QThread::msleep(15);
        }
        Require(failed.isEmpty() && recovered && !client.Pending() && store.Pending().empty() && !store.NeedsReconciliation(),"Recovery not durably observed");
        Require(!client.CanSubmit("prepare"),"Old status used immediately after recovery");status=false;
        Require(client.QueryStatus(),"Refresh verified Prepared");Wait([&]{return status || !failed.isEmpty();});
        Require(failed.isEmpty() && client.CanSubmit("prepare"),"Verified recovery did not restore preparation admission");
        QJsonObject proof;proof["passed"]=true;proof["held_before_recovery"]=true;proof["original_failed_operation_bound"]=true;proof["fresh_status_required_after_recovery"]=true;proof["production_client_and_store"]=true;
        file.setFileName(QString::fromLocal8Bit(argv[2]));Require(file.open(QIODevice::WriteOnly),"Save client proof");const auto bytes=QJsonDocument(proof).toJson();Require(file.write(bytes)==bytes.size(),"Write client proof");
        std::cout<<"PASS production Qt client/store: bound failed Prepare recovery and fresh positive admission\n";
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
