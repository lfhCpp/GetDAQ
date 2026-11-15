#include "eccservice.h"
#include <stdexcept>
#include "eccsuccessorplan.h"
#include "../../../common/text/utf8.h"
#include <QHostAddress>
#include <QThreadPool>
#include <fcntl.h>
namespace GetDAQ {
namespace EccServiceInternal {
std::string Error(const std::string &message) { return "{\"schema\":1,\"error\":"+EncodeJsonString(EccErrorText(message.c_str()))+"}\n"; }
std::string DecodeReceiverSourcePath(const QString &hex) {
    const QByteArray bytes=hex.toLatin1();
    if (bytes.isEmpty() || bytes.size()>256 || (bytes.size()&1) || bytes!=bytes.toLower())
        throw std::invalid_argument("Use a lowercase hexadecimal UTF-8 receiver source ID");
    for (const char c:bytes) if (!((c>='0' && c<='9') || (c>='a' && c<='f')))
        throw std::invalid_argument("Use a lowercase hexadecimal UTF-8 receiver source ID");
    const QByteArray decoded=QByteArray::fromHex(bytes);
    if (decoded.toHex()!=bytes) throw std::invalid_argument("Non-canonical receiver source ID encoding");
    const QString text=QString::fromUtf8(decoded.constData(),decoded.size());
    if (text.toUtf8()!=decoded) throw std::invalid_argument("Receiver source ID is not valid UTF-8");
    return decoded.toStdString();
}
} // namespace EccServiceInternal
EccService::EccService(const std::string &file,QObject *parent) : EccService(file,"","",parent) {}
EccService::EccService(const std::string &file,const std::string &plan,const std::string &receipt,QObject *parent) : QObject(parent),fConfiguration(file),fCredential(fConfiguration.CredentialFile()),fServer(this) {
    if (plan.empty()!=receipt.empty()) throw std::invalid_argument("Successor plan and original handoff receipt must be supplied together");
    if (!plan.empty()) {
        fSuccessor.reset(new EccSuccessorPlan(plan,fConfiguration));
        fHandoffReceipt=SupervisorIO::ReadFile(receipt,1024*1024);
    }
    connect(&fServer,&HttpServer::RequestSignal,this,&EccService::Handle);
}
EccService::~EccService() { fServer.close(); QThreadPool::globalInstance()->waitForDone(); fJobs.reset(); }
void EccService::Start() {
    fConfiguration.Check(); if (fJobs) throw std::logic_error("ECC service already started");
    if (!fServer.listen(QHostAddress(QString::fromStdString(fConfiguration.BindAddress())),quint16(fConfiguration.Port()))) throw std::runtime_error("Cannot listen on ECC control endpoint");
    const int descriptor=int(fServer.socketDescriptor()),flags=::fcntl(descriptor,F_GETFD);
    if (flags<0 || ::fcntl(descriptor,F_SETFD,flags|FD_CLOEXEC)) { fServer.close(); throw std::runtime_error("Cannot protect ECC control listener descriptor"); }
    try {
        if (fSuccessor) fSuccessor->ActivateForStartup();
        fJobs.reset(new EccControlJobs(fConfiguration,fSuccessor,fHandoffReceipt));
    } catch (...) { fServer.close(); throw; }
}
void EccService::Handle(const QString &method,const QString &path,const QByteArray &body,HttpConnection *connection) {
    const auto reply=[&](int code,const std::string &json) { connection->Reply(code,QByteArray::fromStdString(json)); };
    try {
        if(fShutdownRequested) { reply(503,EccServiceInternal::Error("ECC service shutdown is already pending")); return; }
        if (!fCredential.Authorize(connection->RequestHeader("authorization").toStdString())) { reply(401,EccServiceInternal::Error("Authentication required")); return; }
        if (method=="GET" && body.isEmpty()) { HandleGet(path,connection); return; }
        if (method!="POST") { reply(405,EccServiceInternal::Error("Use GET status/operation or POST operations")); return; }
        if (connection->RequestHeader("content-type")!="application/json") { reply(400,EccServiceInternal::Error("Use application/json")); return; }
        HandlePost(path,body,connection);
    } catch (const std::invalid_argument &e) { reply(400,EccServiceInternal::Error(e.what())); }
      catch (const std::exception &e) { reply(503,EccServiceInternal::Error(e.what())); }
}

}
