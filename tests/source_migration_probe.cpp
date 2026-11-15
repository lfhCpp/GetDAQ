#include "xcfgsourcemigration.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>
#include <QElapsedTimer>
#include <cstdlib>
#include <stdexcept>

int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);const auto args=app.arguments();
    if(args.size()<3)return 2;const auto mode=args[1];app.setApplicationName("source-migration-"+mode);QString error;
    if(mode=="temp") {
        QTemporaryDir temporary(QDir::tempPath()+"/getdaq-source-migration-XXXXXX");if(!temporary.isValid())return 2;
        temporary.setAutoRemove(false);QTextStream(stdout)<<temporary.path()<<'\n';return 0;
    }
    if(mode=="read") {
        XcfgDocument document;if(!document.Load(args[2],&error)){QTextStream(stderr)<<error<<'\n';return 3;}
        QTextStream(stdout)<<"Loaded complete reviewed configuration\n";return 0;
    }
    if(mode=="recover" && args.size()==4) {
        XcfgSourceMigration::Recovery preview;
        if(!XcfgSourceMigration::PreviewRecovery(args[2],args[3]=="candidate",&preview,&error) ||
           !XcfgSourceMigration::ApplyRecovery(preview,&error)){QTextStream(stderr)<<error<<'\n';return 4;}
        QTextStream(stdout)<<"Recovered recorded "<<args[3]<<" paths\n";return 0;
    }
    if((mode=="crash" || mode=="hold") && args.size()==5) {
        const auto directory=QFileInfo(args[2]).canonicalFilePath();
        const auto root=QDir(directory).filePath("root.xcfg"),part=QDir(directory).filePath("part.xcfg");
        XcfgWorkspace workspace;if(!workspace.Open(root,&error) || !workspace.Open(QDir(directory).filePath("wrapper.xcfg"),&error))return 5;
        XcfgSourceMigration::Request request;
        request.destinations[args[3]=="root" ? root : part]=QDir(directory).filePath(args[3]=="root" ? "new/main.xcfg" : "new/part.xcfg");
        XcfgSourceMigration::Preview preview;if(!XcfgSourceMigration::CreatePreview(workspace,request,&preview,&error)){QTextStream(stderr)<<error<<'\n';return 6;}
        XcfgSourceMigration::Result result;bool reached=false;
        const auto progress=[&](const QString &phase,const QString &path) {
            if(phase!=args[4] || reached)return;reached=true;
            const auto bytes=QJsonDocument(QJsonObject{{"phase",phase},{"path",path},{"recovery",result.recoveryDirectory},
                {"pid",QString::number(QCoreApplication::applicationPid())}}).toJson(QJsonDocument::Compact);
            QSaveFile ready(QDir(directory).filePath("progress.json"));ready.setDirectWriteFallback(false);
            if(!ready.open(QIODevice::WriteOnly) || ready.write(bytes)!=bytes.size() || !ready.commit())std::_Exit(70);
            if(mode=="crash")std::_Exit(79);
            QElapsedTimer deadline;deadline.start();
            while(QFileInfo::exists(QDir(directory).filePath("hold.gate"))) {
                if(deadline.elapsed()>60000)throw std::runtime_error("Owned migration gate deadline");QThread::msleep(10);
            }
        };
        if(!XcfgSourceMigration::Apply(&workspace,preview,&result,&error,progress)){QTextStream(stderr)<<error<<'\n';return 7;}
        return reached && result.complete ? 0 : 8;
    }
    return 2;
}
