#include "scanplancommands.h"
#include "configtool_registercommands_p.h"
#include "systemexperimentcompiler.h"
#include "effectiveconfigurationsnapshot.h"
#include "systemconfigurationpublisher.h"
#include "xcfgexporter.h"
#include "xcfgbinary.h"
#include "xcfgstructureeditor.h"
#include "xcfgworkspacecopy.h"
#include "xcfgsourcemigration.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QTextStream>
#include <stdexcept>



int main(int argc, char **argv)
{
    QCoreApplication application(argc, argv);
    const auto args = application.arguments();
    const int scanPlanResult = ConfigToolScanPlanCommands::Handle(args);
    if (scanPlanResult >= 0) return scanPlanResult;
    if(args.size()==4 && args[1]=="discover-project-referrers") {
        QJsonObject report;QString error;
        if(!XcfgSourceMigration::DiscoverReferrers(args[2],args[3],&report,&error)){QTextStream(stderr)<<error<<'\n';return 1;}
        QTextStream(stdout)<<QJsonDocument(report).toJson(QJsonDocument::Indented);
        return report.value("complete").toBool() ? 0 : 3;
    }
    if(args.size()>=5 && (args[1]=="preview-path-migration" || args[1]=="migrate-path")) {
        XcfgWorkspace workspace;QString error;
        for(int i=4;i<args.size();++i)if(!workspace.Open(args[i],&error)){QTextStream(stderr)<<error<<'\n';return 1;}
        XcfgSourceMigration::Request request;request.destinations[args[2]]=args[3];XcfgSourceMigration::Preview preview;
        if(!XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error)){QTextStream(stderr)<<error<<'\n';return 1;}
        if(args[1]=="preview-path-migration"){QTextStream(stdout)<<QJsonDocument(preview.report).toJson(QJsonDocument::Indented);return 0;}
        XcfgSourceMigration::Result result;
        if(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error)){QTextStream(stderr)<<error<<"\nRecovery record: "<<result.recoveryDirectory<<'\n';return 1;}
        QTextStream(stdout)<<QJsonDocument(QJsonObject{{"complete",result.complete},{"recovery_directory",result.recoveryDirectory},
            {"roots",QJsonArray::fromStringList(result.roots)},{"hardware_loaded_verified",false}}).toJson();return 0;
    }
    if(args.size()==4 && (args[1]=="preview-path-recovery" || args[1]=="recover-path")) {
        if(args[3]!="candidate" && args[3]!="original"){QTextStream(stderr)<<"Recovery direction must be candidate or original\n";return 2;}
        XcfgSourceMigration::Recovery preview;QString error;
        if(!XcfgSourceMigration::PreviewRecovery(args[2],args[3]=="candidate",&preview,&error) ||
            (args[1]=="recover-path" && !XcfgSourceMigration::ApplyRecovery(preview,&error))){QTextStream(stderr)<<error<<'\n';return 1;}
        QJsonObject result=preview.report;result["disk_written"]=args[1]=="recover-path";
        QTextStream(stdout)<<QJsonDocument(result).toJson();return 0;
    }


    if(args.size()==4 && args[1]=="import-binary") {
        XcfgBinary::Preview preview;QString error;
        if(!XcfgBinary::PreviewImport(args[2],args[3],&preview,&error) || !XcfgBinary::Save(preview,&error)) {QTextStream(stderr)<<error<<'\n';return 1;}
        QTextStream(stdout)<<"Saved editable XML: "<<preview.destination<<"\nObjects: "<<preview.objects<<"; scalars: "<<preview.scalars<<"\n";return 0;
    }
    if(args.size()==6 && args[1]=="export-binary") {
        XcfgExporter::Request request;request.path=args[3];request.destination=args[5];
        if(args[4]=="declarations")request.mode=XcfgExporter::FlattenDeclarations;
        else if(args[4]=="effective")request.mode=XcfgExporter::EffectiveConcreteObject;
        else {QTextStream(stderr)<<"Binary export mode must be declarations or effective\n";return 2;}
        XcfgDocument document;XcfgBinary::Preview preview;QString error;
        if(!document.Load(args[2],&error) || !XcfgBinary::PreviewExport(document,request,&preview,&error) || !XcfgBinary::Save(preview,&error)) {QTextStream(stderr)<<error<<'\n';return 1;}
        QTextStream(stdout)<<"Saved legacy version-1 binary: "<<preview.destination<<"\nNo hardware commands were sent.\n";return 0;
    }
    if(args.size()==5 && (args[1]=="preview-workspace-copy" || args[1]=="copy-workspace")) {
        XcfgWorkspaceCopy::Request request;request.directory=args[4];
        if(args[3]=="shared")request.sharing=XcfgWorkspaceCopy::PreserveSharedFiles;
        else if(args[3]=="isolated")request.sharing=XcfgWorkspaceCopy::IsolateEachReference;
        else{QTextStream(stderr)<<"Workspace mode must be shared or isolated\n";return 2;}
        XcfgDocument document;XcfgWorkspaceCopy::Preview preview;QJsonObject result;QString error;
        if(!document.Load(args[2],&error) || !XcfgWorkspaceCopy::CreatePreview(document,request,&preview,&error) ||
            (args[1]=="copy-workspace" && !XcfgWorkspaceCopy::Save(document,preview,&result,&error))) {QTextStream(stderr)<<error<<'\n';return 1;}
        if(args[1]=="preview-workspace-copy")result=QJsonObject{{"saved",false},{"directory",preview.request.directory},{"initial_inventory",preview.inventory}};
        QTextStream(stdout)<<QJsonDocument(result).toJson(QJsonDocument::Indented);return 0;
    }
    if(args.size()==4 && (args[1]=="preview-object-override" || args[1]=="override-object")) {
        XcfgDocument document;XcfgStructureEditor::Request request;request.operation=XcfgStructureEditor::OverrideInheritedObject;request.path=args[3];
        XcfgStructureEditor::Preview preview;QString error;
        if(!document.Load(args[2],&error) || !XcfgStructureEditor::CreatePreview(document,request,&preview,&error)) {QTextStream(stderr)<<error<<'\n';return 1;}
        const bool apply=args[1]=="override-object";
        if(apply && !XcfgStructureEditor::Apply(&document,preview,&error)){QTextStream(stderr)<<error<<'\n';return 1;}
        QJsonObject result;result["saved"]=apply;result["path"]=preview.request.path;result["destination_file"]=preview.destinationFile;
        result["created_paths"]=QJsonArray::fromStringList(preview.createdPaths);result["updated_xml"]=QString::fromUtf8(preview.updatedBytes);result["review"]=preview.report;
        result["localized_paths"]=QJsonArray::fromStringList(preview.localizedPaths);
        QTextStream(stdout)<<QJsonDocument(result).toJson(QJsonDocument::Indented);return 0;
    }
    if(args.size()==6 && args[1]=="export-xml") {
        XcfgExporter::Request request;request.path=args[3];request.destination=args[5];
        if(args[4]=="declarations")request.mode=XcfgExporter::FlattenDeclarations;
        else if(args[4]=="effective")request.mode=XcfgExporter::EffectiveConcreteObject;
        else {QTextStream(stderr)<<"XML export mode must be declarations or effective\n";return 2;}
        XcfgDocument document;XcfgExporter::Preview preview;QString error;
        if(!document.Load(args[2],&error) || !XcfgExporter::CreatePreview(document,request,&preview,&error) || !XcfgExporter::Save(document,preview,&error)) {
            QTextStream(stderr)<<error<<'\n';return 1;
        }
        QTextStream(stdout)<<"Saved self-contained XML: "<<preview.request.destination<<"\nEffective SHA-256: "<<preview.effectiveSha256
                           <<"\nObjects: "<<preview.objectCount<<"; scalars: "<<preview.scalarCount<<"\nNo hardware commands were sent.\n";return 0;
    }
    if (args.size()==5 && args[1]=="publish-system") {
        XcfgDocument document; QJsonObject result; QString error;
        if (!document.Load(args[2],&error) || !SystemConfigurationPublisher::Publish(document,args[3],args[4],&result,&error)) { QTextStream(stderr)<<error<<'\n'; return 1; }
        QTextStream(stdout)<<QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented)); return 0;
    }
    if (args.size()==4 && args[1]=="snapshot") {
        XcfgDocument document; QJsonObject snapshot; QString error;
        if (!document.Load(args[2],&error) || !EffectiveConfigurationSnapshot::Capture(document,&snapshot,&error)
            || !EffectiveConfigurationSnapshot::Save(snapshot,args[3],&error)) { QTextStream(stderr)<<error<<'\n'; return 1; }
        QTextStream(stdout)<<"Saved local effective configuration snapshot: "<<args[3]<<"\nEffective SHA-256: "<<snapshot.value("effective_sha256").toString()<<'\n'; return 0;
    }
    if (args.size()==5 && args[1]=="diff-snapshot") {
        QJsonObject before,after,report; QString error;
        if (!EffectiveConfigurationSnapshot::Load(args[2],&before,&error) || !EffectiveConfigurationSnapshot::Load(args[3],&after,&error)
            || !EffectiveConfigurationSnapshot::Compare(before,after,&report,&error) || !EffectiveConfigurationSnapshot::SaveReport(report,args[4],&error)) {
            QTextStream(stderr)<<error<<'\n'; return 1;
        }
        QTextStream(stdout)<<"Saved configuration review: "<<args[4]<<"\nEffective parameters changed: "<<(report.value("effective_changed").toBool() ? "yes" : "no")<<'\n'; return 0;
    }
    if (args.size()==5 && args[1]=="compile-system") {
        XcfgDocument document; QString error;
        if (!document.Load(args[2],&error) || !SystemExperimentCompiler::Save(document,args[3],args[4],&error)) { QTextStream(stderr)<<error<<'\n'; return 1; }
        QTextStream(stdout)<<"Compiled complete offline experiment settings: "<<args[4]<<'\n'; return 0;
    }




    return ConfigToolRegisterCommands::Handle(args);
}
