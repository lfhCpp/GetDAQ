#include "configurationrepositoryclient.h"
#include "configurationrepositoryclient_p.h"
#include "configurationpublication.h"
#include "configurationselection.h"
#include "configurationsha256.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <stdexcept>

using namespace ConfigurationRepositoryInternal;

QStringList ConfigurationRepositoryClient::AddBytes(const QByteArray &bytes,const QString &file) {
    QStringList objects;
    for (int offset=0;offset<bytes.size();offset+=32768) {
        const auto data=bytes.mid(offset,32768);const auto sha=Sha(data);objects.append(sha);
        if (!fObjects.contains(sha)) {Object object;object.file=file;object.offset=offset;object.length=data.size();if (file.isEmpty()) object.bytes=data;fObjects[sha]=object;}
    }
    return objects;
}
QByteArray ConfigurationRepositoryClient::ObjectBytes(const QString &sha) const {
    const auto object=fObjects.value(sha);auto bytes=object.bytes;
    if (!object.file.isEmpty()) {QFile file(object.file);if (!file.open(QIODevice::ReadOnly) || !file.seek(object.offset)) throw std::runtime_error("Cannot reread publication object");bytes=file.read(object.length);}
    if (bytes.size()!=object.length || Sha(bytes)!=sha) throw std::runtime_error("Local publication object changed; keep original upload binding");
    return bytes;
}
bool ConfigurationRepositoryClient::Upload(const QString &manifestFile) {
    if (fBusy) return false;
    fSelectionBytes.clear();
    try {
        Digest(fEndpoint.repositoryId);fHavePlan=false;fObjects.clear();fOrder.clear();const auto file=QFileInfo(manifestFile).canonicalFilePath();if (file.isEmpty()) throw std::runtime_error("Publication manifest does not exist");
        const auto bytes=Read(file,1024*1024);const auto directory=QFileInfo(file).absolutePath();const auto publication=GetDAQ::DecodeConfigurationPublication(bytes.toStdString());
        const auto manifestObjects=AddBytes(bytes,file);QJsonArray files;
        const auto artifact=GetDAQ::VerifyConfigurationPublication(publication,[&](const GetDAQ::PublicationFile &reference) {
            const auto path=directory+'/'+QString::fromStdString(reference.stored);const auto data=Read(path,qint64(reference.bytes));
            QJsonObject entry;entry["stored"]=QString::fromStdString(reference.stored);QJsonArray objects;for (const auto &sha:AddBytes(data,path)) objects.append(sha);entry["objects"]=objects;files.append(entry);return data.toStdString();
        });
        fPublicationSha=Sha(bytes);QJsonObject plan;plan["schema_version"]=1;plan["kind"]="getdaq.configuration-upload-plan";plan["publication_sha256"]=fPublicationSha;plan["files"]=files;
        QJsonArray manifests;for (const auto &sha:manifestObjects) manifests.append(sha);plan["manifest_objects"]=manifests;
        const auto encoded=QJsonDocument(plan).toJson(QJsonDocument::Compact);if (encoded.size()>1024*1024) throw std::runtime_error("Repository plan exceeds 1 MiB");QJsonArray plans;for (const auto &sha:AddBytes(encoded)) plans.append(sha);
        fCommit=QJsonObject{{"schema_version",1},{"plan_objects",plans}};fOrder=fObjects.keys();fIndex=0;
        fExpected=QJsonObject{{"publication_sha256",fPublicationSha},{"configuration_sha256",QString::fromStdString(artifact.configurationSha256)},{"artifact_sha256",QString::fromStdString(artifact.artifactSha256)},
            {"effective_sha256",QString::fromStdString(publication.effectiveSha256)},{"snapshot_sha256",QString::fromStdString(publication.snapshotSha256)}};
        fHavePlan=true;fBusy=true;CheckIdentity([this] {ObserveVersion();});return true;
    } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));return false;}
}
bool ConfigurationRepositoryClient::Resume() {
    if (fBusy || !fHavePlan) return false;
    fBusy=true;
    try {CheckIdentity([this] {ObserveVersion();});return true;}catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));return false;}
}
void ConfigurationRepositoryClient::ObserveVersion() {
    fHttp->Request(Url("/v1/versions/"+fPublicationSha),{}, {},fEndpoint.timeoutMs,[this](const QByteArray &bytes,const QString &error) {
        if (error.isEmpty()) {Accept(bytes);return;}
        if (error!="HTTP 404" && !error.startsWith("HTTP 404:")) {Fail(error+"; upload result may be unknown, observe the same version before continuing");return;}
        try {fIndex=0;NextObject();}catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
    },Headers());
}
void ConfigurationRepositoryClient::NextObject() {
    if (fIndex>=fOrder.size()) {Commit();return;}
    const auto sha=fOrder[fIndex];emit ProgressSignal(tr("Checking configuration object %1/%2").arg(fIndex+1).arg(fOrder.size()));
    fHttp->Request(Url("/v1/objects/"+sha),{}, {},fEndpoint.timeoutMs,[this,sha](const QByteArray &bytes,const QString &error) {
        if (!error.isEmpty()) {Fail(error);return;}
        try {
            const auto result=Json(bytes);if (!result["present"].isBool()) throw std::runtime_error("Invalid repository object observation");
            if (result["present"].toBool()) {++fIndex;NextObject();return;}
            fHttp->Request(Url("/v1/objects/"+sha),ObjectBytes(sha),"application/octet-stream",fEndpoint.timeoutMs,[this](const QByteArray &reply,const QString &failure) {
                if (!failure.isEmpty()) {Fail(failure+"; observe the same object before retry");return;}
                try {
                    const auto result=Json(reply);
                    if (!result["stored"].isBool() || !result["stored"].toBool()) throw std::runtime_error("Repository did not confirm object storage");
                    ++fIndex;NextObject();
                } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
            },Headers());
        } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
    },Headers());
}
void ConfigurationRepositoryClient::Commit() {
    emit ProgressSignal(tr("Verifying and registering the uploaded configuration; hardware is unchanged."));
    fHttp->Request(Url("/v1/commit"),QJsonDocument(fCommit).toJson(QJsonDocument::Compact),"application/json",fEndpoint.timeoutMs,[this](const QByteArray &bytes,const QString &error) {if (!error.isEmpty()) {Fail(error+"; query the same version before repeating commit");return;}Accept(bytes);},Headers());
}
void ConfigurationRepositoryClient::Accept(const QByteArray &bytes) {
    try {
        const auto receipt=Json(bytes);
        if (receipt["kind"].toString()!="getdaq.configuration-registration" || receipt["repository_id"].toString()!=fEndpoint.repositoryId || !receipt["hardware_loaded_verified"].isBool() || receipt["hardware_loaded_verified"].toBool()) throw std::runtime_error("Configuration registration identity/scope differs");
        for (auto i=fExpected.constBegin();i!=fExpected.constEnd();++i) if (receipt.value(i.key())!=i.value()) throw std::runtime_error("Configuration registration byte/parameter digest differs");
        fSelectionBytes=QByteArray::fromStdString(GetDAQ::EncodeConfigurationSelection(GetDAQ::SelectConfigurationRegistration(bytes.toStdString())));
        fBusy=false;emit FinishedSignal(receipt,{});
    } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
}
