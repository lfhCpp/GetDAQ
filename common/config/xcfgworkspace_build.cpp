#include "xcfgtransaction.h"
#include "xcfgworkspace.h"
#include "effectiveconfigurationsnapshot.h"

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::Budget(const QMap<QString,QByteArray> &sources,QString *error) {
    qint64 bytes = 0;
    if (sources.size() > 256) return Fail(error, "Workspace exceeds 256 source files");
    for (auto it=sources.constBegin();it!=sources.constEnd();++it) {
        if (it->size()>8*1024*1024) return Fail(error, "Source exceeds 8 MiB: "+it.key());
        bytes += it->size();
    }
    return bytes<=64*1024*1024 || Fail(error, "Workspace sources exceed 64 MiB");
}

bool XcfgWorkspace::Build(const QStringList &roots,const QMap<QString,QByteArray> &bytes,
                         QMap<QString,XcfgDocument> *documents,QMap<QString,QByteArray> *sources,QString *error,bool virtualSources) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    documents->clear();sources->clear();
    for (const auto &root:roots) {
        XcfgDocument document;document.fRootFile=root;document.fReadBytes=bytes;document.fVirtualSources=virtualSources;
        if (document.Read(root,{},error)<0) return false;
        // Validate every declaration, including templates not currently instantiated.
        for (int i=0;i<document.fNodes.size();++i) {
            const auto node=document.fNodes[i];
            if (!node.scalar) { QMap<QString,int> children;if(!document.ChildIndex(i,&children,error)) return false;continue; }
            QVector<XcfgDocument::ScalarProperties> properties;
            if (!document.DeclarationProperties(i,&properties,error)) return false;
            for (const auto &info:properties) {
                auto scalar=node;scalar.attributes.remove("range");
                if(info.rangeSource>=0) scalar.attributes["range"]=info.range;
                if(!XcfgDocument::CheckValue(scalar,scalar.value,error)) return false;
            }
        }
        QJsonObject snapshot;
        if(!EffectiveConfigurationSnapshot::CaptureLoaded(document,false,&snapshot,error)) return false;
        for(auto it=document.fSources.constBegin();it!=document.fSources.constEnd();++it) {
            if(sources->contains(it.key()) && sources->value(it.key())!=it->bytes)
                return Fail(error,"Shared source changed while opening workspace: "+it.key());
            (*sources)[it.key()]=it->bytes;
        }
        if(!Budget(*sources,error)) return false;
        (*documents)[root]=document;
    }
    error->clear();return true;
}
