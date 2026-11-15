#ifndef GETDAQ_XCFGPENDING_H
#define GETDAQ_XCFGPENDING_H
#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
// Header-only so existing standalone configuration readers need no new linker dependency.
inline bool XcfgNoPendingSave(const QString &source,QString *error) {
    const auto digest=QCryptographicHash::hash(source.toUtf8(),QCryptographicHash::Sha256).toHex();
    const auto marker=QFileInfo(source).absolutePath()+"/.getdaq-workspace-save-"+QString::fromLatin1(digest)+".json";
    if(!QFileInfo::exists(marker))return true;
    const auto read=[](const QString &path,qint64 maximum,QByteArray *bytes){const QFileInfo info(path);if(!info.isFile() || info.isSymLink() || info.size()>maximum)return false;QFile file(path);if(!file.open(QIODevice::ReadOnly))return false;*bytes=file.read(maximum+1);return bytes->size()<=maximum && file.error()==QFile::NoError;};
    QByteArray bytes;if(!read(marker,8192,&bytes)){*error="Invalid pending configuration marker: "+marker;return false;}
    QJsonParseError parse;const auto document=QJsonDocument::fromJson(bytes,&parse);const auto value=document.object();
    if(parse.error!=QJsonParseError::NoError || !document.isObject() || value.size()!=5 || value["schema"].toDouble()!=1 || value["kind"].toString()!="getdaq.workspace-save-marker" || value["source"].toString()!=source || !value["directory"].isString() || !value["manifest_sha256"].isString()){*error="Pending configuration marker differs: "+marker;return false;}
    const auto directory=value["directory"].toString(),key=value["manifest_sha256"].toString();QByteArray manifest;
    if(!read(directory+"/manifest.json",2*1024*1024,&manifest) || QString::fromLatin1(QCryptographicHash::hash(manifest,QCryptographicHash::Sha256).toHex())!=key){*error="Pending configuration manifest differs: "+directory;return false;}
    QByteArray complete;
    if(read(directory+"/complete.json",4096,&complete)) {
        const auto doneDocument=QJsonDocument::fromJson(complete,&parse);const auto done=doneDocument.object();const auto outcome=done["outcome"].toString();
        if(parse.error==QJsonParseError::NoError && doneDocument.isObject() && done.size()==4 && done["schema"].toDouble()==1 && done["kind"].toString()=="getdaq.workspace-save-complete" && done["manifest_sha256"].toString()==key && (outcome=="original" || outcome=="candidate"))return true;
    }
    *error="Source has an unfinished Save All; inspect recovery: "+directory;return false;
}
#endif
