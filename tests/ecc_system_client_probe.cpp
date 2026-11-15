#include "eccsystemclient.h"
#include <QCoreApplication>
#include <QSaveFile>
#include <QFile>
#include <QFileInfo>
#include <QTimer>
#include <QDebug>
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        const bool ownershipOnly=argc==11 && QString(argv[10])=="--ownership-only";
        const bool readbacksOnly=argc==11 && QString(argv[10])=="--readbacks-only";
        if (argc!=10 && !ownershipOnly && !readbacksOnly) throw std::runtime_error("Pass address port credential service client snapshot input intent-file operation-id [--ownership-only|--readbacks-only]");
        EccSystemClientOptions options; options.address=argv[1]; options.port=QString(argv[2]).toInt(); options.credentialFile=argv[3]; options.serviceId=argv[4]; options.clientId=argv[5];
        options.snapshotSha256=argv[6]; options.inputBindingSha256=argv[7]; options.clientEpoch=QString(64,'a'); options.timeoutMs=2000;
        const QString path=argv[8],id=argv[9]; std::string saved; bool finished=false;
        EccSystemClient client(options,[&](const std::string &bytes,QString *) {
            if (QFileInfo::exists(path)) return false;
            QSaveFile file(path); if (!file.open(QIODevice::WriteOnly) || file.write(bytes.data(),qint64(bytes.size()))!=qint64(bytes.size()) || !file.commit()) return false;
            saved=bytes; return true;
        });
        QTimer query,deadline; query.setInterval(30); deadline.setSingleShot(true);
        QObject::connect(&query,&QTimer::timeout,&app,[&] { if (!saved.empty()) client.Observe(GetDAQ::DecodeEccControlRequest(saved)); });
        QObject::connect(&deadline,&QTimer::timeout,&app,[&] { qCritical()<<"FAIL Qt/actual ECC observation deadline; preserve intent"; app.exit(1); });
        QObject::connect(&client,&EccSystemClient::IdentitySignal,&app,[&](bool matched,const QByteArray &,const QString &error) {
            if (!matched || !error.isEmpty()) { qCritical()<<"FAIL Qt ECC identity"<<error; app.exit(1); return; }
            if(readbacksOnly){if(!client.QueryConfigureReadbacks()){qCritical()<<"FAIL Qt readback query";app.exit(1);}return;}
            if (!client.QueryStatus()) { qCritical()<<"FAIL Qt ECC status query"; app.exit(1); }
        });
        QObject::connect(&client,&EccSystemClient::StatusSignal,&app,[&](const QByteArray &,const QString &error) {
            if (!error.isEmpty()) { qCritical()<<"FAIL Qt ECC status"<<error; app.exit(1); return; }
            if (ownershipOnly) {
                if (!client.QueryOwnership()) {qCritical()<<"FAIL Qt ownership query";app.exit(1);}
            } else if (!client.Submit("start",id)) { qCritical()<<"FAIL Qt ECC intent/submit"; app.exit(1); }
        });
        QObject::connect(&client,&EccSystemClient::OwnershipSignal,&app,[&](const QByteArray &body,const QString &error) {
            if (!ownershipOnly) return;
            if (!error.isEmpty()) {qCritical()<<"FAIL Qt ownership"<<error;app.exit(1);return;}
            try {
                const auto ownership=GetDAQ::DecodeEccControlOwnership(body.toStdString());
                if (!ownership.enabled || !ownership.heldAtLastCheck || ownership.faultLatched || !saved.empty() || client.Pending())
                    throw std::runtime_error("Read-only ownership probe has invalid state or submitted intent");
                QFile output(argv[8]);
                if (!output.open(QIODevice::WriteOnly|QIODevice::NewOnly) || output.write(body)!=body.size() || !output.flush() || !output.setPermissions(QFile::ReadOwner))
                    throw std::runtime_error("Cannot save fresh ownership observation");
                output.close();finished=true;deadline.stop();
                qInfo()<<"PASS Qt client -> actual ECC ownership: bound identity, cached same-host reservation, no intent or POST";app.exit(0);
            }catch (const std::exception &e) {qCritical()<<e.what();app.exit(1);}
        });
        QObject::connect(&client,&EccSystemClient::ConfigureReadbacksSignal,&app,[&](const QByteArray &body,const QString &error) {
            if(!readbacksOnly)return;
            if(!error.isEmpty()){qCritical()<<"FAIL Qt readbacks"<<error;app.exit(1);return;}
            try {
                GetDAQ::DecodeEccControlConfigureReadbacks(body.toStdString());
                if(!saved.empty() || client.Pending())throw std::runtime_error("Readback probe submitted control intent");
                QFile output(argv[8]);
                if(!output.open(QIODevice::WriteOnly|QIODevice::NewOnly) || output.write(body)!=body.size() || !output.flush() || !output.setPermissions(QFile::ReadOwner))
                    throw std::runtime_error("Cannot save fresh readback response");
                output.close();finished=true;deadline.stop();qInfo()<<"PASS Qt client -> actual ECC Configure readbacks: fixed binding and historical scope, no intent or POST";app.exit(0);
            }catch(const std::exception &e){qCritical()<<e.what();app.exit(1);}
        });
        QObject::connect(&client,&EccSystemClient::OperationSignal,&app,[&](const QString &,const QString &phase,const QByteArray &body,const QString &error) {
            if (!error.isEmpty() || phase=="unknown" || phase=="not-sent") { qCritical()<<"FAIL Qt ECC result"<<phase<<error; app.exit(1); return; }
            if (phase=="accepted" || phase=="running") { query.start(); return; }
            query.stop(); deadline.stop(); const auto operation=GetDAQ::DecodeEccControlOperation(body.toStdString()); const auto status=GetDAQ::DecodeEccControlControllerStatus(operation.resultJson);
            // 实际服务的Idle->Start必须失败，且原生硬件流程未开始；不能伪造Running。
            if (phase!="failed" || status.state!=1 || status.outcomeUnknown || status.needsReconciliation || client.Pending()) { qCritical()<<"FAIL illegal Start fabricated hardware transition"; app.exit(1); return; }
            finished=true; qInfo()<<"PASS Qt client -> actual ECC service: explicit scope/auth, intent-before-POST, same-ID observation; illegal Idle Start rejected, no hardware/full acquisition claim"; app.exit(0);
        });
        deadline.start(10000); if (!client.QueryIdentity()) throw std::runtime_error("Cannot initiate ECC identity query");
        const auto result=app.exec(); return finished ? result : 1;
    } catch (const std::exception &e) { qCritical().noquote()<<e.what(); return 1; }
}
