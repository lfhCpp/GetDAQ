#include "repositoryservice.h"
#include <stdexcept>
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include <QTimer>
#include <csignal>
namespace {volatile std::sig_atomic_t stopped=0;void Stop(int) {stopped=1;}}
int main(int argc,char **argv) {
    QCoreApplication app(argc,argv);QCommandLineParser parser;parser.addHelpOption();
    parser.addOption({"root","Existing configuration repository root.","path"});parser.addOption({"credential","Existing 0600 credential file.","path"});
    parser.addOption({"bind","Canonical IPv4 bind address.","address","127.0.0.1"});parser.addOption({"port","Control HTTP port; 0 chooses an available port.","number","24002"});
    parser.addOption({"generate-token","Generate a new 0600 credential; never display its value.","path"});
    parser.addOption({"read-credential","Optional separate read-only node credential.","path"});
    parser.addOption({"tls-certificate","PEM certificate for HTTPS.","path"});parser.addOption({"tls-key","PEM private key for HTTPS.","path"});parser.process(app);
    try {
        if (parser.isSet("generate-token")) {GetDAQ::RepositoryCredential::Generate(parser.value("generate-token").toStdString());qInfo()<<"Repository credential created; value not displayed.";return 0;}
        bool ok=false;const auto port=parser.value("port").toUInt(&ok);if (!ok || port>65535 || !parser.isSet("root") || !parser.isSet("credential")) throw std::invalid_argument("Supply root, credential and valid port");
        std::signal(SIGPIPE,SIG_IGN);std::signal(SIGINT,Stop);std::signal(SIGTERM,Stop);
        RepositoryService service(parser.value("root").toStdString(),parser.value("credential").toStdString(),nullptr,parser.value("read-credential").toStdString());
        if(parser.isSet("tls-certificate")!=parser.isSet("tls-key"))throw std::invalid_argument("Supply both TLS certificate and key");
        if(parser.isSet("tls-certificate"))service.SetTls(parser.value("tls-certificate"),parser.value("tls-key"));
        service.Start(parser.value("bind"),quint16(port));qInfo()<<"Configuration repository listening; port"<<service.Port()<<"; no raw DAQ or hardware actions";
        QTimer timer;QObject::connect(&timer,&QTimer::timeout,&app,[&] {if (stopped) app.quit();});timer.start(100);return app.exec();
    } catch (const std::exception &e) {qCritical().noquote()<<e.what();return 1;}
}
