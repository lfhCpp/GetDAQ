#include "servicelogpanel.h"
#include "deploymentcontroller.h"
#include <QApplication>
#include <QPlainTextEdit>
#include <QToolButton>
#include <QTimer>
#include <QSaveFile>
// Keep this tiny harness independent of service startup and all hardware code.
QString StartupConfigurationKey(){return "log-panel-probe";}
namespace Deployment { Endpoint NodeEndpoint(const QJsonObject &node){return {node["url"].toString(),Digest(node["certificate_sha256"].toString()),Secret(node["credential_file"].toString())};} }
int main(int argc,char **argv){
 QApplication app(argc,argv);if(argc!=3)return 2;
 auto *events=new QPlainTextEdit;events->setPlainText("probe event before clear");ServiceLogPanel panel(events);panel.SetProject(argv[1]);
 QTimer check;QObject::connect(&check,&QTimer::timeout,&app,[&]{
  const auto text=panel.CombinedText();
  if(!text.contains("Starting Electronics Control Core SOAP server") || !text.contains("Connection established") || !text.contains("SENTRY LOG"))return;
  // The default Event tab was never switched: other streams were collected.
  panel.findChild<QToolButton *>("ClearLog")->click();if(!panel.VisibleText().isEmpty() || !panel.CombinedText().contains("probe event before clear")){app.exit(3);return;}
  QSaveFile output(argv[2]);const auto bytes=panel.CombinedText().toUtf8();if(!output.open(QIODevice::WriteOnly) || output.write(bytes)!=bytes.size() || !output.commit()){app.exit(4);return;}app.exit(0);
 });check.start(100);QTimer::singleShot(15000,&app,[&]{app.exit(5);});return app.exec();
}
