#include "servicelogpanel.h"
#include "deploymentcontroller.h"
#include <QApplication>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QTimer>
#include <QSaveFile>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSet>
QString StartupConfigurationKey(){return "log-panel-probe";}
namespace Deployment { Endpoint NodeEndpoint(const QJsonObject &node){return {node["url"].toString(),Digest(node["certificate_sha256"].toString()),Secret(node["credential_file"].toString())};} }
int main(int argc,char **argv){
 QApplication app(argc,argv);if(argc!=3)return 2;const auto project=Deployment::Parse(Deployment::Read(argv[1]));auto *events=new QPlainTextEdit;events->setPlainText("probe event before clear");ServiceLogPanel panel(events);panel.SetProject(argv[1]);
 // Wait for the initial per-stream boundaries, then append controlled new output.
 QTimer::singleShot(5000,&app,[&]{QSet<QString> files;for(const auto &host:project["nodes"].toArray())for(const auto &value:host.toObject()["inventory"].toObject()["instances"].toArray()){
  const auto instance=value.toObject();const auto service=Deployment::Parse(Deployment::Read(instance["config_file"].toString()));const auto role=instance["role"].toString();files.insert(service["log_directory"].toString()+'/'+role+".log");if(role=="receiver")files.insert(service["sentry_root"].toString()+"/sentry.log");}
 for(const auto &path:files){QFile file(path);if(file.open(QIODevice::Append))file.write("\nCURRENT-GUI-SESSION-OUTPUT\n");}
 });
 QTimer check;QObject::connect(&check,&QTimer::timeout,&app,[&]{const auto text=panel.CombinedText();
  if(!text.contains("CURRENT-GUI-SESSION-OUTPUT") || !text.contains("ECC LOG") || !text.contains("DATA LOG") || !text.contains("SENTRY LOG"))return;
  if(text.contains("Starting Electronics Control Core SOAP server")){app.exit(6);return;}
  panel.findChild<QToolButton *>("ClearLog")->click();if(!panel.VisibleText().isEmpty() || !panel.CombinedText().contains("probe event before clear")){app.exit(3);return;}
  QSaveFile output(argv[2]);const auto bytes=panel.CombinedText().toUtf8();if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()){app.exit(4);return;}app.exit(0);
 });check.start(100);QTimer::singleShot(18000,&app,[&]{app.exit(5);});return app.exec();
}
