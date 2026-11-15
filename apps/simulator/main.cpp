#include "simulator.h"
#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv); app.setApplicationName("getdaq-simulator"); app.setApplicationVersion("0.1.0");
    QCommandLineParser p; p.addHelpOption(); p.addVersionOption();
    p.addOption({"root", "Dedicated simulator data/configuration directory.", "path", "getdaq-demo"});
    p.addOption({"cobos", "Number of CoBos (1–32).", "count", "2"});
    p.addOption({"ecc-port", "First ECC port; following nodes use successive ports. 0 selects a free port per server.", "port", "18083"});
    p.addOption({"sentry-port", "First sentry port. 0 selects a free port per server.", "port", "19080"});
    p.addOption({"fail-start", "Inject Start failure on this node ID (-1 disables).", "id", "-1"});
    p.addOption({"delay-ms", "Delay state transitions after command acknowledgement.", "ms", "100"});
    p.addOption({"max-generated-bytes", "Maximum total simulator .graw bytes (default 1 GiB; hard ceiling 20 GiB).", "bytes", "1073741824"});
    p.process(app);
    const QStringList keys = {"cobos", "ecc-port", "sentry-port", "fail-start", "delay-ms"};
    QMap<QString, int> values;
    for (const QString &key : keys) { bool ok; values[key] = p.value(key).toInt(&ok); if (!ok) { qCritical() << "Invalid integer:" << key; return 2; } }
    bool budgetOk = false; const quint64 maxGeneratedBytes = p.value("max-generated-bytes").toULongLong(&budgetOk);
    if (!budgetOk || maxGeneratedBytes < 64ULL * 1024ULL || maxGeneratedBytes > 20ULL * 1024ULL * 1024ULL * 1024ULL) {
        qCritical() << "--max-generated-bytes must be between 65536 and 21474836480."; return 2;
    }
    Simulator simulator; QString error;
    if (!simulator.Start(p.value("root"), values["cobos"], values["ecc-port"], values["sentry-port"], values["fail-start"], values["delay-ms"], maxGeneratedBytes, &error)) {
        qCritical().noquote() << error; return 1;
    }
    return app.exec();
}
