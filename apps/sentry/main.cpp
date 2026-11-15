#include "sentryservice.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("getdaq-sentry"); app.setApplicationVersion("0.1.0");
    QCommandLineParser parser; parser.addHelpOption(); parser.addVersionOption();
    parser.setApplicationDescription("GET DAQ status and .graw catalog service (compatible /status and /catalog).");
    parser.addOption({{"c", "config"}, "JSON service configuration.", "path"});
    parser.process(app);
    if (!parser.isSet("config")) { qCritical() << "Supply --config config/sentry/node.json"; return 2; }
    SentryConfig config; QString error;
    if (!SentryConfig::Load(parser.value("config"), &config, &error)) { qCritical().noquote() << error; return 2; }
    SentryService service(config);
    if (!service.Start(&error)) { qCritical().noquote() << error; return 1; }
    qInfo().noquote() << QString("getdaq-sentry listening on %1:%2, data=%3").arg(config.bindAddress).arg(service.Port()).arg(config.dataDir);
    return app.exec();
}
