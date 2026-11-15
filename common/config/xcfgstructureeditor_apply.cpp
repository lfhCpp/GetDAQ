#include "xcfgtransaction.h"
#include "xcfgstructureeditor.h"
#include "xcfgstructureeditor_p.h"
#include "effectiveconfigurationsnapshot.h"
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

using namespace XcfgStructureEditorInternal;

bool XcfgStructureEditor::Apply(XcfgDocument *document,const Preview &preview,QString *error)
{
    XcfgIoTransaction ioTransaction(error,XcfgIoTransaction::Write);if(!ioTransaction.IsLocked())return false;

    if (!document) return Fail(error,"No configuration document");
    Preview current;
    if (!CreatePreview(*document,preview.request,&current,error)) return false;
    if (current.destinationFile!=preview.destinationFile || current.sourcePath!=preview.sourcePath ||
        current.originalBytes!=preview.originalBytes || current.updatedBytes!=preview.updatedBytes ||
        current.beforeSnapshot!=preview.beforeSnapshot || current.afterSnapshot!=preview.afterSnapshot || current.report!=preview.report || current.dependencyBytes!=preview.dependencyBytes || current.createdPaths!=preview.createdPaths || current.localizedPaths!=preview.localizedPaths)
        return Fail(error,"Structural preview differs; create and review a new preview");
    if (!document->CheckSources(error)) return false;
    for(auto it=current.dependencyBytes.constBegin();it!=current.dependencyBytes.constEnd();++it) {
        QFile dependency(it.key());
        if(QFileInfo(it.key()).canonicalFilePath()!=it.key() || !dependency.open(QIODevice::ReadOnly) || dependency.read(8*1024*1024+1)!=it.value())
            return Fail(error,"Include/import source changed before save: "+it.key());
    }
    QSaveFile file(current.destinationFile);
    if (!file.open(QIODevice::WriteOnly) || file.write(current.updatedBytes)!=current.updatedBytes.size() || !file.commit()) return Fail(error,file.errorString());
    const auto root=document->fRootFile;
    if (!document->Load(root,error)) {document->fNodes.clear();document->fSources.clear();*error="Structural edit saved, but reload failed: "+*error;return false;}
    QJsonObject actual;
    if (!EffectiveConfigurationSnapshot::Capture(*document,&actual,error) || actual!=current.afterSnapshot) {
        document->fNodes.clear();document->fSources.clear();
        *error="Structural edit saved, but reloaded configuration differs from the reviewed result; reopen and inspect: "+*error;
        return false;
    }
    return true;
}

