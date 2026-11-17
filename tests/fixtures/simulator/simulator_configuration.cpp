#include "simulator.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

bool Simulator::PrepareConfigurationWorkspace(int cobos, const QJsonArray &nodes,
                                               QString *configPath, QString *error)
{
    const QString configs = QDir(fRoot).filePath("get-configs");
    QDir().mkpath(configs + "/describe-cobo");
    for (const QString &prefix : {QString("prepare-"), QString("describe-"), QString("configure-")}) {
        QFile file(configs + "/" + prefix + "Demo.xcfg");
        if (!file.open(QIODevice::WriteOnly)) { *error = file.errorString(); return false; }
        file.write("<!-- SIMULATOR ONLY: not a hardware GET configuration -->\n<Setup/>\n");
    }
    for (int id = 0; id < cobos; ++id) {
        QFile file(configs + QString("/describe-cobo%1.xcfg").arg(id));
        if (!file.open(QIODevice::WriteOnly)) { *error = file.errorString(); return false; }
        file.write("<!-- SIMULATOR ONLY -->\n<Setup/>\n");
    }

    QJsonObject settings;
    settings["simulation"] = false; // Connect through the real HTTP client to local simulator services.
    settings["config_dir"] = configs;
    settings["output_dir"] = QDir(fRoot).filePath("records");
    settings["poll_ms"] = 500;
    settings["timeout_ms"] = 10000;
    settings["catalog_timeout_ms"] = 120000;
    settings["step_timeout_ms"] = 120000;
    settings["settle_ms"] = 100;
    settings["nodes"] = nodes;

    QJsonObject config;
    config["experiment"] = "Demo";
    config["run_number"] = 0;
    config["description"] = "Loopback HTTP simulator; generated .graw files are NOT real GET frames";
    config["fields"] = QJsonObject();
    config["getdaq"] = settings;
    *configPath = QDir(fRoot).filePath("envoy.yml");

    // JSON is a YAML 1.2 subset, avoiding a YAML library dependency in the simulator.
    // Preserve an existing file, including its Run number.
    if (!QFileInfo::exists(*configPath)) {
        QSaveFile file(*configPath);
        const QByteArray bytes = QJsonDocument(config).toJson();
        if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
            *error = file.errorString();
            return false;
        }
    }
    return true;
}
