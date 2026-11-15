#include "xcfgworkspace.h"
#include "xcfgsavejournal.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QDir>
#include <cstdlib>
#include <iostream>
int main(int argc,char **argv){QCoreApplication app(argc,argv);if(argc!=3)return 2;const QString action=QString::fromLocal8Bit(argv[1]),directory=QString::fromLocal8Bit(argv[2]);QString error;
    if(action=="crash") {
        if(!QDir().mkdir(directory))return 3;const auto a=directory+"/a.xcfg",b=directory+"/b.xcfg";const QByteArray original="<Setup><n type=\"int\">2</n></Setup>",changed="<Setup><n type=\"int\">3</n></Setup>";
        for(const auto &path:{a,b}){QFile file(path);if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly) || file.write(original)!=original.size())return 4;}
        XcfgWorkspace workspace;if(!workspace.Open(a,&error) || !workspace.Open(b,&error))return 5;QMap<QString,QByteArray> edits;edits[a]=changed;edits[b]=changed;XcfgWorkspace::Preview preview;XcfgWorkspace::SaveResult result;
        if(!workspace.StageSources(edits,&error) || !workspace.CreatePreview(&preview,&error))return 6;
        workspace.SaveAll(preview,&result,&error,[&](const QString &,const QString &record){std::cout<<record.toStdString()<<std::endl;std::_Exit(77);});return 7;
    }
    if(action=="recover") {
        XcfgSaveJournal::Recovery preview;if(!XcfgSaveJournal::PreviewRecovery(directory,true,&preview,&error)){std::cerr<<error.toStdString()<<'\n';return 8;}
        const auto report=QJsonDocument(preview.report).toJson();std::cout<<report.constData();if(!XcfgSaveJournal::ApplyRecovery(preview,&error)){std::cerr<<error.toStdString()<<'\n';return 9;}std::cout<<"Recovered actual interrupted writer; no replay or hardware command\n";return 0;
    }
    return 10;
}
