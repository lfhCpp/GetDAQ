#include "xcfgsourcemigration.h"
#include "xcfgsourcemigration_p.h"
#include "xcfgtransaction.h"
#include <stdexcept>

using namespace XcfgSourceMigrationInternal;

bool XcfgSourceMigration::Apply(XcfgWorkspace *workspace,const Preview &preview,Result *result,QString *error,const Progress &progress) {
    if(result)*result=Result();
    XcfgIoTransaction transaction(error,XcfgIoTransaction::Write);if(!transaction.IsLocked())return false;
    try {
        Require(workspace && result,"Missing migration workspace/result");*result=Result();
        Preview current;if(!CreatePreview(*workspace,preview.request,&current,error))return false;
        Require(current.revision==preview.revision && current.originalRoots==preview.originalRoots && current.migratedRoots==preview.migratedRoots &&
                current.original==preview.original && current.candidate==preview.candidate && current.report==preview.report,"Path migration review changed; inspect a new preview");
        // Once a record is being created, old in-memory history cannot be reused.
        workspace->fRequiresReopen=true;
        ApplyReviewedMigration(current,&result->recoveryDirectory,progress);
        result->complete=true;result->roots=current.migratedRoots;error->clear();return true;
    }catch(const std::exception &e){*error=QString::fromUtf8(e.what());return false;}
}
