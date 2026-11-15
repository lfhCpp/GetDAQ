#include "xcfgtransaction.h"
#include "xcfgworkspace.h"

#include <QFile>
#include <QFileInfo>

namespace {
bool Fail(QString *error, const QString &message) { *error = message; return false; }
}

bool XcfgWorkspace::PreviewStructure(const QString &path,const XcfgStructureEditor::Request &request,StructurePreview *preview,QString *error) const {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(!preview)return Fail(error,"No workspace structure preview destination");
    XcfgDocument document;if(!Document(path,&document,error))return false;
    StructurePreview result;result.revision=fRevision;result.root=QFileInfo(path).canonicalFilePath();
    if(!XcfgStructureEditor::CreatePreviewImpl(document,request,&result.edit,error,false))return false;
    XcfgWorkspace candidate=*this;
    QMap<QString,QByteArray> change;change[result.edit.destinationFile]=result.edit.updatedBytes;
    if(!candidate.StageSourcesImpl(change,false,error) || !candidate.CreatePreview(&result.workspace,error))return false;
    // Import inputs may disappear from the resulting tree. Still bind every
    // input used by this preview until it is accepted into the workspace.
    for(auto it=result.edit.dependencyBytes.constBegin();it!=result.edit.dependencyBytes.constEnd();++it) {
        if(fBaseline.contains(it.key())) {if(Bytes(it.key())!=it.value())return Fail(error,"Workspace dependency differs");}
        else {QFile file(it.key());if(QFileInfo(it.key()).canonicalFilePath()!=it.key() || !file.open(QIODevice::ReadOnly) || file.read(8*1024*1024+1)!=it.value())return Fail(error,"Structural input changed during workspace review");}
    }
    if(!CheckSources(error))return false;
    *preview=result;return true;
}

bool XcfgWorkspace::StageStructure(const StructurePreview &preview,QString *error) {
    XcfgIoTransaction ioTransaction(error);if(!ioTransaction.IsLocked())return false;

    if(preview.revision!=fRevision)return Fail(error,"Workspace changed; review structure again");
    StructurePreview current;if(!PreviewStructure(preview.root,preview.edit.request,&current,error))return false;
    const auto &a=current.edit,&b=preview.edit;
    if(a.destinationFile!=b.destinationFile || a.sourcePath!=b.sourcePath || a.originalBytes!=b.originalBytes || a.updatedBytes!=b.updatedBytes || a.beforeSnapshot!=b.beforeSnapshot || a.afterSnapshot!=b.afterSnapshot || a.report!=b.report || a.dependencyBytes!=b.dependencyBytes || a.createdPaths!=b.createdPaths || a.localizedPaths!=b.localizedPaths || current.workspace.writes!=preview.workspace.writes || current.workspace.reports!=preview.workspace.reports || current.workspace.revision!=preview.workspace.revision)
        return Fail(error,"Workspace structural review differs; inspect a new preview");
    QMap<QString,QByteArray> change;change[a.destinationFile]=a.updatedBytes;
    return StageSources(change,error);
}
