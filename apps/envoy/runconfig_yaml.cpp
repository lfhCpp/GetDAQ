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
            if (settings["native_catalog"]) c.nativeCatalog=settings["native_catalog"].as<bool>();
            if (settings["native_alarm_status"]) c.nativeClient.alarmStatusEnabled=settings["native_alarm_status"].as<bool>();
            if (settings["native_configuration_status"]) c.nativeClient.configurationStatusEnabled=settings["native_configuration_status"].as<bool>();
            if (settings["native_receiver_status"]) c.nativeClient.receiverStatusEnabled=settings["native_receiver_status"].as<bool>();
            if(settings["native_source_status"])c.nativeClient.sourceStatusEnabled=settings["native_source_status"].as<bool>();
            if(settings["native_receiver_failure_status"])c.nativeClient.receiverFailureStatusEnabled=settings["native_receiver_failure_status"].as<bool>();
            if (settings["simulation"]) c.simulation = settings["simulation"].as<bool>();
            if (settings["ecc_name"]) c.eccName=Text(settings["ecc_name"]);
            if (settings["native_ecc"]) {
                const auto native=settings["native_ecc"];
                if (!native.IsMap() || native.size()!=11) throw std::runtime_error("native_ecc requires exactly eleven fixed client fields.");
                const char *keys[]={"address","port","credential_file","service_id","client_id","client_epoch","snapshot_sha256","input_binding_sha256","timeout_ms","status_max_age_ms","record_dir"};
                for (const char *key:keys) if (!native[key]) throw std::runtime_error("Missing native_ecc field.");
                c.nativeEcc=true; c.nativeClient.address=Text(native["address"]); c.nativeClient.port=native["port"].as<int>();
                c.nativeClient.credentialFile=Resolve(base,Text(native["credential_file"])); c.nativeClient.serviceId=Text(native["service_id"]); c.nativeClient.clientId=Text(native["client_id"]);
                c.nativeClient.clientEpoch=Text(native["client_epoch"]); c.nativeClient.snapshotSha256=Text(native["snapshot_sha256"]); c.nativeClient.inputBindingSha256=Text(native["input_binding_sha256"]);
                c.nativeClient.timeoutMs=native["timeout_ms"].as<int>(); c.nativeClient.statusMaxAgeMs=native["status_max_age_ms"].as<int>(); c.nativeRecordDir=Resolve(base,Text(native["record_dir"]));
            }
            if (settings["config_dir"]) c.configDir = Resolve(base, Text(settings["config_dir"]));
            if (settings["output_dir"]) c.outputDir = Resolve(base, Text(settings["output_dir"]));
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
                    const QString role = Text(item["role"]);
                    if (role != "mutant" && role != "cobo") throw std::runtime_error("role must be mutant or cobo.");
                    n.mutant = role == "mutant";
                    n.host = item["host"] ? Text(item["host"]) : QString();
                    n.name = item["name"] ? Text(item["name"]) : QString("%1 %2").arg(role).arg(n.id);
                    if (item["ecc_port"]) n.eccPort = item["ecc_port"].as<int>();
                    if (item["sentry_port"]) n.sentryPort = item["sentry_port"].as<int>();
                    if (item["router_port"]) n.routerPort = item["router_port"].as<int>();
                    if (item["exporter_port"]) n.exporterPort = item["exporter_port"].as<int>();
                    if (item["router_host"]) n.routerHost = Text(item["router_host"]);
                    if (item["exporter_host"]) n.exporterHost = Text(item["exporter_host"]);
                    if (item["sentry_host"]) n.sentryHost = Text(item["sentry_host"]);
                    if (item["describe_id"]) n.describeId = Text(item["describe_id"]);
                    if (item["prepare_id"]) n.prepareId = Text(item["prepare_id"]);
                    if (item["configure_id"]) n.configureId = Text(item["configure_id"]);
                    if (item["soap_namespace"]) n.soapNamespace = Text(item["soap_namespace"]);
                    if (item["soap_inline_xml"]) n.soapInlineXml = item["soap_inline_xml"].as<bool>();
                    if (item["native_sender_id"]) n.nativeSenderId=Text(item["native_sender_id"]);
                    if (item["native_agent_id"]) n.nativeAgentId=Text(item["native_agent_id"]);
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
    g["simulation"] = simulation;
    auto automation=g["auto_run"];
    automation["enabled"]=autoRun.enabled;
    automation["condition"]=autoRun.condition==AutoRunSettings::Time ? "time" : autoRun.condition==AutoRunSettings::FileSize ? "file_size" : autoRun.condition==AutoRunSettings::Either ? "either" : "none";
    automation["action"]=autoRun.action==AutoRunSettings::Stop ? "stop" : "next_run";
    automation["duration_ms"]=autoRun.durationMs;automation["file_bytes"]=autoRun.fileBytes;
    automation["maximum_runs"]=autoRun.maximumRuns;
    if (nativeEcc) {
        g["native_catalog"]=nativeCatalog;
        g["native_alarm_status"]=nativeClient.alarmStatusEnabled;
        g["native_configuration_status"]=nativeClient.configurationStatusEnabled;
        g["native_receiver_status"]=nativeClient.receiverStatusEnabled;
        g["native_source_status"]=nativeClient.sourceStatusEnabled;
        g["native_receiver_failure_status"]=nativeClient.receiverFailureStatusEnabled;
        auto n=g["native_ecc"]; n["address"]=Utf8(nativeClient.address); n["port"]=nativeClient.port; n["credential_file"]=Utf8(nativeClient.credentialFile);
        n["service_id"]=Utf8(nativeClient.serviceId); n["client_id"]=Utf8(nativeClient.clientId); n["client_epoch"]=Utf8(nativeClient.clientEpoch);
        n["snapshot_sha256"]=Utf8(nativeClient.snapshotSha256); n["input_binding_sha256"]=Utf8(nativeClient.inputBindingSha256); n["timeout_ms"]=nativeClient.timeoutMs;
        n["status_max_age_ms"]=nativeClient.statusMaxAgeMs; n["record_dir"]=Utf8(nativeRecordDir);
    }
    g["config_dir"] = Utf8(configDir);
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
        item["router_port"] = n.routerPort;
        item["exporter_port"] = n.exporterPort;
        item["router_host"] = Utf8(n.routerHost);
        item["exporter_host"] = Utf8(n.exporterHost);
        item["sentry_host"] = Utf8(n.sentryHost);
        item["describe_id"] = Utf8(n.describeId);
        item["prepare_id"] = Utf8(n.prepareId);
        item["configure_id"] = Utf8(n.configureId);
        item["soap_namespace"] = Utf8(n.soapNamespace);
        item["soap_inline_xml"] = n.soapInlineXml;
        if (nativeEcc) { item["native_sender_id"]=Utf8(n.nativeSenderId); item["native_agent_id"]=Utf8(n.nativeAgentId); }
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
