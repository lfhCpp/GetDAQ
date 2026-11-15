#include "systemconfigurationpublisher.h"
#include "systemexperimentcompiler.h"
#include "effectiveconfigurationsnapshot.h"
#include "../experiment/configurationpublication.h"
#include "../experiment/configurationsha256.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <map>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#elif defined(Q_OS_WIN)
#include <io.h>
#endif
namespace {
void Require(bool condition,const QString &message) { if (!condition) throw std::runtime_error(message.toStdString()); }
QByteArray ReadProfile(const QString &path) {
    QFile file(path); Require(file.open(QIODevice::ReadOnly),"Cannot read publication profile: "+file.errorString());
    const auto bytes=file.read(1024*1024+1); Require(file.error()==QFile::NoError && file.atEnd() && !bytes.isEmpty() && bytes.size()<=1024*1024,"Unreadable/excessive publication profile"); return bytes;
}
void SyncDirectory(const QString &directory) {
#ifdef Q_OS_UNIX
    const auto path=QFile::encodeName(directory); const int fd=::open(path.constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if (fd<0) throw std::runtime_error("Cannot open publication directory for sync");
    const int status=::fsync(fd); ::close(fd); Require(status==0,"Cannot sync publication directory; inspect any published manifest before reuse");
#else
    Q_UNUSED(directory);
#endif
}
void WriteNew(const QString &path,const QByteArray &bytes) {
    QFile file(path); Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot create publication file: "+file.errorString());
    Require(file.write(bytes)==bytes.size() && file.flush(),"Incomplete publication file: "+path);
#ifdef Q_OS_UNIX
    Require(::fsync(file.handle())==0,"Cannot sync publication file: "+path);
#elif defined(Q_OS_WIN)
    // QFile::flush only drains Qt's buffer; _commit also asks the Windows
    // CRT/filesystem to commit the file contents before continuing publication.
    Require(::_commit(file.handle())==0,"Cannot commit publication file: "+path);
#endif
    Require(file.setPermissions(QFileDevice::ReadOwner|QFileDevice::ReadUser),"Cannot make publication file read-only: "+path);
#ifdef Q_OS_UNIX
    Require(::fsync(file.handle())==0,"Cannot sync publication file permissions: "+path);
#elif defined(Q_OS_WIN)
    Require(::_commit(file.handle())==0,"Cannot commit publication file permissions: "+path);
#endif
    file.close();
}
GetDAQ::PublicationFile Reference(const std::string &name,const QByteArray &bytes) {
    GetDAQ::PublicationFile f; f.stored=name; f.bytes=static_cast<std::uint64_t>(bytes.size()); f.sha256=GetDAQ::ConfigurationSha256(bytes.toStdString()); return f;
}
}
bool SystemConfigurationPublisher::Publish(const XcfgDocument &document,const QString &profilePath,const QString &directory,QJsonObject *result,QString *error)
{
    bool created=false; QString target;
    try {
        const auto profileFile=QFileInfo(profilePath).canonicalFilePath(); Require(!profileFile.isEmpty(),"Publication profile does not exist"); const auto profile=ReadProfile(profileFile);
        QJsonObject effective; if (!EffectiveConfigurationSnapshot::Capture(document,&effective,error)) return false;
        GetDAQ::SystemExperimentArtifact artifact;
        if (!SystemExperimentCompiler::Compile(document,profile,&artifact,error)) return false;
        // 文档是本次调用的const副本，编译不修改有效树；只需重新核对磁盘来源。
        if (!document.SourcesUnchanged(error)) return false;
        Require(ReadProfile(profileFile)==profile,"Publication profile changed during compilation");
        const auto requested=QFileInfo(QDir::cleanPath(QFileInfo(directory).absoluteFilePath()));
        const auto parent=QFileInfo(requested.absolutePath()).canonicalFilePath();
        Require(!parent.isEmpty() && !requested.fileName().isEmpty() && requested.fileName()!="." && requested.fileName()!="..","Publication parent directory must already exist");
        target=QDir(parent).filePath(requested.fileName());
        Require(!QFileInfo::exists(target) && !QFileInfo(target).isSymLink(),"Publication directory already exists; choose a new version directory");
        GetDAQ::ConfigurationPublication publication; publication.rootSource=document.RootFile().toStdString(); publication.configurationSha256=artifact.configurationSha256;
        publication.effectiveSha256=effective.value("effective_sha256").toString().toStdString(); publication.snapshotSha256=effective.value("snapshot_sha256").toString().toStdString();
        const auto artifactBytes=QByteArray::fromStdString(GetDAQ::EncodeSystemExperimentArtifact(artifact)); const auto effectiveBytes=QJsonDocument(effective).toJson(QJsonDocument::Compact)+'\n';
        publication.artifact=Reference("experiment.json",artifactBytes); publication.profile=Reference("profile.json",profile); publication.effective=Reference("effective.json",effectiveBytes);
        std::map<std::string,QByteArray> sources,files; files["experiment.json"]=artifactBytes; files["profile.json"]=profile; files["effective.json"]=effectiveBytes;
        for (auto it=document.Sources().constBegin();it!=document.Sources().constEnd();++it) sources[it.key().toStdString()]=it->bytes;
        for (const auto &entry:sources) {
            GetDAQ::PublicationSource source; source.sourcePath=entry.first; source.file=Reference(GetDAQ::PublicationSourceName(publication.sources.size()),entry.second);
            files[source.file.stored]=entry.second; publication.sources.push_back(source);
        }
        const auto manifest=QByteArray::fromStdString(GetDAQ::EncodeConfigurationPublication(publication));
        GetDAQ::VerifyConfigurationPublication(publication,[&](const GetDAQ::PublicationFile &f) { return files.at(f.stored).toStdString(); });
#ifdef Q_OS_UNIX
        Require(::mkdir(QFile::encodeName(target).constData(),0700)==0,"Cannot create new publication directory");
#else
        Require(QDir(parent).mkdir(requested.fileName()),"Cannot create new publication directory");
#endif
        created=true;
        Require(QFile::setPermissions(target,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner|QFileDevice::ReadUser|QFileDevice::WriteUser|QFileDevice::ExeUser),"Cannot set publication directory permissions");
        for (const auto &entry:files) WriteNew(QDir(target).filePath(QString::fromStdString(entry.first)),entry.second);
        SyncDirectory(target); SyncDirectory(parent);
        WriteNew(QDir(target).filePath("publication.json"),manifest); SyncDirectory(target);
        QJsonObject receipt; receipt["manifest_file"]=QDir(target).filePath("publication.json"); receipt["manifest_sha256"]=QString::fromStdString(GetDAQ::ConfigurationSha256(manifest.toStdString()));
        receipt["artifact_file"]=QDir(target).filePath("experiment.json"); receipt["artifact_sha256"]=QString::fromStdString(publication.artifact.sha256);
        receipt["configuration_sha256"]=QString::fromStdString(publication.configurationSha256); receipt["effective_sha256"]=QString::fromStdString(publication.effectiveSha256); receipt["snapshot_sha256"]=QString::fromStdString(publication.snapshotSha256);
        receipt["hardware_loaded_verified"]=false; *result=receipt; error->clear(); return true;
    } catch (const std::exception &e) {
        *error=QString::fromUtf8(e.what()); if (created) *error+="; publication directory retained for inspection: "+target;
        return false;
    }
}
