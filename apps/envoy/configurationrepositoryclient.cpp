#include "configurationrepositoryclient.h"
#include <stdexcept>
#include "configurationselection.h"
#include "configurationrepositoryclient_p.h"
#include <QFileInfo>
#include <QHostAddress>
using namespace ConfigurationRepositoryInternal;
ConfigurationRepositoryClient::ConfigurationRepositoryClient(const ConfigurationRepositoryEndpoint &endpoint,QObject *parent):QObject(parent),fEndpoint(endpoint),fHttp(new HttpClient(this)) {
    const QHostAddress address(endpoint.address);
    if (address.protocol()!=QAbstractSocket::IPv4Protocol || address.toString()!=endpoint.address || address==QHostAddress::AnyIPv4 || endpoint.port<1 || endpoint.port>65535 || endpoint.timeoutMs<100 || endpoint.timeoutMs>180000 || !QFileInfo(endpoint.credentialFile).isAbsolute()) throw std::invalid_argument("Invalid configuration repository endpoint");
    if (!endpoint.repositoryId.isEmpty()) Digest(endpoint.repositoryId);
    Headers();
}
ConfigurationRepositoryClient::~ConfigurationRepositoryClient() {fHttp->Cancel();}
QMap<QByteArray,QByteArray> ConfigurationRepositoryClient::Headers() {
    if (fCredentialFault) throw std::runtime_error("Repository client credential fault is latched");
    try {
        const QFileInfo info(fEndpoint.credentialFile);if (!info.isFile() || info.isSymLink() || info.size()<64 || info.size()>65) throw std::runtime_error("Invalid repository client credential file");
#ifdef Q_OS_UNIX
        if (info.permissions()&(QFile::ReadGroup|QFile::WriteGroup|QFile::ExeGroup|QFile::ReadOther|QFile::WriteOther|QFile::ExeOther)) throw std::runtime_error("Repository credential requires private permissions");
#endif
        auto token=Read(fEndpoint.credentialFile,65);if (token.size()==65 && token.endsWith('\n')) token.chop(1);Digest(QString::fromLatin1(token));
        if (!fCredential.isEmpty() && token!=fCredential) throw std::runtime_error("Repository credential changed");
        fCredential=token;
        return {{"Authorization","Bearer "+token},{"X-Repository-ID",fEndpoint.repositoryId.toLatin1()},{"Connection","close"}};
    } catch (...) {fCredentialFault=true;throw;}
}
QUrl ConfigurationRepositoryClient::Url(const QString &path) const {QUrl url;url.setScheme("http");url.setHost(fEndpoint.address);url.setPort(fEndpoint.port);url.setPath(path);return url;}
bool ConfigurationRepositoryClient::QueryIdentity() {
    if (fBusy) return false;
    fBusy=true;
    try {fHttp->Request(Url("/v1/identity"),{}, {},fEndpoint.timeoutMs,[this](const QByteArray &bytes,const QString &error) {
        fBusy=false;if (!error.isEmpty()) {emit IdentitySignal({},error);return;}
        try {const auto identity=Json(bytes);Digest(identity["repository_id"].toString());if (identity["kind"].toString()!="getdaq.configuration-repository" || identity["chunk_bytes"].toInt()!=32768) throw std::runtime_error("Unsupported configuration repository identity");emit IdentitySignal(identity,{});}
        catch (const std::exception &e) {emit IdentitySignal({},QString::fromUtf8(e.what()));}
    },Headers());return true;} catch (const std::exception &e) {fBusy=false;emit IdentitySignal({},QString::fromUtf8(e.what()));return false;}
}
void ConfigurationRepositoryClient::CheckIdentity(std::function<void()> next) {
    Digest(fEndpoint.repositoryId);
    fHttp->Request(Url("/v1/identity"),{}, {},fEndpoint.timeoutMs,[this,next](const QByteArray &bytes,const QString &error) {
        if (!error.isEmpty()) {Fail(error);return;}
        try {const auto identity=Json(bytes);if (identity["repository_id"].toString()!=fEndpoint.repositoryId || identity["kind"].toString()!="getdaq.configuration-repository" || identity["chunk_bytes"].toInt()!=32768) throw std::runtime_error("Configuration repository identity differs");next();}
        catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
    },Headers());
}
bool ConfigurationRepositoryClient::QueryVersions() {
    if (fBusy) return false;
    fBusy=true;
    try {CheckIdentity([this] {fHttp->Request(Url("/v1/versions"),{}, {},fEndpoint.timeoutMs,[this](const QByteArray &bytes,const QString &error) {
        fBusy=false;if (!error.isEmpty()) {emit VersionsSignal({},error);return;}
        try {const auto versions=Json(bytes);if (versions["repository_id"].toString()!=fEndpoint.repositoryId || !versions["versions"].isArray()) throw std::runtime_error("Repository listing identity differs");emit VersionsSignal(versions,{});}
        catch (const std::exception &e) {emit VersionsSignal({},QString::fromUtf8(e.what()));}
    },Headers());});return true;} catch (const std::exception &e) {fBusy=false;emit VersionsSignal({},QString::fromUtf8(e.what()));return false;}
}
bool ConfigurationRepositoryClient::QueryVersion(const QString &sha) {
    if (fBusy) return false;
    fSelectionBytes.clear();
    fBusy=true;
    try {
        Digest(sha);CheckIdentity([this,sha] {fHttp->Request(Url("/v1/versions/"+sha),{}, {},fEndpoint.timeoutMs,[this,sha](const QByteArray &bytes,const QString &error) {
            if (!error.isEmpty()) {Fail(error);return;}
            try {
                const auto receipt=Json(bytes);
                if (receipt["kind"].toString()!="getdaq.configuration-registration" || receipt["repository_id"].toString()!=fEndpoint.repositoryId || receipt["publication_sha256"].toString()!=sha ||
                    !receipt["hardware_loaded_verified"].isBool() || receipt["hardware_loaded_verified"].toBool() || receipt["manifest_file"].toString().isEmpty()) throw std::runtime_error("Repository version receipt differs");
                for (const QString &key:QStringList{"configuration_sha256","artifact_sha256","effective_sha256","snapshot_sha256"}) Digest(receipt[key].toString());
                fSelectionBytes=QByteArray::fromStdString(GetDAQ::EncodeConfigurationSelection(GetDAQ::SelectConfigurationRegistration(bytes.toStdString())));
                fBusy=false;emit FinishedSignal(receipt,{});
            } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));}
        },Headers());});return true;
    } catch (const std::exception &e) {Fail(QString::fromUtf8(e.what()));return false;}
}
void ConfigurationRepositoryClient::Fail(const QString &error) {fBusy=false;emit FinishedSignal({},error);}
void ConfigurationRepositoryClient::Cancel() {fHttp->Cancel();fBusy=false;emit FinishedSignal({},tr("Upload cancelled locally; remote objects or registration may already exist. Resume observes the same version."));}
