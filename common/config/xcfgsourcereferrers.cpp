#include "xcfgsourcemigration.h"
#include "xcfgsavejournal.h"
#include "xcfgtransaction.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUrl>
#include <QXmlStreamReader>
#include <algorithm>
#include <stdexcept>

namespace {
void Require(bool okay,const QString &message) {
    if(!okay)throw std::runtime_error(message.toStdString());
}
bool IsWithin(const QString &root,const QString &path) {
    const auto relative=QDir(root).relativeFilePath(path);
    return relative!=".." && !relative.startsWith("../") && !QFileInfo(relative).isAbsolute();
}
QByteArray Read(const QString &path,qint64 maximum) {
    const QFileInfo info(path);
    Require(info.isFile() && !info.isSymLink() && info.size()>=0 && info.size()<=maximum,
            "Invalid migration file: "+path);
    QFile file(path);
    Require(file.open(QIODevice::ReadOnly),"Cannot read migration file: "+path);
    const auto bytes=file.read(maximum+1);
    Require(file.error()==QFile::NoError && bytes.size()<=maximum,
            "Migration read failed or exceeds budget: "+path);
    return bytes;
}

}

bool XcfgSourceMigration::DiscoverReferrers(const QString &directory,const QString &source,QJsonObject *report,QString *error) {
    XcfgIoTransaction transaction(error);if(!transaction.IsLocked())return false;
    try {
        Require(report && error,"Missing path-referrer report/error destination");
        const QFileInfo rootInfo(directory),sourceInfo(source);
        const auto root=rootInfo.canonicalFilePath(),target=sourceInfo.canonicalFilePath();
        Require(rootInfo.isDir() && !rootInfo.isSymLink() && !root.isEmpty(),"Choose an existing regular project directory");
        Require(sourceInfo.isFile() && !sourceInfo.isSymLink() && !target.isEmpty(),"Choose an existing regular source file");

        constexpr int maxEntries=100000,maxFiles=4096,maxXmlTokens=100000,maxXmlDepth=128;
        constexpr qint64 maxFileBytes=8LL*1024*1024,maxTotalBytes=256LL*1024*1024;
        bool complete=true;int entries=0,issueCount=0; qint64 totalBytes=0;
        QStringList issues,files;QMap<QString,QByteArray> contents;
        auto issue=[&](const QString &message,const QString &path) {
            complete=false;
            ++issueCount;
            if(issues.size()<256)issues.append(message+(path.isEmpty()?QString():QStringLiteral(": ")+path));
        };
        QStringList pending{root};
        while(!pending.isEmpty()) {
            const auto current=pending.takeLast();
            QDir dir(current);
            const auto rows=dir.entryInfoList(QDir::AllEntries|QDir::Hidden|QDir::System|QDir::NoDotAndDotDot,QDir::Name);
            for(const auto &info:rows) {
                if(++entries>maxEntries){issue("directory entry scan limit exceeded",root);pending.clear();break;}
                if(info.isSymLink()){issue("symbolic link skipped",info.absoluteFilePath());continue;}
                if(info.isDir()){pending.append(info.absoluteFilePath());continue;}
                if(!info.isFile())continue;
                const auto suffix=info.suffix().toLower();
                if(suffix!="xcfg" && suffix!="xml")continue;
                const auto path=info.canonicalFilePath();
                if(path.isEmpty() || !IsWithin(root,path)){issue("configuration path escaped the selected directory",info.absoluteFilePath());continue;}
                if(contents.contains(path))continue;
                if(files.size()>=maxFiles){issue("configuration file scan limit exceeded",path);continue;}
                QString pendingSave;
                if(!XcfgSaveJournal::Pending(path,&pendingSave,error))return false;
                if(!pendingSave.isEmpty()){issue("configuration has an unfinished save record",path);continue;}
                QFileInfo beforeRead(path);beforeRead.refresh();
                if(beforeRead.size()<0 || beforeRead.size()>maxFileBytes || totalBytes>maxTotalBytes-beforeRead.size()) {
                    issue("configuration byte scan limit exceeded",path);continue;
                }
                QByteArray bytes;
                try {bytes=Read(path,maxFileBytes);} catch(const std::exception &) {issue("configuration could not be read",path);continue;}
                QFileInfo afterRead(path);afterRead.refresh();
                if(afterRead.isSymLink() || afterRead.canonicalFilePath()!=path || afterRead.size()!=beforeRead.size() || afterRead.lastModified()!=beforeRead.lastModified()) {
                    issue("configuration changed while being scanned",path);continue;
                }
                if(totalBytes>maxTotalBytes-bytes.size()){issue("configuration byte scan limit exceeded",path);continue;}
                totalBytes+=bytes.size();files.append(path);contents[path]=bytes;
            }
            if(entries>maxEntries)break;
        }

        QMap<QString,QStringList> edges; // containing file -> included files
        QMap<QString,QStringList> incoming;
        for(auto it=contents.constBegin();it!=contents.constEnd();++it) {
            QXmlStreamReader xml(it.value());int depth=0,tokens=0;bool malformed=false;
            while(!xml.atEnd()) {
                xml.readNext();
                if(++tokens>maxXmlTokens){issue("XML token scan limit exceeded",it.key());malformed=true;break;}
                if(xml.tokenType()==QXmlStreamReader::DTD){issue("DTD is not followed during reference discovery",it.key());malformed=true;break;}
                if(xml.tokenType()==QXmlStreamReader::StartElement) {
                    if(++depth>maxXmlDepth){issue("XML nesting scan limit exceeded",it.key());malformed=true;break;}
                    const bool hasInclude=xml.attributes().hasAttribute(QLatin1String("include"));
                    if(!hasInclude)continue;
                    const auto include=xml.attributes().value(QLatin1String("include")).toString();
                    if(include.isEmpty()){issue("empty include attribute cannot be resolved",it.key());continue;}
                    QString literal=include,path;
                    if(literal.startsWith(QLatin1String("file:"))) {
                        const QUrl url(literal);
                        if(!url.isLocalFile()){issue("non-local file URL cannot be inspected",it.key());continue;}
                        literal=url.toLocalFile();
                    } else if(literal.contains(QLatin1String("://"))) {
                        issue("non-local include URL cannot be inspected",it.key());continue;
                    }
                    const auto candidate=QDir::cleanPath(QFileInfo(QDir(QFileInfo(it.key()).absolutePath()).absoluteFilePath(literal)).absoluteFilePath());
                    const QFileInfo includeInfo(candidate);
                    if(includeInfo.isSymLink()){issue("symbolic-link include cannot be inspected",candidate);continue;}
                    path=includeInfo.canonicalFilePath();
                    if(path.isEmpty() || !includeInfo.isFile()){issue("include target is missing or not a regular file",candidate);continue;}
                    if(!IsWithin(root,path)){issue("include target is outside the selected directory",path);continue;}
                    if(!contents.contains(path)){
                        // The inventory intentionally covers the configuration formats
                        // used by GetDAQ; unfamiliar extensions are not proof of absence.
                        issue("include target was not in the .xcfg/.xml inventory",path);continue;
                    }
                    if(!edges[it.key()].contains(path))edges[it.key()].append(path);
                    if(!incoming[path].contains(it.key()))incoming[path].append(it.key());
                } else if(xml.tokenType()==QXmlStreamReader::EndElement) {
                    if(--depth<0){issue("XML element nesting is invalid",it.key());malformed=true;break;}
                }
            }
            if(!malformed && xml.hasError())issue("XML parse failed: "+xml.errorString(),it.key());
        }
        QMap<QString,int> colors;bool cycleFound=false;
        for(const auto &start:contents.keys()) {
            if(colors.value(start,0)!=0)continue;
            QVector<QPair<QString,bool>> stack;stack.append(qMakePair(start,false));
            while(!stack.isEmpty()) {
                const auto item=stack.takeLast();
                if(item.second){colors[item.first]=2;continue;}
                if(colors.value(item.first,0)==2)continue;
                if(colors.value(item.first,0)==1){cycleFound=true;continue;}
                colors[item.first]=1;stack.append(qMakePair(item.first,true));
                for(const auto &child:edges.value(item.first)) {
                    if(colors.value(child,0)==1){cycleFound=true;continue;}
                    if(colors.value(child,0)==0)stack.append(qMakePair(child,false));
                }
            }
        }
        if(cycleFound)issue("include graph contains a cycle",root);
        if(!contents.contains(target))issue("selected source is outside the scanned XML inventory",target);

        QSet<QString> affected;QList<QString> queue;
        if(contents.contains(target)){affected.insert(target);queue.append(target);}
        while(!queue.isEmpty()) {
            const auto current=queue.takeFirst();
            for(const auto &parent:incoming.value(current))if(!affected.contains(parent)){affected.insert(parent);queue.append(parent);}
        }
        QStringList affectedFiles=affected.values();affectedFiles.sort();
        QSet<QString> rootSet;
        for(const auto &path:affectedFiles) {
            bool hasAffectedParent=false;
            for(const auto &parent:incoming.value(path))if(affected.contains(parent)){hasAffectedParent=true;break;}
            if(!hasAffectedParent)rootSet.insert(path);
        }
        QStringList candidateRoots=rootSet.values();candidateRoots.sort();
        if(!affected.isEmpty() && candidateRoots.isEmpty())issue("affected include graph has no top-level root (possible cycle)",target);
        QJsonArray issueRows,referrers,rootRows,scannedRows;
        for(const auto &item:issues)issueRows.append(item);
        for(const auto &path:affectedFiles)if(path!=target)referrers.append(path);
        for(const auto &path:candidateRoots)rootRows.append(path);
        files.sort();for(const auto &path:files)scannedRows.append(path);
        *report=QJsonObject{{"kind","getdaq.configuration-path-referrer-discovery"},
            {"scan_directory",root},{"source",target},{"complete",complete},
            {"inventory_scope","regular .xcfg and .xml files beneath the selected directory; this cannot prove absence of references stored in other formats or outside this directory"},
            {"files_scanned",files.size()},{"bytes_scanned",double(totalBytes)},
            {"issues_omitted",qMax(0,issueCount-issues.size())},{"referrers",referrers},{"candidate_roots",rootRows},
            {"scanned_configuration_files",scannedRows},{"issues",issueRows},
            {"read_only",true},{"disk_written",false}};
        if(error)error->clear();
        return true;
    } catch(const std::exception &e) {if(error)*error=QString::fromUtf8(e.what());return false;}
}
