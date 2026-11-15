#include "recoveryservice.h"
#include <stdexcept>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>
#include <csignal>
namespace {volatile std::sig_atomic_t interrupted=0;void Stop(int){interrupted=1;}}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);app.setApplicationName("getdaq-handoff-service");
    QCommandLineParser parser;parser.addHelpOption();parser.setApplicationDescription("Fixed cold DATA handoff recovery tasks; separate from the stopped ECC. No raw transport or old hardware takeover.");
    parser.addOption({"config","Private fixed recovery JSON configuration.","path"});parser.process(app);
    try {
        if(!parser.isSet("config"))throw std::invalid_argument("Supply --config with the fixed recovery policy");
        std::signal(SIGPIPE,SIG_IGN);std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);
        GetDAQ::RecoveryService service(parser.value("config").toStdString());service.Start();
        QTimer shutdown;QObject::connect(&shutdown,&QTimer::timeout,&app,[&]{if(interrupted && !service.Active())app.quit();});shutdown.start(100);
        qInfo()<<"getdaq-handoff-service active; port"<<service.Port()<<"; Qt forwards metadata only";return app.exec();
    }catch(const std::exception &e){qCritical().noquote()<<e.what();return 2;}
}
