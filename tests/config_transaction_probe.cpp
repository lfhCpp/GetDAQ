#include "xcfgdocument.h"
#include "xcfgworkspace.h"
#include "xcfgsavejournal.h"
#include "xcfgtransaction.h"
#include <QCoreApplication>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QThread>
#include <QTemporaryDir>
#include <iostream>
#include <stdexcept>
namespace {
void Require(bool okay,const QString &why){if(!okay)throw std::runtime_error(why.toStdString());}
void Write(const QString &path,const QByteArray &bytes){QFile f(path);Require(f.open(QIODevice::WriteOnly|QIODevice::NewOnly),f.errorString());Require(f.write(bytes)==bytes.size() && f.flush(),"Probe metadata write");}
void Ready(const QString &gate,const QString &record={}) {
    QJsonObject o;o["pid"]=QString::number(QCoreApplication::applicationPid());o["cache"]=QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);o["recovery"]=record;
    Write(gate+".ready",QJsonDocument(o).toJson(QJsonDocument::Compact));
}
void WaitGate(const QString &gate){QElapsedTimer time;time.start();while(QFileInfo::exists(gate)){Require(time.elapsed()<60000,"Owned gate deadline");QThread::msleep(5);}}
}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);
    try {
        Require(argc>=3,"Pass mode root [gate/record]");const auto mode=QString::fromLocal8Bit(argv[1]);app.setApplicationName(mode);
        if(mode=="temp") {QTemporaryDir temp(QDir::tempPath()+"/getdaq-config-transactions-XXXXXX");Require(temp.isValid(),"Create native test source directory");temp.setAutoRemove(false);std::cout<<temp.path().toLocal8Bit().constData()<<'\n';return 0;}
        const auto directory=QFileInfo(QString::fromLocal8Bit(argv[2])).canonicalFilePath();Require(!directory.isEmpty(),"Missing source fixture directory");
        const auto root=directory+"/root.xcfg",a=directory+"/a.xcfg",b=directory+"/b.xcfg";QString error;
        if(mode=="recover") {Require(argc==4,"Recovery directory needed");XcfgSaveJournal::Recovery p;Require(XcfgSaveJournal::PreviewRecovery(QString::fromLocal8Bit(argv[3]),true,&p,&error),error);Require(XcfgSaveJournal::ApplyRecovery(p,&error),error);std::cout<<"Recovered original candidate without replay\n";return 0;}
        if(mode=="read") {
            XcfgDocument d;if(!d.Load(root,&error)){std::cerr<<error.toStdString()<<'\n';return 3;}
            const auto left=d.Lookup("Setup/A/n",&error),right=d.Lookup("Setup/B/n",&error);Require(left>=0 && right>=0,error);
            QJsonObject o;o["a"]=d.Nodes()[left].value;o["b"]=d.Nodes()[right].value;std::cout<<QJsonDocument(o).toJson(QJsonDocument::Compact).constData()<<'\n';return 0;
        }
        Require(argc==4,"Gate argument needed");const auto gate=QString::fromLocal8Bit(argv[3]);
        if(mode=="hold-read" || mode=="crash-read") {
            XcfgIoTransaction access(&error);Require(access.IsLocked(),error);XcfgDocument d;Require(d.Load(root,&error),error);Ready(gate);if(mode=="crash-read")std::_Exit(78);WaitGate(gate);return 0;
        }
        if(mode=="preloaded-write") {
            XcfgDocument d;Require(d.Load(root,&error),error);const auto node=d.Lookup("Setup/A/n",&error);Require(node>=0,error);Ready(gate);WaitGate(gate);
            if(!d.SetValue(node,"3",&error)){std::cerr<<error.toStdString()<<'\n';return 4;}return 0;
        }
        Require(mode=="hold-write" || mode=="crash-write","Unknown transaction probe mode");
        XcfgWorkspace workspace;Require(workspace.Open(root,&error),error);
        QMap<QString,QByteArray> changes;changes[a]="<A><n type=\"int\">2</n></A>";changes[b]="<B><n type=\"int\">2</n></B>";
        Require(workspace.StageSources(changes,&error),error);XcfgWorkspace::Preview review;Require(workspace.CreatePreview(&review,&error),error);XcfgWorkspace::SaveResult result;int synced=0;
        Require(workspace.SaveAll(review,&result,&error,[&](const QString &,const QString &record){if(++synced==1){Ready(gate,record);if(mode=="crash-write")std::_Exit(77);WaitGate(gate);}}),error);
        Require(result.complete && synced==2,"Transaction did not save both sources");return 0;
    }catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
