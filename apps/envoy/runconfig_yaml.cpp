#include "runconfig.h"
#include <stdexcept>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <yaml-cpp/yaml.h>

namespace {
QString Text(const YAML::Node &node) { return QString::fromStdString(node.as<std::string>()); }
std::string Utf8(const QString &text) { return text.toUtf8().toStdString(); }
QString Resolve(const QString &base, const QString &path)
{ return QDir::isAbsolutePath(path) ? QDir::cleanPath(path) : QDir(base).absoluteFilePath(path); }
}

bool RunConfig::Load(const QString &path, QString *error)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) { *error = file.errorString(); return false; }
    if (file.size() > 4 * 1024 * 1024) { *error = "Configuration exceeds 4 MiB."; return false; }
    try {
        const YAML::Node root = YAML::Load(file.readAll().toStdString());
        if (!root.IsMap()) throw std::runtime_error("YAML root must be a mapping.");
        RunConfig c = Default();
        c.experiment = Text(root["experiment"]);
        c.runNumber = root["run_number"].as<int>();
        c.description = root["description"] ? Text(root["description"]) : QString();
        if (root["fields"]) {
            if (!root["fields"].IsMap()) throw std::runtime_error("fields must be a mapping.");
            c.fields.clear();
            for (const auto &field : root["fields"]) c.fields.insert(Text(field.first), Text(field.second));
        }
        const YAML::Node settings = root["getdaq"];
        const QString base = QFileInfo(path).absolutePath();
        if (settings) {
            if (!settings.IsMap()) throw std::runtime_error("getdaq must be a mapping.");
            if(settings["config_dir"])c.configDir=Resolve(base,Text(settings["config_dir"]));
            if(settings["output_dir"])c.outputDir=Resolve(base,Text(settings["output_dir"]));
            if(settings["configuration_backup_warning"])c.configurationBackupWarning=Text(settings["configuration_backup_warning"]);
            if(settings["configuration_snapshot"])c.configurationSnapshot=Resolve(base,Text(settings["configuration_snapshot"]));
            if(settings["deployment_groups"]){if(!settings["deployment_groups"].IsSequence())throw std::runtime_error("deployment_groups must be a sequence");for(const auto &group:settings["deployment_groups"])c.deploymentGroups.append(Text(group));}
            if(settings["deployment_project"])c.deploymentProject=Resolve(base,Text(settings["deployment_project"]));
            if(settings["sync_configuration"])c.syncConfiguration=settings["sync_configuration"].as<bool>();
            if(settings["ecc_name"])c.eccName=Text(settings["ecc_name"]);
            if(settings["auto_run"]) {
                const auto a=settings["auto_run"];
                if(!a.IsMap())throw std::runtime_error("auto_run must be a mapping");
                c.autoRun.enabled=a["enabled"].as<bool>();
                const auto condition=Text(a["condition"]),action=Text(a["action"]);
                if(condition!="time" && condition!="file_size" && condition!="either" && condition!="none")throw std::runtime_error("auto_run condition must be time, file_size, either or none");
                if(action!="stop" && action!="next_run")throw std::runtime_error("auto_run action must be stop or next_run");
                c.autoRun.condition=condition=="time" ? AutoRunSettings::Time : condition=="file_size" ? AutoRunSettings::FileSize : condition=="either" ? AutoRunSettings::Either : AutoRunSettings::NoCondition;
                c.autoRun.action=action=="stop" ? AutoRunSettings::Stop : AutoRunSettings::NextRun;
                c.autoRun.durationMs=a["duration_ms"].as<qint64>();c.autoRun.fileBytes=a["file_bytes"].as<quint64>();
                c.autoRun.maximumRuns=a["maximum_runs"].as<int>();
            }
            if(settings["native_ecc"]){const auto old=settings["native_ecc"];if(old.IsMap() || old.as<bool>())throw std::runtime_error("The rewritten native ECC backend has been removed. Select an original SOAP ECC configuration.");}
            if (settings["poll_ms"]) c.pollMs = settings["poll_ms"].as<int>();
            if (settings["timeout_ms"]) c.timeoutMs = settings["timeout_ms"].as<int>();
            if (settings["catalog_timeout_ms"]) c.catalogTimeoutMs = settings["catalog_timeout_ms"].as<int>();
            if (settings["step_timeout_ms"]) c.stepTimeoutMs = settings["step_timeout_ms"].as<int>();
            if (settings["settle_ms"]) c.settleMs = settings["settle_ms"].as<int>();
            if (settings["nodes"]) {
                if (!settings["nodes"].IsSequence()) throw std::runtime_error("nodes must be a sequence.");
                c.nodes.clear();
                for (const auto &item : settings["nodes"]) {
                    NodeConfig n;
                    n.id = item["id"].as<int>();
                    if(item["board_index"])n.boardIndex=item["board_index"].as<int>();
                    const QString role = Text(item["role"]);
                    if (role != "mutant" && role != "cobo") throw std::runtime_error("role must be mutant or cobo.");
                    n.mutant = role == "mutant";
                    n.host = item["host"] ? Text(item["host"]) : QString();
                    n.name = item["name"] ? Text(item["name"]) : QString("%1 %2").arg(role).arg(n.id);
                    if (item["ecc_port"]) n.eccPort = item["ecc_port"].as<int>();
                    if (item["sentry_port"]) n.sentryPort = item["sentry_port"].as<int>();
                    if (item["router_port"]) n.routerPort = item["router_port"].as<int>();
                    if(item["preview_port"])n.previewPort=item["preview_port"].as<int>();
                    if(item["preview_host"])n.previewHost=Text(item["preview_host"]);
                    if (item["exporter_port"]) n.exporterPort = item["exporter_port"].as<int>();
                    if (item["router_host"]) n.routerHost = Text(item["router_host"]);
                    if (item["exporter_host"]) n.exporterHost = Text(item["exporter_host"]);
                    if (item["sentry_host"]) n.sentryHost = Text(item["sentry_host"]);
                    if (item["describe_id"]) n.describeId = Text(item["describe_id"]);
                    if (item["prepare_id"]) n.prepareId = Text(item["prepare_id"]);
                    if (item["configure_id"]) n.configureId = Text(item["configure_id"]);
                    if (item["soap_namespace"]) n.soapNamespace = Text(item["soap_namespace"]);
                    if (item["soap_inline_xml"]) n.soapInlineXml = item["soap_inline_xml"].as<bool>();
                    c.nodes.append(n);
                }
            }
        }
        c.filePath = QFileInfo(path).absoluteFilePath();
        if (!c.Validate(error)) return false;
        *this = c;
        return true;
    } catch (const std::exception &e) { *error = QString::fromUtf8(e.what()); return false; }
}

bool RunConfig::Save(const QString &path, QString *error) const
{
    if (!Validate(error)) return false;
    YAML::Node root;
    root["experiment"] = Utf8(experiment);
    root["run_number"] = runNumber;
    root["description"] = Utf8(description);
    root["fields"] = YAML::Node(YAML::NodeType::Map);
    for (auto it = fields.constBegin(); it != fields.constEnd(); ++it) root["fields"][Utf8(it.key())] = Utf8(it.value());
    YAML::Node g = root["getdaq"];
    if(!eccName.isEmpty())g["ecc_name"]=Utf8(eccName);
    auto automation=g["auto_run"];
    automation["enabled"]=autoRun.enabled;
    automation["condition"]=autoRun.condition==AutoRunSettings::Time ? "time" : autoRun.condition==AutoRunSettings::FileSize ? "file_size" : autoRun.condition==AutoRunSettings::Either ? "either" : "none";
    automation["action"]=autoRun.action==AutoRunSettings::Stop ? "stop" : "next_run";
    automation["duration_ms"]=autoRun.durationMs;automation["file_bytes"]=autoRun.fileBytes;
    automation["maximum_runs"]=autoRun.maximumRuns;

    g["config_dir"] = Utf8(configDir);
    g["sync_configuration"]=syncConfiguration;
    if(!configurationBackupWarning.isEmpty())g["configuration_backup_warning"]=Utf8(configurationBackupWarning);
    if(!configurationSnapshot.isEmpty())g["configuration_snapshot"]=Utf8(configurationSnapshot);
    if(!deploymentGroups.isEmpty()){for(const auto &group:deploymentGroups)g["deployment_groups"].push_back(Utf8(group));}
    if(!deploymentProject.isEmpty())g["deployment_project"]=Utf8(deploymentProject);
    g["output_dir"] = Utf8(outputDir);
    g["poll_ms"] = pollMs;
    g["timeout_ms"] = timeoutMs;
    g["catalog_timeout_ms"] = catalogTimeoutMs;
    g["step_timeout_ms"] = stepTimeoutMs;
    g["settle_ms"] = settleMs;
    for (const NodeConfig &n : nodes) {
        YAML::Node item;
        item["id"] = n.id;
        item["role"] = n.mutant ? "mutant" : "cobo";
        item["name"] = Utf8(n.name);
        item["host"] = Utf8(n.host);
        item["ecc_port"] = n.eccPort;
        item["sentry_port"] = n.sentryPort;
        if(n.boardIndex>=0)item["board_index"]=n.boardIndex;
        item["router_port"] = n.routerPort;
        item["exporter_port"] = n.exporterPort;
        item["preview_port"]=n.previewPort;item["preview_host"]=Utf8(n.previewHost);
        item["router_host"] = Utf8(n.routerHost);
        item["exporter_host"] = Utf8(n.exporterHost);
        item["sentry_host"] = Utf8(n.sentryHost);
        item["describe_id"] = Utf8(n.describeId);
        item["prepare_id"] = Utf8(n.prepareId);
        item["configure_id"] = Utf8(n.configureId);
        item["soap_namespace"] = Utf8(n.soapNamespace);
        item["soap_inline_xml"] = n.soapInlineXml;
        g["nodes"].push_back(item);
    }
    YAML::Emitter out;
    out.SetStringFormat(YAML::DoubleQuoted);
    out << root;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(out.c_str(), qint64(out.size())) != qint64(out.size()) || !file.commit()) {
        *error = file.errorString(); return false;
    }
    return true;
}
