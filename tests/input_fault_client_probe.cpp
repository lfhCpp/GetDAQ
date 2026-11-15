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
        EccClientStore store(v.value("store").toString(),o);
        EccSystemClient client(o,[&](const std::string &bytes,QString *error){return store.WriteIntent(bytes,error);});
        bool identity=false,status=false,positive=false,negative=false;QString failed;
        QObject::connect(&client,&EccSystemClient::IdentitySignal,[&](bool matched,const QByteArray &,const QString &error){if(!error.isEmpty())failed=error;identity=matched;});
        QObject::connect(&client,&EccSystemClient::StatusSignal,[&](const QByteArray &body,const QString &error){if(!error.isEmpty())failed=error;status=!body.isEmpty();});
        QObject::connect(&client,&EccSystemClient::OperationSignal,[&](const QString &id,const QString &phase,const QByteArray &body,const QString &error){
            if(!error.isEmpty()){failed=error;return;}
            if(!body.isEmpty()){QString why;if(!store.Observe(body.toStdString(),&why))failed=why;}
            if(id=="qt-input-positive" && phase=="failed")positive=true;
            if(id=="qt-input-negative" && phase=="complete")negative=true;
        });
        Require(client.QueryIdentity(),"Query client identity");Wait([&]{return identity || !failed.isEmpty();});Require(failed.isEmpty() && identity,"Client identity failed");
        Require(client.QueryStatus(),"Query initial status");Wait([&]{return status || !failed.isEmpty();});Require(failed.isEmpty() && client.CanSubmit("stop"),"Ordinary stop unavailable before source fault detection");
        Require(client.Submit("stop","qt-input-positive"),"Submit tracked ordinary stop");Wait([&]{return positive || !failed.isEmpty();});Require(failed.isEmpty() && positive,"Bound ordinary stop did not return durable failure");
        Require(!client.Pending() && store.Pending().empty() && client.CanSubmit("stop-production-after-failure"),"Rejected ordinary intent blocked explicit negative stop");
        Require(client.Submit("stop-production-after-failure","qt-input-negative"),"Submit retained production stop");
        while(!negative && failed.isEmpty()) {
            QCoreApplication::processEvents();
            if(!client.Busy()) {const auto pending=store.Pending();if(!pending.empty())client.Observe(pending.front());}
            QThread::msleep(15);
            static unsigned int attempts=0;Require(++attempts<2000,"Negative operation deadline");
        }
        Require(failed.isEmpty() && negative && store.Pending().empty() && !client.CanSubmit("start"),"Negative stop cleared positive gate or left intent pending");
        QJsonObject proof;proof["passed"]=true;proof["ordinary_intent_terminal_failed"]=true;proof["negative_after_failed_intent"]=true;proof["positive_gate_preserved"]=true;proof["production_client_and_store"]=true;
        file.setFileName(QString::fromLocal8Bit(argv[2]));Require(file.open(QIODevice::WriteOnly),"Save client proof");const auto bytes=QJsonDocument(proof).toJson();Require(file.write(bytes)==bytes.size(),"Write client proof");
        std::cout<<"PASS production Qt client/store: durable failed ordinary intent then retained negative stop\n";
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
