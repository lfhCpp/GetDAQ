#include "xcfgtransaction.h"
#include "xcfgworkspacecopy.h"
#include "xcfgstructureeditor.h"
#include "effectiveconfigurationsnapshot.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTemporaryDir>
#include <algorithm>
#include <functional>
#include <stdexcept>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {
const qint64 MaxBytes=64*1024*1024;
void Require(bool condition,const QString &message) {if(!condition)throw std::runtime_error(message.toStdString());}
QString Hash(const QByteArray &bytes) {return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
void WriteNew(const QString &path,const QByteArray &bytes)
{
    QFile file(path);Require(file.open(QIODevice::WriteOnly|QIODevice::NewOnly),"Cannot create workspace file: "+file.errorString());
    Require(file.write(bytes)==bytes.size() && file.flush(),"Workspace file write failed: "+path);
    Require(file.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ReadUser|QFileDevice::WriteUser),"Cannot set workspace file permissions: "+path);
#ifdef Q_OS_UNIX
    Require(::fsync(file.handle())==0,"Cannot synchronize workspace file: "+path);
#endif
    file.close();
}
void SyncDirectory(const QString &path)
{
#ifdef Q_OS_UNIX
    const int fd=::open(QFile::encodeName(path).constData(),O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    Require(fd>=0,"Cannot open workspace directory for synchronization");const int result=::fsync(fd);::close(fd);
    Require(result==0,"Cannot synchronize workspace directory: "+path);
#else
    Q_UNUSED(path);
#endif
}
bool Inspect(const QMap<QString,QByteArray> &files,QString *effective,QString *error)
{
    QTemporaryDir temporary;Require(temporary.isValid(),"Cannot create workspace preview validation directory");
    Require(QDir(temporary.path()).mkdir("sources"),"Cannot create workspace preview sources directory");
    for(auto it=files.constBegin();it!=files.constEnd();++it)WriteNew(QDir(temporary.path()).filePath(it.key()),it.value());
    XcfgDocument copied;QJsonObject snapshot;
    if(!copied.Load(temporary.filePath("main.xcfg"),error) || !EffectiveConfigurationSnapshot::Capture(copied,&snapshot,error))return false;
    Require(copied.Sources().size()==files.size(),"Workspace copy has unreferenced or missing source files");
    for(auto it=copied.Sources().constBegin();it!=copied.Sources().constEnd();++it) {
        const auto relative=QDir(temporary.path()).relativeFilePath(it.key());
        Require(files.contains(relative) && files.value(relative)==it->bytes,"Workspace reference escaped the copied file set");
    }
    *effective=snapshot.value("effective_sha256").toString();return true;
}
}

bool XcfgWorkspaceCopy::CreatePreview(const XcfgDocument &document,const Request &request,Preview *preview,QString *error)
{
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    try {
        Require(preview && (request.sharing==PreserveSharedFiles || request.sharing==IsolateEachReference),"Unknown workspace sharing mode");
        Require(!request.directory.isEmpty() && !request.directory.contains(QChar(0)) && !request.directory.contains("://"),"Choose a new local workspace directory");
        const QFileInfo target(QDir::cleanPath(QFileInfo(request.directory).absoluteFilePath()));
        const auto parent=QFileInfo(target.absolutePath()).canonicalFilePath();
        Require(!parent.isEmpty() && !target.fileName().isEmpty(),"Workspace parent directory must exist");
        const auto directory=QDir(parent).filePath(target.fileName());
        Require(!QFileInfo::exists(directory) && !QFileInfo(directory).isSymLink(),"Workspace directory already exists; choose a new name");
        QJsonObject before;if(!EffectiveConfigurationSnapshot::Capture(document,&before,error))return false;
        Preview result;result.request=request;result.request.directory=directory;result.rootFile=document.RootFile();
        QMap<QString,QString> storedSources;QMap<QString,QJsonObject> records;QJsonArray edges;int count=0; qint64 bytes=0;
        QMap<QString,QVector<int>> includeNodes;
        for(int i=0;i<document.Nodes().size();++i)if(document.Nodes()[i].includedRoot>=0)includeNodes[document.Nodes()[i].file].append(i);
        const auto anchor=QDir(QDir::rootPath()).filePath("getdaq-workspace-reference");
        std::function<QString(const QString &,int)> copy;
        copy=[&](const QString &sourceFile,int depth) -> QString {
            Require(depth<32,"Workspace include depth exceeds 32");
            if(request.sharing==PreserveSharedFiles && storedSources.contains(sourceFile))return storedSources.value(sourceFile);
            Require(count<256,"Workspace expansion exceeds 256 source files");
            const int number=count++;const auto stored=number==0 ? QString("main.xcfg") : QString("sources/source-%1.xcfg").arg(number,6,10,QChar('0'));
            storedSources[sourceFile]=stored;const auto source=document.Sources().value(sourceFile);QString text=source.text;
            struct Patch {int at,size;QString replacement;};QVector<Patch> patches;
            for(int index:includeNodes.value(sourceFile)) {
                const auto &node=document.Nodes()[index];
                Require(edges.size()<10000,"Workspace include edge budget exceeded");
                const auto included=document.Nodes()[node.includedRoot].file;const auto child=copy(included,depth+1);
                const auto relative=QDir(QDir(anchor).filePath(QFileInfo(stored).path())).relativeFilePath(QDir(anchor).filePath(child));
                const auto opening=source.text.mid(node.tagStart,node.contentStart-node.tagStart);
                patches.append({node.tagStart,opening.size(),XcfgStructureEditor::RewriteOpeningAttribute(opening,"include",relative)});
                Require(edges.size()<10000,"Workspace include edge budget exceeded");
                edges.append(QJsonObject{{"from",stored},{"to",child},{"declaration_path",node.path},{"line",node.line}});
            }
            std::sort(patches.begin(),patches.end(),[](const Patch &a,const Patch &b){return a.at>b.at;});
            for(const auto &patch:patches)text.replace(patch.at,patch.size,patch.replacement);
            const auto output=text.toUtf8();Require(output.size()<=8*1024*1024 && bytes<=MaxBytes-output.size(),"Workspace expansion exceeds 64 MiB or per-file 8 MiB");
            bytes+=output.size();result.files[stored]=output;
            records[stored]=QJsonObject{{"stored",stored},{"source_file",sourceFile},{"source_sha256",Hash(source.bytes)},
                                       {"output_sha256",Hash(output)},{"output_bytes",QString::number(output.size())}};
            return stored;
        };
        Require(copy(document.RootFile(),0)=="main.xcfg","Invalid workspace root mapping");
        if(!Inspect(result.files,&result.effectiveSha256,error))return false;
        Require(result.effectiveSha256==before.value("effective_sha256").toString(),"Workspace copy changes effective objects/values/types/units/ranges");
        if(!document.SourcesUnchanged(error))return false;
        for(auto it=document.Sources().constBegin();it!=document.Sources().constEnd();++it)result.sourceBytes[it.key()]=it->bytes;
        QJsonArray files;for(auto it=records.constBegin();it!=records.constEnd();++it)files.append(it.value());
        result.inventory=QJsonObject{{"kind","getdaq.editable-configuration-workspace"},{"schema_version",1},{"complete",true},
            {"sharing",request.sharing==PreserveSharedFiles ? "shared" : "isolated"},{"root_file","main.xcfg"},{"original_root_file",document.RootFile()},
            {"initial_directory",directory},
            {"initial_effective_sha256",result.effectiveSha256},{"files",files},{"include_edges",edges},{"hardware_loaded_verified",false}};
        Require(QJsonDocument(result.inventory).toJson(QJsonDocument::Compact).size()<=16*1024*1024,"Workspace inventory exceeds 16 MiB");
        *preview=result;error->clear();return true;
    }catch(const std::exception &failure){*error=QString::fromUtf8(failure.what());return false;}
}

bool XcfgWorkspaceCopy::Save(const XcfgDocument &document,const Preview &preview,QJsonObject *receipt,QString *error)
{
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    bool created=false;const auto target=preview.request.directory;
    try {
        Require(receipt && document.RootFile()==preview.rootFile,"Workspace source document changed; review a new preview");
        Preview current;if(!CreatePreview(document,preview.request,&current,error))return false;
        Require(current.sourceBytes==preview.sourceBytes && current.files==preview.files && current.inventory==preview.inventory && current.effectiveSha256==preview.effectiveSha256,
                "Workspace preview changed; review a new preview");
        if(!document.SourcesUnchanged(error))return false;
#ifdef Q_OS_UNIX
        Require(::mkdir(QFile::encodeName(target).constData(),0700)==0,"Cannot create new workspace directory");
#else
        Require(QDir(QFileInfo(target).absolutePath()).mkdir(QFileInfo(target).fileName()),"Cannot create new workspace directory");
#endif
        created=true;
        Require(QFile::setPermissions(target,QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner|QFileDevice::ReadUser|QFileDevice::WriteUser|QFileDevice::ExeUser),"Cannot set workspace directory permissions");
        Require(QDir(target).mkdir("sources"),"Cannot create workspace sources directory");
        Require(QFile::setPermissions(QDir(target).filePath("sources"),QFileDevice::ReadOwner|QFileDevice::WriteOwner|QFileDevice::ExeOwner|QFileDevice::ReadUser|QFileDevice::WriteUser|QFileDevice::ExeUser),"Cannot set workspace source directory permissions");
        WriteNew(QDir(target).filePath("workspace.incomplete"),QByteArray("Workspace copy is not verified until workspace.json exists.\n"));
        for(auto it=current.files.constBegin();it!=current.files.constEnd();++it)WriteNew(QDir(target).filePath(it.key()),it.value());
        XcfgDocument saved;QJsonObject snapshot;
        if(!saved.Load(QDir(target).filePath("main.xcfg"),error) || !EffectiveConfigurationSnapshot::Capture(saved,&snapshot,error))throw std::runtime_error(error->toStdString());
        Require(snapshot.value("effective_sha256").toString()==current.effectiveSha256 && saved.Sources().size()==current.files.size(),"Saved workspace semantic/file count verification failed");
        for(auto it=saved.Sources().constBegin();it!=saved.Sources().constEnd();++it) {
            const auto relative=QDir(target).relativeFilePath(it.key());Require(current.files.contains(relative) && current.files.value(relative)==it->bytes,"Saved workspace bytes or reference containment differ");
        }
        if(!document.SourcesUnchanged(error))throw std::runtime_error(error->toStdString());
        SyncDirectory(QDir(target).filePath("sources"));SyncDirectory(target);SyncDirectory(QFileInfo(target).absolutePath());
        const auto manifest=QJsonDocument(current.inventory).toJson(QJsonDocument::Indented);WriteNew(QDir(target).filePath("workspace.json"),manifest);
        SyncDirectory(target);Require(QFile::remove(QDir(target).filePath("workspace.incomplete")),"Verified workspace written but incomplete marker cleanup failed");SyncDirectory(target);
        *receipt=QJsonObject{{"directory",target},{"root_file",QDir(target).filePath("main.xcfg")},{"inventory_file",QDir(target).filePath("workspace.json")},
            {"inventory_sha256",Hash(manifest)},{"effective_sha256",current.effectiveSha256},{"sharing",current.inventory.value("sharing")},
            {"file_count",current.files.size()},{"include_edge_count",current.inventory.value("include_edges").toArray().size()},{"hardware_loaded_verified",false}};
        error->clear();return true;
    }catch(const std::exception &failure){*error=QString::fromUtf8(failure.what());if(created)*error+="; new workspace retained for inspection: "+target;return false;}
}
